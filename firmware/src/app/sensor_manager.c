#include "sensor_manager.h"
#include "../drivers/sensors/sensor_registry.h"
#include "board.h"
#include "../drivers/sensors/tca9548a/tca9548a.h"
#include "../drivers/bsp/bsp_delay.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "../drivers/bsp/bsp_uart.h"
#include "../drivers/bsp/log_manager.h"
#include "config_manager.h"
#include "algo_params.h"
#include "config_parser.h"
#include "plat.h"

extern void IWDG_Feed(void);

/* Active driver, looked up from the board's sensor manifest on first use.
 * Lazy so code paths that never ran Scan_Ports (e.g. replay tests) still see it. */
static const SensorDriver_t *s_drv;

static const SensorDriver_t *drv(void)
{
    if (!s_drv) s_drv = Sensor_FindDriver(BOARD_SENSOR_TYPE);
    return s_drv;
}

// FLAG_RES: 使用每一位代表不同的使能标志
// Bit 0: 标定使能
// Bit 1: 死区过滤使能
// Bit 2: 滑动窗口滤波使能
// Bit 3: 预测补偿使能
// Bit 4: 映射使能
// Bit 5: 平滑过渡使能
// Bit 6: Z轴正值过滤使能
// 初始值: 0x7B = 0111 1011 (标定、死区、预测补偿、映射、平滑过渡、Z轴正值过滤 开启，滑动窗口滤波 关闭)
uint8_t FLAG_RES = 0x7B;

uint8_t SENSOR_List[1] = {0};   // 传感器状态列表,0-无传感器,1-有传感器
uint8_t SENSOR_Run[1] = {0};    // 传感器运行状态

// 传感器上下文：可容纳任一已注册驱动的句柄（类型由运行期选中的驱动决定）
SensorCtx_t actual_sensors[1];     /* 实际传感器 */
SensorCtx_t reference_sensors[1];  /* 参考传感器 */

// 传感器数据存储 - 1对传感器的数据
SensorPair_Data sensor_data[1] = {0};

// 处理后的传感器数据存储 - 1对传感器的处理后数据
SensorPair_Data processed_sensor_data[1] = {0};
// 传感器标定值存储 - 1对传感器的标定值
SensorPair_Calibration sensor_calibration[1] = {0};

AlgoTiming_t sensor_timing;
static AlgoTiming_t s_output_timing;
static AlgoState_t s_algorithm;
static AlgoOutput_t s_output;
static AlgoParams_t s_previous_params;
static AlgoChannelCfg_t s_previous_channel;
static uint8_t s_previous_flags, s_initialized, s_format_v2, s_refout;
static AlgoTrace_t s_trace;
static uint16_t s_node_bypass;
static uint8_t s_trace_enabled,s_trace_ready,s_trace_clock;
static uint32_t s_trace_time;
void Sensor_SetTrace(uint8_t enabled) { s_trace_enabled=enabled; s_trace_ready=0; s_trace_clock=0; }
uint8_t Sensor_GetTrace(void) { return s_trace_enabled; }
uint16_t Sensor_GetNodeBypass(void) { return s_node_bypass; }
uint8_t Sensor_SetNode(uint8_t node,uint8_t enabled)
{
    static const uint8_t bits[10]={0,0,1,128,8,4,2,64,16,32};
    if(node==0 || node>=10 || (g_algo.config.profile==0 && (node==1 || node==3))) return 1;
    if(enabled) { s_node_bypass&=~(1U<<node); FLAG_RES|=bits[node]; }
    else s_node_bypass|=1U<<node;
    Sensor_ResetAlgorithm(); s_trace_ready=0;
    return 0;
}
uint8_t Sensor_TakeTrace(AlgoTrace_t *trace)
{
    if(!s_trace_ready) return 0;
    *trace=s_trace; s_trace_ready=0; return 1;
}
int Sensor_FormatTrace(const AlgoTrace_t *t,unsigned node,char *buf,size_t size)
{
    if(node>=10 || (t->valid && !(t->present&(1U<<node))) || (!t->valid && node!=0)) return 0;
    const float *a=t->actual[node],*r=t->reference[node];
    return snprintf(buf,size,"N seq=%lu t=%lu p=%u s=%u run=%u valid=%u err=%u x=%.7g y=%.7g z=%.7g rx=%.7g ry=%.7g rz=%.7g\r\n",
        (unsigned long)t->seq,(unsigned long)t->timestamp_us,t->profile,node,
        !!(t->executed&(1U<<node)),t->valid,t->error,
        t->valid?a[0]:0.0,t->valid?a[1]:0.0,t->valid?a[2]:0.0,
        t->valid?r[0]:0.0,t->valid?r[1]:0.0,t->valid?r[2]:0.0);
}
float X_MAPPING_VALUE=1.0f, Y_MAPPING_VALUE=1.0f, Z_MAPPING_VALUE=1.0f;

void Sensor_ResetAlgorithm(void)
{
    Algo_Reset(&s_algorithm);
    s_initialized=0;
    processed_sensor_data[0].valid=0;
}
const AlgoCounters_t *Sensor_AlgoCounters(void) { return &s_algorithm.counters; }
uint8_t Sensor_SetFormat(uint8_t v2) { s_format_v2=v2; if(!v2) s_refout=0; return 0; }
uint8_t Sensor_GetFormat(void) { return s_format_v2; }
uint8_t Sensor_SetReferenceOutput(uint8_t enabled)
{
    if(enabled && !s_format_v2) return 1;
    s_refout=enabled; return 0;
}
void Sensor_ProcessSample(const AlgoPair_t *sample,const AlgoTiming_t *timing)
{
    AlgoChannelCfg_t cc={0};
    cc.calibration=sensor_calibration[0];
    cc.mapping[0]=X_MAPPING_VALUE; cc.mapping[1]=Y_MAPPING_VALUE; cc.mapping[2]=Z_MAPPING_VALUE;
    const int8_t actual_rotation[9]=BOARD_SENSOR_ROTATION_ACTUAL;
    const int8_t reference_rotation[9]=BOARD_SENSOR_ROTATION_REFERENCE;
    memcpy(cc.actual_rotation,actual_rotation,sizeof(actual_rotation));
    memcpy(cc.reference_rotation,reference_rotation,sizeof(reference_rotation));
    cc.legacy_settle_ms=drv() ? drv()->settle_ms : 3;
    cc.quantum_uT=drv() ? drv()->quantum_uT : 1.1f;
    if(drv() && drv()->quantum)
        cc.quantum_uT=fmaxf(drv()->quantum(&actual_sensors[0]),drv()->quantum(&reference_sensors[0]));
    AlgoParams_t params=g_algo;
    /* Replay callers set only the historical first three fields. */
    if(!params.config.window_ms) { AlgoParams_t defaults; Algo_DefaultParams(&defaults); params.config=defaults.config; }
    if(!s_initialized || memcmp(&params,&s_previous_params,sizeof(params)) ||
       memcmp(&cc,&s_previous_channel,sizeof(cc)) || s_previous_flags!=FLAG_RES) {
        AlgoCounters_t counters=s_algorithm.counters;
        Algo_Reset(&s_algorithm); s_algorithm.counters=counters;
        s_previous_params=params; s_previous_channel=cc; s_previous_flags=FLAG_RES; s_initialized=1;
    }
    s_output_timing=*timing;
    uint8_t trace_now=s_trace_enabled && (!s_trace_clock || (uint32_t)(timing->actual_us-s_trace_time)>=100000U);
    bool valid=(trace_now || s_node_bypass) ?
        Algo_ProcessTrace(sample,timing,&cc,&params,FLAG_RES,s_refout,&s_algorithm,&s_output,s_node_bypass,trace_now?&s_trace:NULL) :
        Algo_Process(sample,timing,&cc,&params,FLAG_RES,s_refout,&s_algorithm,&s_output);
    if(valid)
        processed_sensor_data[0]=s_output.value;
    else processed_sensor_data[0].valid=0;
    s_output_timing.error=s_output.error;
    if(trace_now) { s_trace_ready=1; s_trace_clock=1; s_trace_time=timing->actual_us; }
}
void data_process(void)
{
    /* Compatibility/replay entry: no fabricated high-resolution timing. */
    const AlgoTiming_t untimed={.rate_hz=10};
    Sensor_ProcessSample(&sensor_data[0],&untimed);
}

/* Global runtime config — loaded from Flash at boot, used by main.c for ML SET/GETCFG */
Config_TypeDef g_config;

uint8_t Scan_Ports(void)
{
    uint8_t sensor_count = 0;
    uint8_t r_actual, r_ref;

    SENSOR_List[0] = 0;
    SENSOR_Run[0] = 0;
    if (Config_Validate(&g_config)) return 0;

if (!drv()) {
        LOG_E("SCAN", "[scan] no driver for sensor type %u", (unsigned)BOARD_SENSOR_TYPE);
        return 0;
    }
    LOG_D("SCAN", "[scan] Init actual sensor (%s)...", drv()->name);
    r_actual = drv()->probe(&actual_sensors[0], 0, SENSOR_ROLE_ACTUAL);
    if(r_actual == 0) {
        // 应用配置到实际传感器
        r_actual = Config_ApplyToSensor(&g_config, drv(), &actual_sensors[0], 0);
    }
    LOG_D("SCAN", "[scan] actual begin -> %s", r_actual == 0 ? "OK" : "FAIL");
    IWDG_Feed();

LOG_D("SCAN", "[scan] Init reference sensor (%s)...", drv()->name);
    r_ref = drv()->probe(&reference_sensors[0], 0, SENSOR_ROLE_REFERENCE);
    if(r_ref == 0) {
        // 应用配置到参考传感器
        r_ref = Config_ApplyToSensor(&g_config, drv(), &reference_sensors[0], 0);
    }
    LOG_D("SCAN", "[scan] reference begin -> %s", r_ref == 0 ? "OK" : "FAIL");

    if(r_actual == 0 && r_ref == 0)
    {
        SENSOR_List[0] = 1;
        sensor_count++;
        LOG_D("SCAN", "[scan] Both sensors ready");
    }
    else
    {
        SENSOR_List[0] = 0;
        SENSOR_Run[0] = 0;
        LOG_D("SCAN", "[scan] FAIL: actual=%d ref=%d", r_actual, r_ref);
    }
    return sensor_count;
}

/**
 * 函数: init_data_structures
 * 功能: 初始化所有数据结构
 * 参数: 无
 * 返回: 无
 * 说明: 初始化自适应EMA滤波器、历史数据等，防止未初始化变量导致的问题
 */
void init_data_structures(void) { Sensor_ResetAlgorithm(); }

uint8_t sensor_start()
{
   int i = 0;
   if (!Scan_Ports()) return 1;
   if (drv()->sampling_supported && !drv()->sampling_supported(&g_config)) {
       LOG_W("SENSOR", "Host sampling requires standby or continuous mode");
       return 1;
   }
   // 初始化所有数据结构
   init_data_structures();

   // 启动传感器 - 仅1个传感器
   if(SENSOR_List[i] == 1)
   {
       SENSOR_Run[i] = 1;
   }

   // 执行标定
   if(perform_sensor_calibration() == 0)
   {
       LOG_I("SENSOR", "Sensor ready for measurement.");
       return 0;
   }
   else
   {
       SENSOR_Run[i] = 0;
       LOG_W("SENSOR", "Sensor start failed!");
       return 1;
   }
}

const char *Sensor_GetName(void)
{
    return drv() ? drv()->name : "none";
}

uint8_t sensor_stop()
{
   SENSOR_Run[0] = 0;
   return 0;	
}

static uint8_t wait_for_sensor_pair(void)
{
    /* Drivers without a status register (MLX90393) are ready once settle_ms passed. */
    if (!drv()->ready) return 0;

    uint32_t start = plat_tick_ms();
    for (;;) {
        int8_t actual, reference;
        IWDG_Feed();
        /* Read both statuses (actual first) before judging, as the original
         * per-sensor loops did; a failed status read aborts immediately. */
        actual = drv()->ready(&actual_sensors[0]);
        if (actual == SENSOR_READ_ERROR) { sensor_timing.error=1; return 1; }
        reference = drv()->ready(&reference_sensors[0]);
        if (reference == SENSOR_READ_ERROR) { sensor_timing.error=1; return 1; }
        if (actual == SENSOR_FAULT || reference == SENSOR_FAULT) { sensor_timing.error=2; return 1; }
        if (actual == SENSOR_READY && reference == SENSOR_READY) return 0;
        if ((uint32_t)(plat_tick_ms() - start) >= 100U) {
            sensor_timing.error=3;
            LOG_W("SCAN", "Conversion timeout: actual=%d ref=%d", actual, reference);
            return 1;
        }
        Delay_ms(1);
    }
}

/**
 * 函数: get_sensor_data
 * 功能: 获取所有运行中传感器的转换后物理量数据(单次测量模式)
 * 参数: 无
 * 返回: 0-成功, 1-失败
 * 说明: 启动单次测量->等待测量完成->读取数据,并转换为物理量(温度°C,磁场μT)
 */
uint8_t get_sensor_data()
{
   int i = 0;
   uint8_t status;

    static uint32_t sequence;
    memset(&sensor_timing,0,sizeof(sensor_timing));
    sensor_timing.seq=sequence++;
#ifdef USE_FREERTOS
    sensor_timing.rate_hz=(drv() && drv()->settle_ms>=20) ? 20 : 100;
#else
    sensor_timing.rate_hz=10; /* nominal rate of the maintained bare-metal loop */
#endif
    sensor_timing.timed=1;
    sensor_timing.error=1;
    sensor_timing.actual_us=plat_time_us();
    sensor_timing.reference_us=sensor_timing.actual_us;
    sensor_data[i].valid = 0;
    processed_sensor_data[i].valid = 0;
    if(SENSOR_Run[i] == 0)
    {
        sensor_data[i].valid = 0;  // 标记数据无效
        return 1;
    }

    /*启动实际传感器单次测量*/
    sensor_timing.triggered=!drv()->host_triggered ||
        (drv()->host_triggered(&actual_sensors[i]) && drv()->host_triggered(&reference_sensors[i]));
    sensor_timing.actual_us=plat_time_us();
    status = drv()->trigger(&actual_sensors[i], 0);
    if(status != 0)
    {
        LOG_D("SCAN", "[get] actual startMeasurement FAIL status=0x%02X", status);
        sensor_data[i].valid = 0;
        return 1;
    }
    LOG_D("SCAN", "[get] actual trigger OK");

    /*启动参考传感器单次测量*/
    sensor_timing.reference_us=plat_time_us();
    status = drv()->trigger(&reference_sensors[i], 0);
    if(status != 0)
    {
        LOG_D("SCAN", "[get] ref startMeasurement FAIL status=0x%02X", status);
        sensor_data[i].valid = 0;
        return 1;
    }
    LOG_D("SCAN", "[get] ref trigger OK");

    /*等待测量完成*/
    Delay_ms(drv()->settle_ms);
    IWDG_Feed();  /* 延时后立即喂狗，防止校准过程看门狗复位 */
    sensor_timing.error=0;
    if (wait_for_sensor_pair()) {
        sensor_data[i].valid = 0;
        return 1;
    }

    /*读取实际传感器数据*/
    sensor_timing.error=1;
    status = drv()->read(&actual_sensors[i], 0, &sensor_data[i].actual);
    if(status != 0)
    {
        LOG_D("SCAN", "[get] actual readMeasurement FAIL status=0x%02X", status);
        sensor_data[i].valid = 0;
        return 1;
    }
    LOG_D("SCAN", "[get] actual read: X=%.2f Y=%.2f Z=%.2f",
          sensor_data[i].actual.x, sensor_data[i].actual.y, sensor_data[i].actual.z);

    /*读取参考传感器数据*/
    status = drv()->read(&reference_sensors[i], 0, &sensor_data[i].reference);
    if(status != 0)
    {
        LOG_D("SCAN", "[get] ref readMeasurement FAIL status=0x%02X", status);
        sensor_data[i].valid = 0;
        return 1;
    }
    LOG_D("SCAN", "[get] ref read: X=%.2f Y=%.2f Z=%.2f",
          sensor_data[i].reference.x, sensor_data[i].reference.y, sensor_data[i].reference.z);

    /*标记数据有效*/
    sensor_timing.error=0;
    int32_t skew=(int32_t)(sensor_timing.actual_us-sensor_timing.reference_us);
    if(sensor_timing.triggered && (skew>5000 || skew < -5000)) {
        sensor_timing.error=4;
        sensor_data[i].valid=0;
        return 1;
    }
    sensor_data[i].valid = 1;
    /* P4: data_process() moved to T_PROC (algorithm task separation) */
    return 0;
}




/**
 * 函数: print_calibration_data
 * 功能: 打印标定数据(用于调试)
 * 参数: sensor_index - 传感器索引 (0-4)
 * 返回: 无
 */
void print_calibration_data(uint8_t sensor_index)
{
    if(sensor_index >= 1) return;
    if(sensor_calibration[sensor_index].calibrated == 0) return;

    LOG_D("CALIB", "=== Calibration Data [%d] ===", sensor_index);
    LOG_D("CALIB", "Actual Offset: X=%.2f, Y=%.2f, Z=%.2f",
            sensor_calibration[sensor_index].actual_offset.x,
            sensor_calibration[sensor_index].actual_offset.y,
            sensor_calibration[sensor_index].actual_offset.z);
    LOG_D("CALIB", "Reference Offset: X=%.2f, Y=%.2f, Z=%.2f",
            sensor_calibration[sensor_index].reference_offset.x,
            sensor_calibration[sensor_index].reference_offset.y,
            sensor_calibration[sensor_index].reference_offset.z);
    LOG_D("CALIB", "Actual Std: X=%.2f, Y=%.2f, Z=%.2f",
            sensor_calibration[sensor_index].actual_std.x,
            sensor_calibration[sensor_index].actual_std.y,
            sensor_calibration[sensor_index].actual_std.z);
}

/**
 * 函数: print_sensor_data
 * 功能: 打印传感器原始数据（数据流输出）
 * 参数: sensor_index - 传感器索引 (0-4)
 * 返回: 无
 */
void print_sensor_data(uint8_t sensor_index)
{
    if(sensor_index >= 1) return;
    if(sensor_data[sensor_index].valid == 0) return;

    // 打印实际传感器数据（数据流，不加日志前缀）
    Serial_Printf("sensor%d:%.2f, %.2f, %.2f\r\n",
            sensor_index,
            sensor_data[sensor_index].actual.x,
            sensor_data[sensor_index].actual.y,
            sensor_data[sensor_index].actual.z);
}

/**
 * 函数: print_processed_sensor_data
 * 功能: 打印处理后的传感器数据（数据流输出）
 * 参数: sensor_index - 传感器索引 (0-4)
 * 返回: 无
 */
/**
 * 函数: format_processed_sensor_data
 * 功能: 将处理后的传感器数据格式化到字符串缓冲区
 * 参数: sensor_index - 传感器索引, buf - 输出缓冲区, bufsize - 缓冲区大小
 * 返回: 写入的字节数（不含null终止符），如果数据无效则返回0
 * P5: T_PROC 调用此函数格式化数据，然后通过 TxQueue 发送到 T_TX
 */
int format_processed_sensor_data(uint8_t sensor_index, char *buf, size_t bufsize)
{
    if(sensor_index!=0 || !buf || !bufsize) return 0;
    const AlgoPair_t *p=&processed_sensor_data[0];
    int n;
    if(!s_format_v2) {
        if(!p->valid) return 0;
        n=snprintf(buf,bufsize,"sensor0:%.2f,%.2f,%.2f\r\n",p->actual.x,p->actual.y,p->actual.z);
    } else {
        n=snprintf(buf,bufsize,
            "S ch=0 seq=%lu t=%lu rate=%u valid=%u x=%.6g y=%.6g z=%.6g tr=%lu skew=%ld timing=%s err=%u",
            (unsigned long)s_output_timing.seq,(unsigned long)s_output_timing.actual_us,
            s_output_timing.rate_hz,p->valid,
            (double)(p->valid?p->actual.x:0.0f),(double)(p->valid?p->actual.y:0.0f),
            (double)(p->valid?p->actual.z:0.0f),(unsigned long)s_output_timing.reference_us,
            (long)s_output.pair_skew_us,s_output_timing.triggered?"trigger":"observed",s_output_timing.error);
        if(n<0 || (size_t)n>=bufsize) return 0;
        if(drv()->has_temperature && p->valid && isfinite(p->actual.t)) {
            int extra=snprintf(buf+n,bufsize-n," T=%.3f",(double)p->actual.t);
            if(extra<0 || (size_t)extra>=bufsize-n) return 0;
            n+=extra;
        }
        if(s_refout && p->valid) {
            int extra=snprintf(buf+n,bufsize-n," rx=%.6g ry=%.6g rz=%.6g rrx=%.6g rry=%.6g rrz=%.6g",
                (double)s_output.value.reference.x,(double)s_output.value.reference.y,(double)s_output.value.reference.z,
                (double)s_output.reference_zero.x,(double)s_output.reference_zero.y,(double)s_output.reference_zero.z);
            if(extra<0 || (size_t)extra>=bufsize-n) return 0;
            n+=extra;
        }
        if(n+2>=(int)bufsize) return 0;
        buf[n++]='\r'; buf[n++]='\n'; buf[n]=0;
    }
    return n<0 || (size_t)n>=bufsize ? 0 : n;
}

void print_processed_sensor_data(uint8_t sensor_index)
{
    char line[384];
    if(format_processed_sensor_data(sensor_index,line,sizeof(line))>0) Serial_SendString(line);
}

/**
 * 函数: perform_sensor_calibration
 * 功能: legacy 标定，收集 300 个有效样本，在线计算均值和样本标准差
 * 参数: 无
 * 返回: 0-标定成功, 1-标定失败
 * 说明:
 *   1. 对单通道实际/参考传感器对进行标定
 *   2. 仅有效样本计入 300 点，连续读取失败超过 20 次则中止
 *   3. 将均值保存为标定偏移量,标准差用于评估传感器稳定性
 *   4. 标定前需确保传感器已启动，并保持无载静止
 */
#ifndef ALGO_V2_ONLY
static uint8_t perform_legacy_calibration(void)
{
    enum { CALIBRATION_SAMPLES = 300 };
    uint16_t count=0;
    uint8_t retry_count=0;
    float mean[2][3]={{0}},m2[2][3]={{0}};
    sensor_calibration[0].calibrated=0;
    if(!SENSOR_Run[0]) {
        LOG_W("CALIB", "Calibration skipped: sensor not running");
        return 1;
    }
    LOG_I("CALIB", "Calibrating sensors (300 samples)...");
    while(count<CALIBRATION_SAMPLES) {
        uint32_t started=plat_tick_ms();
        IWDG_Feed();
        if(!SENSOR_Run[0]) return 1;
        if(get_sensor_data()!=0 || !sensor_data[0].valid) {
            if(++retry_count>20) {
                LOG_W("CALIB", "Calibration aborted: sensor read failed repeatedly");
                return 1;
            }
            continue;
        }
        const AlgoVector_t *values[2]={&sensor_data[0].actual,&sensor_data[0].reference};
        bool finite=true;
        for(unsigned channel=0;channel<2;channel++) {
            const AlgoVector_t *v=values[channel];
            if(!isfinite(v->x) || !isfinite(v->y) || !isfinite(v->z)) finite=false;
        }
        if(!finite) {
            if(++retry_count>20) return 1;
            continue;
        }
        retry_count=0;
        count++;
        for(unsigned channel=0;channel<2;channel++) {
            const AlgoVector_t *v=values[channel];
            const float xyz[3]={v->x,v->y,v->z};
            for(unsigned axis=0;axis<3;axis++) {
                float delta=xyz[axis]-mean[channel][axis];
                mean[channel][axis]+=delta/count;
                m2[channel][axis]+=delta*(xyz[axis]-mean[channel][axis]);
            }
        }
#ifdef USE_FREERTOS
        /* Match the 100 Hz acquisition cadence when reads complete sooner. */
        uint32_t elapsed=plat_tick_ms()-started;
        if(count<CALIBRATION_SAMPLES && elapsed<10U) Delay_ms(10U-elapsed);
#else
        (void)started;
#endif
    }
    SensorPair_Calibration result={0};
    result.actual_offset=(AlgoVector_t){mean[0][0],mean[0][1],mean[0][2],0};
    result.reference_offset=(AlgoVector_t){mean[1][0],mean[1][1],mean[1][2],0};
    result.actual_std=(AlgoVector_t){sqrtf(fmaxf(0,m2[0][0])/(count-1)),
        sqrtf(fmaxf(0,m2[0][1])/(count-1)),sqrtf(fmaxf(0,m2[0][2])/(count-1)),0};
    result.reference_std=(AlgoVector_t){sqrtf(fmaxf(0,m2[1][0])/(count-1)),
        sqrtf(fmaxf(0,m2[1][1])/(count-1)),sqrtf(fmaxf(0,m2[1][2])/(count-1)),0};
    result.calibrated=1;
    sensor_calibration[0]=result;
    LOG_I("CALIB", "Calibration complete! Sensor calibrated.");
    return 0;
}

#endif
SensorCalibrationMetadata sensor_calibration_metadata;
static SensorCalibrationExecutor s_calibration_executor;
void Sensor_SetCalibrationExecutor(SensorCalibrationExecutor executor) { s_calibration_executor=executor; }

static uint8_t perform_timed_calibration(void)
{
    sensor_calibration_metadata.version=1;
    if(!SENSOR_Run[0]) { sensor_calibration_metadata.failure=1; return 1; }
    /* Welford: bounded memory and stable variance, no large sample arrays. */
    float means[2][4]={{0}}, m2[2][4]={{0}};
    uint16_t n=0, attempts=0;
    uint32_t start=plat_tick_ms(), started_us=plat_time_us();
#ifndef ALGO_V2_ONLY
    uint16_t duration=g_algo.config.calib_ms;
#endif
    uint32_t period=drv()->settle_ms>=20 ? 50U : 10U;
    sensor_calibration[0].calibrated=0;
    do {
        if(!SENSOR_Run[0]) { sensor_calibration_metadata.failure=1; return 1; }
        uint32_t tick=plat_tick_ms();
        attempts++;
        if(!get_sensor_data() && sensor_data[0].valid) {
            const AlgoVector_t *a=&sensor_data[0].actual,*r=&sensor_data[0].reference;
            float v[2][4]={{a->x,a->y,a->z,a->t},{r->x,r->y,r->z,r->t}};
            uint8_t valid=1;
            for(unsigned i=0;i<2;i++) for(unsigned j=0;j<4;j++) if(!isfinite(v[i][j])) valid=0;
            if(valid) {
                n++;
                for(unsigned i=0;i<2;i++) for(unsigned j=0;j<4;j++) {
                    float d=v[i][j]-means[i][j]; means[i][j]+=d/n;
                    m2[i][j]+=d*(v[i][j]-means[i][j]);
                }
            }
        }
        IWDG_Feed();
        uint32_t elapsed=plat_tick_ms()-tick;
        if(elapsed<period) Delay_ms(period-elapsed);
#ifdef ALGO_V2_ONLY
    /* MA03 always calibrates 300 valid pairs. Bound failure time below the
     * command timeout, and cap attempts to enforce >=90% valid input. */
    } while(n<300U && attempts<334U && (uint32_t)(plat_tick_ms()-start)<10000U);
#else
    } while((uint32_t)(plat_tick_ms()-start)<duration);
#endif
    sensor_calibration_metadata.samples=n; sensor_calibration_metadata.attempts=attempts;
    sensor_calibration_metadata.timestamp_us=started_us;
    sensor_calibration_metadata.duration_us=(plat_tick_ms()-start)*1000U;
    if(n<2) { sensor_calibration_metadata.failure=2; return 1; }
#ifdef ALGO_V2_ONLY
    if(n<300U) { sensor_calibration_metadata.failure=3; return 1; }
#endif
    if((uint32_t)n*10U<(uint32_t)attempts*9U) { sensor_calibration_metadata.failure=3; return 1; }
    SensorPair_Calibration result={0};
    result.actual_offset=(AlgoVector_t){means[0][0],means[0][1],means[0][2],means[0][3]};
    result.reference_offset=(AlgoVector_t){means[1][0],means[1][1],means[1][2],means[1][3]};
    result.actual_std=(AlgoVector_t){sqrtf(m2[0][0]/(n-1)),sqrtf(m2[0][1]/(n-1)),sqrtf(m2[0][2]/(n-1)),0};
    result.reference_std=(AlgoVector_t){sqrtf(m2[1][0]/(n-1)),sqrtf(m2[1][1]/(n-1)),sqrtf(m2[1][2]/(n-1)),0};
    result.calibrated=1;
    sensor_calibration[0]=result;
    memset(&sensor_calibration_metadata,0,sizeof(sensor_calibration_metadata));
    sensor_calibration_metadata.version=1;
    sensor_calibration_metadata.samples=n; sensor_calibration_metadata.attempts=attempts;
    sensor_calibration_metadata.timestamp_us=started_us;
    sensor_calibration_metadata.duration_us=(plat_tick_ms()-start)*1000U;
    sensor_calibration_metadata.rate_hz=(float)attempts*1000000.0f/sensor_calibration_metadata.duration_us;
    sensor_calibration_metadata.actual_temperature=means[0][3];
    sensor_calibration_metadata.reference_temperature=means[1][3];
    sensor_calibration_metadata.temperature_valid=drv()->has_temperature;
    /* FNV-1a over the complete result + metadata prefix. Not a Flash schema. */
    uint32_t hash=2166136261U;
    const uint8_t *bytes=(const uint8_t *)&result;
    for(unsigned i=0;i<sizeof(result);i++) hash=(hash^bytes[i])*16777619U;
    bytes=(const uint8_t *)&sensor_calibration_metadata;
    for(unsigned i=0;i<offsetof(SensorCalibrationMetadata,checksum);i++) hash=(hash^bytes[i])*16777619U;
    sensor_calibration_metadata.checksum=hash;
    return 0;
}
uint8_t Sensor_CalibrateOwned(void)
{
    memset(&sensor_calibration_metadata,0,sizeof(sensor_calibration_metadata));
#ifdef ALGO_V2_ONLY
    uint8_t failed=perform_timed_calibration();
#else
    uint8_t failed=g_algo.config.profile==ALGO_PROFILE_V2 ? perform_timed_calibration() : perform_legacy_calibration();
#endif
    Sensor_ResetAlgorithm();
    return failed;
}
uint8_t perform_sensor_calibration(void)
{
    return s_calibration_executor ? s_calibration_executor() : Sensor_CalibrateOwned();
}

/**
 * 函数: Config_Init_All
 * 功能: 初始化所有配置参数，从Flash读取
 * 参数: 无
 * 返回: 无
 * 说明: 系统启动时调用，如果Flash中无有效数据则使用默认值
 */
void Config_Init_All(void)
{
    Config_Init();

    if (Config_Load(&g_config) == 0)
    {
        LOG_I("CFG", "Config loaded (schema v%u):", g_config.schema_ver);
    }
    else
    {
        LOG_I("CFG", "Config defaults applied.");
        Config_Save(&g_config);
    }

    /* Apply to runtime variables */
    FLAG_RES        = g_config.FLAG_RES;
    X_MAPPING_VALUE = g_config.X_MAPPING_VALUE;
    Y_MAPPING_VALUE = g_config.Y_MAPPING_VALUE;
    Z_MAPPING_VALUE = g_config.Z_MAPPING_VALUE;
    AlgoParams_Load(&g_config);
    Log_Init((LogLevel_t)g_config.log_level);

    LOG_D("CFG", "FLAG_RES=0x%02X  Map:%.2f,%.2f,%.2f",
                  FLAG_RES, X_MAPPING_VALUE, Y_MAPPING_VALUE, Z_MAPPING_VALUE);
    LOG_D("CFG", "dead=%.2f  spike=%.2f  stab=%.3f",
                  g_algo.dead_value, g_algo.spike_multiplier, g_algo.stability_threshold);
}

/**
 * 函数: Config_Save_All
 * 功能: 保存所有配置参数到Flash
 * 参数: 无
 * 返回: 0-成功, 1-失败
 */
uint8_t Config_Save_All(void)
{
    /* Sync runtime vars back to g_config before saving */
    g_config.algo                = g_algo.config;
    g_config.FLAG_RES            = FLAG_RES;
    g_config.X_MAPPING_VALUE     = X_MAPPING_VALUE;
    g_config.Y_MAPPING_VALUE     = Y_MAPPING_VALUE;
    g_config.Z_MAPPING_VALUE     = Z_MAPPING_VALUE;
    g_config.dead_value          = g_algo.dead_value;
    g_config.spike_multiplier    = g_algo.spike_multiplier;
    g_config.stability_threshold = g_algo.stability_threshold;

    if (Config_Save(&g_config) == 0)
    {
        LOG_I("CFG", "Config saved.");
        return 0;
    }
    LOG_W("CFG", "Config save FAILED.");
    return 1;
}

/**
 * 函数: Mapping_Value_Save
 * 功能: 保存映射系数到Flash（同时保存FLAG_RES）
 * 参数: 无
 * 返回: 0-成功, 1-失败
 */
uint8_t Mapping_Value_Save(void)
{
    return Config_Save_All();
}
