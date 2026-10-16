#include "sensor_manager.h"
#include "../drivers/sensors/sensor_hal.h"
#include "../drivers/sensors/tmag3001/tmag3001.h"
#include "../drivers/sensors/tca9548a/tca9548a.h"
#include "../drivers/bsp/bsp_delay.h"
#include <math.h>
#include "../drivers/bsp/bsp_uart.h"
#include "../drivers/bsp/log_manager.h"
#include "config_manager.h"
#include "algo_params.h"
#include "config_parser.h"
#include "stm32f1xx_hal.h"

extern void IWDG_Feed(void);

// 使用TMAG3001传感器
#define SENSOR_HAL g_hal_tmag3001
// TMAG3001测量延时: 典型转换时间约2ms
#define MEASURE_INTERVAL 2  /* CONTINUOUS 模式下传感器自动转换，只需短暂延迟 */

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

// 定义2个TMAG3001传感器句柄
/*实际传感器数组 - 1个*/
TMAG3001_Handle_t tmag_actual_sensors[1];

/*参考传感器数组 - 1个*/
TMAG3001_Handle_t tmag_reference_sensors[1];

// 传感器数据存储 - 1对传感器的数据
SensorPair_Data sensor_data[1] = {0};

// 处理后的传感器数据存储 - 1对传感器的处理后数据
SensorPair_Data processed_sensor_data[1] = {0};
// 传感器标定值存储 - 1对传感器的标定值
SensorPair_Calibration sensor_calibration[1] = {0};

// 参考传感器历史数据存储 - 用于预测补偿
SensorHistory reference_sensor_history[1] = {0};

// 滑动平均滤波器存储
SlidingAverage actual_sensor_filter[1] = {0};
SlidingAverage reference_sensor_filter[1] = {0};

// 稳定性跟踪器存储
StabilityTracker actual_sensor_stability[1] = {0};
StabilityTracker reference_sensor_stability[1] = {0};

// 尖峰滤波器存储
SpikeFilter spike_filters[1] = {0};

/* Global runtime config — loaded from Flash at boot, used by main.c for ML SET/GETCFG */
Config_TypeDef g_config;

uint8_t Scan_Ports(void)
{
    uint8_t sensor_count = 0;
    uint8_t r_actual, r_ref;

    SENSOR_List[0] = 0;
    SENSOR_Run[0] = 0;
    if (Config_Validate(&g_config)) return 0;

    LOG_D("SCAN", "[scan] Init actual sensor (TMAG3001, addr=0x35)...");
    tmag_actual_sensors[0].i2c_addr = TMAG3001_I2C_ADDR_35;
    r_actual = SENSOR_HAL.init(&tmag_actual_sensors[0], 0, 0, -1, 0);
    if(r_actual == 0) {
        // 应用配置到实际传感器
        r_actual = Config_ApplyToSensor(&g_config, &tmag_actual_sensors[0], 0);
    }
    LOG_D("SCAN", "[scan] actual begin -> %s", r_actual == 0 ? "OK" : "FAIL");
    IWDG_Feed();

    LOG_D("SCAN", "[scan] Init reference sensor (TMAG3001, addr=0x34)...");
    tmag_reference_sensors[0].i2c_addr = TMAG3001_I2C_ADDR_34;
    r_ref = SENSOR_HAL.init(&tmag_reference_sensors[0], 0, 1, -1, 0);
    if(r_ref == 0) {
        // 应用配置到参考传感器
        r_ref = Config_ApplyToSensor(&g_config, &tmag_reference_sensors[0], 0);
    }
    LOG_D("SCAN", "[scan] reference begin -> %s", r_ref == 0 ? "OK" : "FAIL");

    if(r_actual == 0 && r_ref == 0)
    {
        SENSOR_List[0] = 1;
        sensor_count++;
        LOG_D("SCAN", "[scan] Both TMAG3001 sensors ready");
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
void init_data_structures(void)
{
    int i, j;

    // 初始化滑动平均滤波器 - 仅1个传感器
    i = 0;
    {
        // 实际传感器滤波器初始化
        actual_sensor_filter[i].index = 0;
        actual_sensor_filter[i].count = 0;
        actual_sensor_filter[i].initialized = 0;
        for(j = 0; j < SLIDING_WINDOW_SIZE; j++)
        {
            actual_sensor_filter[i].buffer_x[j] = 0.0f;
            actual_sensor_filter[i].buffer_y[j] = 0.0f;
            actual_sensor_filter[i].buffer_z[j] = 0.0f;
        }

        // 参考传感器滤波器初始化
        reference_sensor_filter[i].index = 0;
        reference_sensor_filter[i].count = 0;
        reference_sensor_filter[i].initialized = 0;
        for(j = 0; j < SLIDING_WINDOW_SIZE; j++)
        {
            reference_sensor_filter[i].buffer_x[j] = 0.0f;
            reference_sensor_filter[i].buffer_y[j] = 0.0f;
            reference_sensor_filter[i].buffer_z[j] = 0.0f;
        }
    }

    // 初始化参考传感器历史数据 - 仅1个传感器
    i = 0;
    {
        reference_sensor_history[i].historyIndex = 0;
        reference_sensor_history[i].historyCount = 0;
        for(j = 0; j < 3; j++)
        {
            reference_sensor_history[i].history[j].x = 0.0f;
            reference_sensor_history[i].history[j].y = 0.0f;
            reference_sensor_history[i].history[j].z = 0.0f;
            reference_sensor_history[i].history[j].t = 0.0f;
        }
        reference_sensor_history[i].velocity.x = 0.0f;
        reference_sensor_history[i].velocity.y = 0.0f;
        reference_sensor_history[i].velocity.z = 0.0f;
        reference_sensor_history[i].velocity.t = 0.0f;
        reference_sensor_history[i].acceleration.x = 0.0f;
        reference_sensor_history[i].acceleration.y = 0.0f;
        reference_sensor_history[i].acceleration.z = 0.0f;
        reference_sensor_history[i].acceleration.t = 0.0f;
    }

    // 初始化稳定性跟踪器 - 仅1个传感器
    i = 0;
    {
        // 实际传感器稳定性跟踪器初始化
        actual_sensor_stability[i].stable_count_x = 0;
        actual_sensor_stability[i].stable_count_y = 0;
        actual_sensor_stability[i].stable_count_z = 0;
        actual_sensor_stability[i].transition_factor = 1.0f;
        actual_sensor_stability[i].in_transition = 0;

        // 参考传感器稳定性跟踪器初始化
        reference_sensor_stability[i].stable_count_x = 0;
        reference_sensor_stability[i].stable_count_y = 0;
        reference_sensor_stability[i].stable_count_z = 0;
        reference_sensor_stability[i].transition_factor = 1.0f;
        reference_sensor_stability[i].in_transition = 0;
    }

    // 初始化尖峰滤波器 - 仅1个传感器
    i = 0;
    {
        spike_filters[i].initialized = 0;
        spike_filters[i].last_valid_actual.x = 0.0f;
        spike_filters[i].last_valid_actual.y = 0.0f;
        spike_filters[i].last_valid_actual.z = 0.0f;
        spike_filters[i].last_valid_actual.t = 0.0f;
        spike_filters[i].last_valid_reference.x = 0.0f;
        spike_filters[i].last_valid_reference.y = 0.0f;
        spike_filters[i].last_valid_reference.z = 0.0f;
        spike_filters[i].last_valid_reference.t = 0.0f;
    }
}

uint8_t sensor_start()
{
   int i = 0;
   if (!Scan_Ports()) return 1;
   if (g_config.op_mode != TMAG3001_MODE_STANDBY && g_config.op_mode != TMAG3001_MODE_CONTINUOUS) {
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

uint8_t sensor_stop()
{
   SENSOR_Run[0] = 0;
   return 0;	
}

static uint8_t wait_for_sensor_pair(void)
{
    uint32_t start = HAL_GetTick();
    for (;;) {
        uint8_t actual, reference;
        IWDG_Feed();
        if (TMAG3001_ReadConvStatus(&tmag_actual_sensors[0], &actual) ||
            TMAG3001_ReadConvStatus(&tmag_reference_sensors[0], &reference)) return 1;
        if ((actual | reference) & (TMAG3001_STATUS_DIAG | TMAG3001_STATUS_POR)) return 1;
        uint8_t actual_new = !tmag_actual_sensors[0].sample_seen ||
            (actual & TMAG3001_STATUS_SET_COUNT) != tmag_actual_sensors[0].last_set_count;
        uint8_t reference_new = !tmag_reference_sensors[0].sample_seen ||
            (reference & TMAG3001_STATUS_SET_COUNT) != tmag_reference_sensors[0].last_set_count;
        if ((actual & reference & TMAG3001_STATUS_READY) && actual_new && reference_new) return 0;
        if ((uint32_t)(HAL_GetTick() - start) >= 100U) {
            LOG_W("SCAN", "Conversion timeout: actual=0x%02X ref=0x%02X", actual, reference);
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

    sensor_data[i].valid = 1;
    processed_sensor_data[i].valid = 0;
    if(SENSOR_Run[i] == 0)
    {
        sensor_data[i].valid = 0;  // 标记数据无效
        return 1;
    }

    /*启动实际传感器单次测量*/
    status = SENSOR_HAL.start_meas(&tmag_actual_sensors[i], 0);
    if(status != 0)
    {
        LOG_D("SCAN", "[get] actual startMeasurement FAIL status=0x%02X", status);
        sensor_data[i].valid = 0;
        return 1;
    }
    LOG_D("SCAN", "[get] actual trigger OK");

    /*启动参考传感器单次测量*/
    status = SENSOR_HAL.start_meas(&tmag_reference_sensors[i], 0);
    if(status != 0)
    {
        LOG_D("SCAN", "[get] ref startMeasurement FAIL status=0x%02X", status);
        sensor_data[i].valid = 0;
        return 1;
    }
    LOG_D("SCAN", "[get] ref trigger OK");

    /*等待测量完成*/
    Delay_ms(MEASURE_INTERVAL);
    if (wait_for_sensor_pair()) {
        sensor_data[i].valid = 0;
        return 1;
    }

    /*读取实际传感器数据*/
    status = SENSOR_HAL.read_meas(&tmag_actual_sensors[i], 0, &sensor_data[i].actual);
    if(status != 0)
    {
        LOG_D("SCAN", "[get] actual readMeasurement FAIL status=0x%02X", status);
        sensor_data[i].valid = 0;
        return 1;
    }
    LOG_D("SCAN", "[get] actual read: X=%.2f Y=%.2f Z=%.2f",
          sensor_data[i].actual.x, sensor_data[i].actual.y, sensor_data[i].actual.z);

    /*读取参考传感器数据*/
    status = SENSOR_HAL.read_meas(&tmag_reference_sensors[i], 0, &sensor_data[i].reference);
    if(status != 0)
    {
        LOG_D("SCAN", "[get] ref readMeasurement FAIL status=0x%02X", status);
        sensor_data[i].valid = 0;
        return 1;
    }
    LOG_D("SCAN", "[get] ref read: X=%.2f Y=%.2f Z=%.2f",
          sensor_data[i].reference.x, sensor_data[i].reference.y, sensor_data[i].reference.z);

    /*标记数据有效*/
    sensor_data[i].valid = 1;
    data_process();
    return 0;
}




/**
 * 函数: update_reference_history
 * 功能: 更新参考传感器的历史数据,并计算速度和加速度
 * 参数: sensor_index - 传感器索引 (0-4)
 *       newData - 新的参考传感器数据
 * 返回: 无
 */
static void update_reference_history(uint8_t sensor_index, SensorData newData)
{
    if(sensor_index >= 1) return;

    SensorHistory* history = &reference_sensor_history[sensor_index];

    // 存储新数据到历史记录
    history->history[history->historyIndex] = newData;
    history->historyIndex = (history->historyIndex + 1) % 3;

    if(history->historyCount < 3) {
        history->historyCount++;
    }

    // 如果有足够的历史数据,计算速度
    if(history->historyCount >= 2) {
        // 当前最新数据索引
        uint8_t currentIndex = (history->historyIndex + 2) % 3;
        // 前一个数据索引
        uint8_t prevIndex = (history->historyIndex + 1) % 3;

        history->velocity.x = history->history[currentIndex].x - history->history[prevIndex].x;
        history->velocity.y = history->history[currentIndex].y - history->history[prevIndex].y;
        history->velocity.z = history->history[currentIndex].z - history->history[prevIndex].z;
    }

    // 如果有3个历史数据,计算加速度
    if(history->historyCount >= 3) {
        uint8_t prevIndex = (history->historyIndex + 1) % 3;
        uint8_t prevPrevIndex = history->historyIndex;

        // 计算前一时刻的速度
        float prevVelX = history->history[prevIndex].x - history->history[prevPrevIndex].x;
        float prevVelY = history->history[prevIndex].y - history->history[prevPrevIndex].y;
        float prevVelZ = history->history[prevIndex].z - history->history[prevPrevIndex].z;

        // 计算加速度 = 当前速度 - 前一时刻速度
        history->acceleration.x = history->velocity.x - prevVelX;
        history->acceleration.y = history->velocity.y - prevVelY;
        history->acceleration.z = history->velocity.z - prevVelZ;
    }
}

/**
 * 函数: apply_predictive_compensation
 * 功能: 使用参考传感器的运动趋势预测并补偿实际传感器
 * 参数: sensor_index - 传感器索引 (0-4)
 * 返回: 无
 * 说明: 假设参考传感器与实际传感器之间有固定的时间延迟
 */
static void apply_predictive_compensation(uint8_t sensor_index)
{
    if(sensor_index >= 1) return;

    SensorHistory* refHistory = &reference_sensor_history[sensor_index];

    // 至少需要2个历史数据点才能进行预测
    if(refHistory->historyCount < 2) return;

    // 时间间隔(根据MEASURE_INTERVAL调整,单位:秒)
    float timeDelta = MEASURE_INTERVAL / 1000.0f;  // 25ms = 0.025s

    // 预测参考传感器的当前值(使用线性预测)
    SensorData predictedReference;
    uint8_t currentIndex = (refHistory->historyIndex + 2) % 3;

    predictedReference.x = refHistory->history[currentIndex].x + refHistory->velocity.x * timeDelta;
    predictedReference.y = refHistory->history[currentIndex].y + refHistory->velocity.y * timeDelta;
    predictedReference.z = refHistory->history[currentIndex].z + refHistory->velocity.z * timeDelta;

    // 如果有加速度信息,使用二次预测
    if(refHistory->historyCount >= 3) {
        predictedReference.x += 0.5f * refHistory->acceleration.x * timeDelta * timeDelta;
        predictedReference.y += 0.5f * refHistory->acceleration.y * timeDelta * timeDelta;
        predictedReference.z += 0.5f * refHistory->acceleration.z * timeDelta * timeDelta;
    }

    // 计算预测的参考传感器变化量(相对于标定值)
    SensorData predictedRefDelta;
    predictedRefDelta.x = predictedReference.x - sensor_calibration[sensor_index].reference_offset.x;
    predictedRefDelta.y = predictedReference.y - sensor_calibration[sensor_index].reference_offset.y;
    predictedRefDelta.z = predictedReference.z - sensor_calibration[sensor_index].reference_offset.z;

    // 用预测的参考变化量来补偿实际传感器
    // 实际传感器的补偿值 = 实际传感器当前值 - 预测的参考变化量
    processed_sensor_data[sensor_index].actual.x -= predictedRefDelta.x;
    processed_sensor_data[sensor_index].actual.y += predictedRefDelta.y;
    processed_sensor_data[sensor_index].actual.z += predictedRefDelta.z;
}

/**
 * 函数: apply_dead_band_filter
 * 功能: 死区滤波 - 对小于2倍标准差的数据置零
 * 参数: sensor_index - 传感器索引 (0-4)
 * 返回: 无
 * 说明: 死区阈值 = 2 × 标准差
 */
void apply_dead_band_filter(uint8_t sensor_index)
{
    if(sensor_index >= 1) return;
    if(sensor_calibration[sensor_index].calibrated == 0) return;

    float threshold_x, threshold_y, threshold_z;

    // 实际传感器死区滤波
    threshold_x = g_algo.dead_value * sensor_calibration[sensor_index].actual_std.x;
    threshold_y = g_algo.dead_value * sensor_calibration[sensor_index].actual_std.y;
    threshold_z = g_algo.dead_value * sensor_calibration[sensor_index].actual_std.z;

    if(fabsf(processed_sensor_data[sensor_index].actual.x) < threshold_x)
        processed_sensor_data[sensor_index].actual.x = 0.0f;
    if(fabsf(processed_sensor_data[sensor_index].actual.y) < threshold_y)
        processed_sensor_data[sensor_index].actual.y = 0.0f;
    if(fabsf(processed_sensor_data[sensor_index].actual.z) < threshold_z)
        processed_sensor_data[sensor_index].actual.z = 0.0f;

    // 参考传感器死区滤波
    threshold_x = g_algo.dead_value * sensor_calibration[sensor_index].reference_std.x;
    threshold_y = g_algo.dead_value * sensor_calibration[sensor_index].reference_std.y;
    threshold_z = g_algo.dead_value * sensor_calibration[sensor_index].reference_std.z;

    if(fabsf(processed_sensor_data[sensor_index].reference.x) < threshold_x)
        processed_sensor_data[sensor_index].reference.x = 0.0f;
    if(fabsf(processed_sensor_data[sensor_index].reference.y) < threshold_y)
        processed_sensor_data[sensor_index].reference.y = 0.0f;
    if(fabsf(processed_sensor_data[sensor_index].reference.z) < threshold_z)
        processed_sensor_data[sensor_index].reference.z = 0.0f;
}

/**
 * 函数: apply_z_positive_filter
 * 功能: Z轴正值过滤 - 将正值的z轴数据改为0
 * 参数: sensor_index - 传感器索引 (0-4)
 * 返回: 无
 * 说明: 自动过滤掉值为正的z轴数据，将其置为0
 */
void apply_z_positive_filter(uint8_t sensor_index)
{
    if(sensor_index >= 1) return;

    // 实际传感器Z轴正值过滤
    if(processed_sensor_data[sensor_index].actual.z > 0.0f)
    {
        processed_sensor_data[sensor_index].actual.z = 0.0f;
    }

    // 参考传感器Z轴正值过滤
    if(processed_sensor_data[sensor_index].reference.z > 0.0f)
    {
        processed_sensor_data[sensor_index].reference.z = 0.0f;
    }
}

/**
 * 函数: apply_spike_filter
 * 功能: 尖峰滤波 - 检测并过滤偶发的突变数据
 * 参数: sensor_index - 传感器索引 (0-4)
 * 返回: 无
 * 说明: 通过对比当前数据与上一次有效数据，检测是否存在尖峰
 *       如果变化量超过阈值（10倍标准差），则认为是尖峰，用上一次有效值替代
 */
void apply_spike_filter(uint8_t sensor_index)
{
    if(sensor_index >= 1) return;
    if(sensor_calibration[sensor_index].calibrated == 0) return;

    SpikeFilter *filter = &spike_filters[sensor_index];

    // 如果是第一次运行，初始化为当前值
    if(filter->initialized == 0)
    {
        filter->last_valid_actual = processed_sensor_data[sensor_index].actual;
        filter->last_valid_reference = processed_sensor_data[sensor_index].reference;
        filter->initialized = 1;
        return;
    }

    // 计算尖峰检测阈值（10倍标准差）
    float threshold_x_actual = g_algo.spike_multiplier * sensor_calibration[sensor_index].actual_std.x;
    float threshold_y_actual = g_algo.spike_multiplier * sensor_calibration[sensor_index].actual_std.y;
    float threshold_z_actual = g_algo.spike_multiplier * sensor_calibration[sensor_index].actual_std.z;

    float threshold_x_ref = g_algo.spike_multiplier * sensor_calibration[sensor_index].reference_std.x;
    float threshold_y_ref = g_algo.spike_multiplier * sensor_calibration[sensor_index].reference_std.y;
    float threshold_z_ref = g_algo.spike_multiplier * sensor_calibration[sensor_index].reference_std.z;

    // ========== 处理实际传感器 ==========
    float delta_x = fabsf(processed_sensor_data[sensor_index].actual.x - filter->last_valid_actual.x);
    float delta_y = fabsf(processed_sensor_data[sensor_index].actual.y - filter->last_valid_actual.y);
    float delta_z = fabsf(processed_sensor_data[sensor_index].actual.z - filter->last_valid_actual.z);

    uint8_t spike_detected_actual = 0;

    // 检测X轴尖峰
    if(delta_x > threshold_x_actual && threshold_x_actual > 0.01f)
    {
        processed_sensor_data[sensor_index].actual.x = filter->last_valid_actual.x;
        spike_detected_actual = 1;
    }

    // 检测Y轴尖峰
    if(delta_y > threshold_y_actual && threshold_y_actual > 0.01f)
    {
        processed_sensor_data[sensor_index].actual.y = filter->last_valid_actual.y;
        spike_detected_actual = 1;
    }

    // 检测Z轴尖峰
    if(delta_z > threshold_z_actual && threshold_z_actual > 0.01f)
    {
        processed_sensor_data[sensor_index].actual.z = filter->last_valid_actual.z;
        spike_detected_actual = 1;
    }

    // 如果没有检测到尖峰，更新上一次有效值
    if(spike_detected_actual == 0)
    {
        filter->last_valid_actual = processed_sensor_data[sensor_index].actual;
    }

    // ========== 处理参考传感器 ==========
    delta_x = fabsf(processed_sensor_data[sensor_index].reference.x - filter->last_valid_reference.x);
    delta_y = fabsf(processed_sensor_data[sensor_index].reference.y - filter->last_valid_reference.y);
    delta_z = fabsf(processed_sensor_data[sensor_index].reference.z - filter->last_valid_reference.z);

    uint8_t spike_detected_ref = 0;

    // 检测X轴尖峰
    if(delta_x > threshold_x_ref && threshold_x_ref > 0.01f)
    {
        processed_sensor_data[sensor_index].reference.x = filter->last_valid_reference.x;
        spike_detected_ref = 1;
    }

    // 检测Y轴尖峰
    if(delta_y > threshold_y_ref && threshold_y_ref > 0.01f)
    {
        processed_sensor_data[sensor_index].reference.y = filter->last_valid_reference.y;
        spike_detected_ref = 1;
    }

    // 检测Z轴尖峰
    if(delta_z > threshold_z_ref && threshold_z_ref > 0.01f)
    {
        processed_sensor_data[sensor_index].reference.z = filter->last_valid_reference.z;
        spike_detected_ref = 1;
    }

    // 如果没有检测到尖峰，更新上一次有效值
    if(spike_detected_ref == 0)
    {
        filter->last_valid_reference = processed_sensor_data[sensor_index].reference;
    }
}


/**
 * 函数: is_valid_float
 * 功能: 检查浮点数是否有效(非NaN、非Inf、在合理范围内)
 */
static inline uint8_t is_valid_float(float x)
{
    // 检查是否为NaN或Inf
    if(x != x) return 0;  // NaN检测
    if(x > 1e10f || x < -1e10f) return 0;  // 溢出检测
    return 1;
}

/**
 * 函数: apply_sliding_average_filter
 * 功能: 滑动平均滤波 - 对最近N个数据求平均
 * 参数: sensor_index - 传感器索引 (0-4)
 * 返回: 无
 * 说明: 使用固定窗口大小的滑动平均滤波,简单稳定
 */
void apply_sliding_average_filter(uint8_t sensor_index)
{
    if(sensor_index >= 1) return;

    SlidingAverage *actual_filter = &actual_sensor_filter[sensor_index];
    SlidingAverage *reference_filter = &reference_sensor_filter[sensor_index];
    int i;

    // ========== 处理实际传感器数据 ==========
    float x = processed_sensor_data[sensor_index].actual.x;
    float y = processed_sensor_data[sensor_index].actual.y;
    float z = processed_sensor_data[sensor_index].actual.z;

    // 检查输入数据有效性
    if(!is_valid_float(x)) x = 0.0f;
    if(!is_valid_float(y)) y = 0.0f;
    if(!is_valid_float(z)) z = 0.0f;

    // 将新数据写入缓冲区
    actual_filter->buffer_x[actual_filter->index] = x;
    actual_filter->buffer_y[actual_filter->index] = y;
    actual_filter->buffer_z[actual_filter->index] = z;

    // 更新索引和计数
    actual_filter->index = (actual_filter->index + 1) % SLIDING_WINDOW_SIZE;
    if(actual_filter->count < SLIDING_WINDOW_SIZE) {
        actual_filter->count++;
    }

    // 计算滑动平均 - 遍历整个缓冲区累加所有有效数据
    float sum_x = 0.0f, sum_y = 0.0f, sum_z = 0.0f;
    uint8_t valid_count = (actual_filter->count < SLIDING_WINDOW_SIZE) ? actual_filter->count : SLIDING_WINDOW_SIZE;

    for(i = 0; i < valid_count; i++)
    {
        sum_x += actual_filter->buffer_x[i];
        sum_y += actual_filter->buffer_y[i];
        sum_z += actual_filter->buffer_z[i];
    }

    // 输出平均值
    if(valid_count > 0)
    {
        processed_sensor_data[sensor_index].actual.x = sum_x / valid_count;
        processed_sensor_data[sensor_index].actual.y = sum_y / valid_count;
        processed_sensor_data[sensor_index].actual.z = sum_z / valid_count;
    }

    actual_filter->initialized = 1;

    // ========== 处理参考传感器数据 ==========
    x = processed_sensor_data[sensor_index].reference.x;
    y = processed_sensor_data[sensor_index].reference.y;
    z = processed_sensor_data[sensor_index].reference.z;

    // 检查输入数据有效性
    if(!is_valid_float(x)) x = 0.0f;
    if(!is_valid_float(y)) y = 0.0f;
    if(!is_valid_float(z)) z = 0.0f;

    // 将新数据写入缓冲区
    reference_filter->buffer_x[reference_filter->index] = x;
    reference_filter->buffer_y[reference_filter->index] = y;
    reference_filter->buffer_z[reference_filter->index] = z;

    // 更新索引和计数
    reference_filter->index = (reference_filter->index + 1) % SLIDING_WINDOW_SIZE;
    if(reference_filter->count < SLIDING_WINDOW_SIZE) {
        reference_filter->count++;
    }

    // 计算滑动平均 - 遍历整个缓冲区累加所有有效数据
    sum_x = 0.0f;
    sum_y = 0.0f;
    sum_z = 0.0f;
    valid_count = (reference_filter->count < SLIDING_WINDOW_SIZE) ? reference_filter->count : SLIDING_WINDOW_SIZE;

    for(i = 0; i < valid_count; i++)
    {
        sum_x += reference_filter->buffer_x[i];
        sum_y += reference_filter->buffer_y[i];
        sum_z += reference_filter->buffer_z[i];
    }

    // 输出平均值
    if(valid_count > 0)
    {
        processed_sensor_data[sensor_index].reference.x = sum_x / valid_count;
        processed_sensor_data[sensor_index].reference.y = sum_y / valid_count;
        processed_sensor_data[sensor_index].reference.z = sum_z / valid_count;
    }

    reference_filter->initialized = 1;
}
float X_MAPPING_VALUE = 1.0f;
float Y_MAPPING_VALUE = 1.0f;
float Z_MAPPING_VALUE = 1.0f;
void apply_mapping(uint8_t sensor_index)
{
		if(sensor_index >= 1) return;
    if(sensor_calibration[sensor_index].calibrated == 0) return;
    processed_sensor_data[sensor_index].actual.x = processed_sensor_data[sensor_index].actual.x * X_MAPPING_VALUE;
    processed_sensor_data[sensor_index].actual.y = processed_sensor_data[sensor_index].actual.y * Y_MAPPING_VALUE;
    processed_sensor_data[sensor_index].actual.z = processed_sensor_data[sensor_index].actual.z * Z_MAPPING_VALUE;
}

/**
 * 函数: apply_stability_and_transition
 * 功能: 检测传感器数据稳定性，并在满足条件时平滑过渡到0
 * 参数: sensor_index - 传感器索引 (0-4)
 * 返回: 无
 * 说明:
 *   1. 检测数据是否在3倍标准差内且小于阈值
 *   2. 若连续STABILITY_CHECK_SAMPLES次满足条件，则启动平滑过渡
 *   3. 使用指数衰减实现平滑过渡到0
 */
void apply_stability_and_transition(uint8_t sensor_index)
{
    if(sensor_index >= 1) return;
    if(sensor_calibration[sensor_index].calibrated == 0) return;

    StabilityTracker *actual_tracker = &actual_sensor_stability[sensor_index];
    StabilityTracker *ref_tracker = &reference_sensor_stability[sensor_index];

    // 获取3倍标准差阈值
    float threshold_x_actual = 3.0f * sensor_calibration[sensor_index].actual_std.x;
    float threshold_y_actual = 3.0f * sensor_calibration[sensor_index].actual_std.y;
    float threshold_z_actual = 3.0f * sensor_calibration[sensor_index].actual_std.z;

    // ========== 处理实际传感器 ==========
    float abs_x = fabsf(processed_sensor_data[sensor_index].actual.x);
    float abs_y = fabsf(processed_sensor_data[sensor_index].actual.y);
    float abs_z = fabsf(processed_sensor_data[sensor_index].actual.z);

    // X轴稳定性检测
    if(abs_x < threshold_x_actual && abs_x < g_algo.stability_threshold)
    {
        if(actual_tracker->stable_count_x < STABILITY_CHECK_SAMPLES)
        {
            actual_tracker->stable_count_x++;
        }
    }
    else
    {
        actual_tracker->stable_count_x = 0;
        actual_tracker->in_transition = 0;
        actual_tracker->transition_factor = 1.0f;
    }

    // Y轴稳定性检测
    if(abs_y < threshold_y_actual && abs_y < g_algo.stability_threshold)
    {
        if(actual_tracker->stable_count_y < STABILITY_CHECK_SAMPLES)
        {
            actual_tracker->stable_count_y++;
        }
    }
    else
    {
        actual_tracker->stable_count_y = 0;
        actual_tracker->in_transition = 0;
        actual_tracker->transition_factor = 1.0f;
    }

    // Z轴稳定性检测
    if(abs_z < threshold_z_actual && abs_z < g_algo.stability_threshold)
    {
        if(actual_tracker->stable_count_z < STABILITY_CHECK_SAMPLES)
        {
            actual_tracker->stable_count_z++;
        }
    }
    else
    {
        actual_tracker->stable_count_z = 0;
        actual_tracker->in_transition = 0;
        actual_tracker->transition_factor = 1.0f;
    }

    // 如果所有轴都稳定，启动平滑过渡
    if(actual_tracker->stable_count_x >= STABILITY_CHECK_SAMPLES &&
       actual_tracker->stable_count_y >= STABILITY_CHECK_SAMPLES &&
       actual_tracker->stable_count_z >= STABILITY_CHECK_SAMPLES)
    {
        actual_tracker->in_transition = 1;

        // 指数衰减: 每次乘以0.9，实现平滑过渡
        actual_tracker->transition_factor *= 0.9f;

        // 当过渡系数小于0.01时，直接归零
        if(actual_tracker->transition_factor < 0.01f)
        {
            actual_tracker->transition_factor = 0.0f;
        }

        // 应用过渡系数
        processed_sensor_data[sensor_index].actual.x *= actual_tracker->transition_factor;
        processed_sensor_data[sensor_index].actual.y *= actual_tracker->transition_factor;
        processed_sensor_data[sensor_index].actual.z *= actual_tracker->transition_factor;
    }

    // ========== 处理参考传感器 (类似逻辑) ==========
    float threshold_x_ref = 3.0f * sensor_calibration[sensor_index].reference_std.x;
    float threshold_y_ref = 3.0f * sensor_calibration[sensor_index].reference_std.y;
    float threshold_z_ref = 3.0f * sensor_calibration[sensor_index].reference_std.z;

    abs_x = fabsf(processed_sensor_data[sensor_index].reference.x);
    abs_y = fabsf(processed_sensor_data[sensor_index].reference.y);
    abs_z = fabsf(processed_sensor_data[sensor_index].reference.z);

    // X轴稳定性检测
    if(abs_x < threshold_x_ref && abs_x < g_algo.stability_threshold)
    {
        if(ref_tracker->stable_count_x < STABILITY_CHECK_SAMPLES)
            ref_tracker->stable_count_x++;
    }
    else
    {
        ref_tracker->stable_count_x = 0;
        ref_tracker->in_transition = 0;
        ref_tracker->transition_factor = 1.0f;
    }

    // Y轴稳定性检测
    if(abs_y < threshold_y_ref && abs_y < g_algo.stability_threshold)
    {
        if(ref_tracker->stable_count_y < STABILITY_CHECK_SAMPLES)
            ref_tracker->stable_count_y++;
    }
    else
    {
        ref_tracker->stable_count_y = 0;
        ref_tracker->in_transition = 0;
        ref_tracker->transition_factor = 1.0f;
    }

    // Z轴稳定性检测
    if(abs_z < threshold_z_ref && abs_z < g_algo.stability_threshold)
    {
        if(ref_tracker->stable_count_z < STABILITY_CHECK_SAMPLES)
            ref_tracker->stable_count_z++;
    }
    else
    {
        ref_tracker->stable_count_z = 0;
        ref_tracker->in_transition = 0;
        ref_tracker->transition_factor = 1.0f;
    }

    // 如果所有轴都稳定，启动平滑过渡
    if(ref_tracker->stable_count_x >= STABILITY_CHECK_SAMPLES &&
       ref_tracker->stable_count_y >= STABILITY_CHECK_SAMPLES &&
       ref_tracker->stable_count_z >= STABILITY_CHECK_SAMPLES)
    {
        ref_tracker->in_transition = 1;

        // 指数衰减
        ref_tracker->transition_factor *= 0.9f;

        // 当过渡系数小于0.01时，直接归零
        if(ref_tracker->transition_factor < 0.01f)
        {
            ref_tracker->transition_factor = 0.0f;
        }

        // 应用过渡系数
        processed_sensor_data[sensor_index].reference.x *= ref_tracker->transition_factor;
        processed_sensor_data[sensor_index].reference.y *= ref_tracker->transition_factor;
        processed_sensor_data[sensor_index].reference.z *= ref_tracker->transition_factor;
    }
}

/**
 * 函数: data_process
 * 功能: 处理传感器数据(标定值补偿、参考传感器校准、过零检测、滤波等)
 * 参数: 无
 * 返回: 无
 * 说明: 对获取的数据进行后处理,不修改原始sensor_data,处理结果存储在processed_sensor_data中
 */
void data_process()
{
    int i = 0;

    // 对每个传感器进行处理
    // 先检查数据是否有效
    if(sensor_data[i].valid == 0)
    {
        processed_sensor_data[i].valid = 0;  // 标记处理后数据无效
        return;
    }
    // 拷贝有效数据到processed_sensor_data
    processed_sensor_data[i] = sensor_data[i];

    LOG_D("ALGO", "=== Algorithm Pipeline Start ===");
    LOG_D("ALGO", "[0] Raw: X=%.2f Y=%.2f Z=%.2f",
            sensor_data[i].actual.x, sensor_data[i].actual.y, sensor_data[i].actual.z);

    if(sensor_calibration[i].calibrated == 1)
    {
        // 更新参考传感器历史数据(用于预测补偿)
        update_reference_history(i, sensor_data[i].reference);
    }

    // 标定值补偿
    if(FLAG_RES & FLAG_CALIBRATION_ENABLE)
    {
        // 检查该传感器是否已经标定过
        if(sensor_calibration[i].calibrated == 1)
        {
            // 实际传感器: 原始数据减去标定值,保留变化量
            processed_sensor_data[i].actual.x -= sensor_calibration[i].actual_offset.x;
            processed_sensor_data[i].actual.y -= sensor_calibration[i].actual_offset.y;
            processed_sensor_data[i].actual.z -= sensor_calibration[i].actual_offset.z;

            // 参考传感器: 原始数据减去标定值,保留变化量
            processed_sensor_data[i].reference.x -= sensor_calibration[i].reference_offset.x;
            processed_sensor_data[i].reference.y -= sensor_calibration[i].reference_offset.y;
            processed_sensor_data[i].reference.z -= sensor_calibration[i].reference_offset.z;

            LOG_D("ALGO", "[1] Calibration: X=%.2f Y=%.2f Z=%.2f (offset: %.2f,%.2f,%.2f)",
                    processed_sensor_data[i].actual.x, processed_sensor_data[i].actual.y, processed_sensor_data[i].actual.z,
                    sensor_calibration[i].actual_offset.x, sensor_calibration[i].actual_offset.y, sensor_calibration[i].actual_offset.z);
        }
    }
    else
    {
        LOG_D("ALGO", "[1] Calibration: DISABLED");
    }

    // 预测补偿 - 使用参考传感器预测并补偿实际传感器
    if(FLAG_RES & FLAG_PREDICTIVE_COMPENSATION)
    {
        if(sensor_calibration[i].calibrated == 1)
        {
            SensorData before = processed_sensor_data[i].actual;
            apply_predictive_compensation(i);
            LOG_D("ALGO", "[2] Predictive: X=%.2f Y=%.2f Z=%.2f (delta: %.2f,%.2f,%.2f)",
                    processed_sensor_data[i].actual.x, processed_sensor_data[i].actual.y, processed_sensor_data[i].actual.z,
                    processed_sensor_data[i].actual.x - before.x,
                    processed_sensor_data[i].actual.y - before.y,
                    processed_sensor_data[i].actual.z - before.z);
        }
    }
    else
    {
        LOG_D("ALGO", "[2] Predictive: DISABLED");
    }

    // 死区滤波
    if(FLAG_RES & FLAG_DEAD_BAND_ENABLE)
    {
        if(sensor_calibration[i].calibrated == 1)
        {
            SensorData before = processed_sensor_data[i].actual;
            apply_dead_band_filter(i);
            LOG_D("ALGO", "[3] DeadBand: X=%.2f Y=%.2f Z=%.2f (before: %.2f,%.2f,%.2f)",
                    processed_sensor_data[i].actual.x, processed_sensor_data[i].actual.y, processed_sensor_data[i].actual.z,
                    before.x, before.y, before.z);
        }
    }
    else
    {
        LOG_D("ALGO", "[3] DeadBand: DISABLED");
    }

    // Z轴正值过滤 - 在死区滤波后执行
    if(FLAG_RES & FLAG_Z_POSITIVE_FILTER)
    {
        float before_z = processed_sensor_data[i].actual.z;
        apply_z_positive_filter(i);
        LOG_D("ALGO", "[4] Z-Filter: Z=%.2f (before: %.2f)", processed_sensor_data[i].actual.z, before_z);
    }
    else
    {
        LOG_D("ALGO", "[4] Z-Filter: DISABLED");
    }

    // 滑动平均滤波
    if(FLAG_RES & FLAG_SLIDING_WINDOW_FILTER)
    {
        if(sensor_calibration[i].calibrated == 1)
        {
            SensorData before = processed_sensor_data[i].actual;
            apply_sliding_average_filter(i);
            LOG_D("ALGO", "[5] SlidingAvg: X=%.2f Y=%.2f Z=%.2f (before: %.2f,%.2f,%.2f)",
                    processed_sensor_data[i].actual.x, processed_sensor_data[i].actual.y, processed_sensor_data[i].actual.z,
                    before.x, before.y, before.z);
        }
    }
    else
    {
        LOG_D("ALGO", "[5] SlidingAvg: DISABLED");
    }

    // 映射
    if(FLAG_RES & FLAG_MAPPING_ENABLE)
    {
        if(sensor_calibration[i].calibrated == 1)
        {
            SensorData before = processed_sensor_data[i].actual;
            apply_mapping(i);
            LOG_D("ALGO", "[6] Mapping: X=%.2f Y=%.2f Z=%.2f (coeff: %.3f,%.3f,%.3f)",
                    processed_sensor_data[i].actual.x, processed_sensor_data[i].actual.y, processed_sensor_data[i].actual.z,
                    X_MAPPING_VALUE, Y_MAPPING_VALUE, Z_MAPPING_VALUE);
        }
    }
    else
    {
        LOG_D("ALGO", "[6] Mapping: DISABLED");
    }

    // 稳定性检测和平滑过渡
    if(FLAG_RES & FLAG_SMOOTH_TRANSITION_ENABLE)
    {
        if(sensor_calibration[i].calibrated == 1)
        {
            SensorData before = processed_sensor_data[i].actual;
            apply_stability_and_transition(i);
            LOG_D("ALGO", "[7] Smooth: X=%.2f Y=%.2f Z=%.2f (before: %.2f,%.2f,%.2f)",
                    processed_sensor_data[i].actual.x, processed_sensor_data[i].actual.y, processed_sensor_data[i].actual.z,
                    before.x, before.y, before.z);
        }
    }
    else
    {
        LOG_D("ALGO", "[7] Smooth: DISABLED");
    }

    LOG_D("ALGO", "=== Final Output: X=%.2f Y=%.2f Z=%.2f ===",
            processed_sensor_data[i].actual.x, processed_sensor_data[i].actual.y, processed_sensor_data[i].actual.z);
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
void print_processed_sensor_data(uint8_t sensor_index)
{
    if(processed_sensor_data[sensor_index].valid == 0) return;

    // 打印实际传感器处理后数据（数据流，不加日志前缀）
    Serial_Printf("sensor%d:%.2f,%.2f,%.2f\r\n",
            sensor_index,
            processed_sensor_data[sensor_index].actual.x,
            processed_sensor_data[sensor_index].actual.y,
            processed_sensor_data[sensor_index].actual.z);
//    // 打印参考传感器处理后数据
//    Serial_Printf("sensor%d:%.2f, %.2f, %.2f\r\n",
//            sensor_index+5,
//            processed_sensor_data[sensor_index].reference.x,
//            processed_sensor_data[sensor_index].reference.y,
//            processed_sensor_data[sensor_index].reference.z);
}

/**
 * 函数: perform_sensor_calibration
 * 功能: 传感器标定 - 连续读取10次所有传感器数据并计算均值和标准差作为标定值
 * 参数: 无
 * 返回: 0-标定成功, 1-标定失败
 * 说明:
 *   1. 对所有运行中的传感器(5对,共10个)进行标定
 *   2. 连续读取10次数据,计算每个轴的均值和标准差
 *   3. 将均值保存为标定偏移量,标准差用于评估传感器稳定性
 *   4. 标定前需确保传感器已启动突发模式
 */
uint8_t perform_sensor_calibration(void)
{
    int i = 0;
    int j, k;
    uint8_t calibration_samples = 10;  // 标定采样次数

    // 数据缓冲区 - 存储所有采样数据用于计算标准差 - 仅1个传感器
    SensorData actual_samples[1][10];    // 实际传感器采样数据
    SensorData reference_samples[1][10]; // 参考传感器采样数据
    uint8_t sample_valid[1][10] = {0};      // 采样有效标志

    // 临时累加器 - 用于存储累加和 - 仅1个传感器
    struct {
        float actual_x, actual_y, actual_z, actual_t;
        float reference_x, reference_y, reference_z, reference_t;
        uint8_t sample_count;  // 有效采样计数
    } accumulator[1] = {0};

    // 清空标定数据
    sensor_calibration[i].calibrated = 0;

    /* 传感器未启动则直接报错，避免后续死循环 */
    if(SENSOR_Run[i] == 0)
    {
        LOG_W("CALIB", "Calibration skipped: sensor not running");
        return 1;
    }

    LOG_I("CALIB", "Calibrating sensors (10 samples)...");

    // 连续读取10次数据
    uint8_t retry_count = 0;
    for(j = 0; j < calibration_samples; j++)
    {
        IWDG_Feed();
        // 读取所有传感器数据
        if(get_sensor_data() != 0)
        {
            if(++retry_count > 20)  // 最多允许20次连续失败，防止死循环
            {
                LOG_W("CALIB", "Calibration aborted: sensor read failed repeatedly");
                return 1;
            }
            j--;
            continue;  // 读取失败,重试本次采样
        }
        retry_count = 0;  // 读取成功则重置重试计数器

        // 保存数据并累加有效数据
        if(SENSOR_Run[i] == 0) continue;  // 传感器未运行,跳过

        // 保存本次采样数据
        actual_samples[i][j] = sensor_data[i].actual;
        reference_samples[i][j] = sensor_data[i].reference;
        sample_valid[i][j] = 1;

        // 累加实际传感器数据
        accumulator[i].actual_x += sensor_data[i].actual.x;
        accumulator[i].actual_y += sensor_data[i].actual.y;
        accumulator[i].actual_z += sensor_data[i].actual.z;

        // 累加参考传感器数据
        accumulator[i].reference_x += sensor_data[i].reference.x;
        accumulator[i].reference_y += sensor_data[i].reference.y;
        accumulator[i].reference_z += sensor_data[i].reference.z;

        // 增加有效采样计数
        accumulator[i].sample_count++;
    }

    // 计算均值、标准差并保存为标定值
    if(SENSOR_Run[i] == 0)
    {
        LOG_W("CALIB", "please start sensor!");
        return 1;  // 传感器未运行,跳过
    }

    // 检查是否有足够的有效采样(至少5次)
    if(accumulator[i].sample_count < 5)
    {
        sensor_calibration[i].calibrated = 0;  // 标定失败
        LOG_W("CALIB", "sensor calibrate failed!");
        return 1;
    }

    // 计算实际传感器的均值
    sensor_calibration[i].actual_offset.x = accumulator[i].actual_x / accumulator[i].sample_count;
    sensor_calibration[i].actual_offset.y = accumulator[i].actual_y / accumulator[i].sample_count;
    sensor_calibration[i].actual_offset.z = accumulator[i].actual_z / accumulator[i].sample_count;

    // 计算参考传感器的均值
    sensor_calibration[i].reference_offset.x = accumulator[i].reference_x / accumulator[i].sample_count;
    sensor_calibration[i].reference_offset.y = accumulator[i].reference_y / accumulator[i].sample_count;
    sensor_calibration[i].reference_offset.z = accumulator[i].reference_z / accumulator[i].sample_count;

    // 计算标准差 - 实际传感器
    float variance_actual_x = 0, variance_actual_y = 0, variance_actual_z = 0;
    float variance_ref_x = 0, variance_ref_y = 0, variance_ref_z = 0;

    for(k = 0; k < calibration_samples; k++)
    {
        if(sample_valid[i][k] == 0) continue;

        // 实际传感器方差累加
        float diff_x = actual_samples[i][k].x - sensor_calibration[i].actual_offset.x;
        float diff_y = actual_samples[i][k].y - sensor_calibration[i].actual_offset.y;
        float diff_z = actual_samples[i][k].z - sensor_calibration[i].actual_offset.z;

        variance_actual_x += diff_x * diff_x;
        variance_actual_y += diff_y * diff_y;
        variance_actual_z += diff_z * diff_z;

        // 参考传感器方差累加
        diff_x = reference_samples[i][k].x - sensor_calibration[i].reference_offset.x;
        diff_y = reference_samples[i][k].y - sensor_calibration[i].reference_offset.y;
        diff_z = reference_samples[i][k].z - sensor_calibration[i].reference_offset.z;

        variance_ref_x += diff_x * diff_x;
        variance_ref_y += diff_y * diff_y;
        variance_ref_z += diff_z * diff_z;
    }

    // 计算标准差(使用样本标准差公式,除以 n-1)
    uint8_t n = accumulator[i].sample_count;
    if(n > 1)
    {
        // 需要包含 math.h 来使用 sqrtf
        sensor_calibration[i].actual_std.x = sqrtf(variance_actual_x / (n - 1));
        sensor_calibration[i].actual_std.y = sqrtf(variance_actual_y / (n - 1));
        sensor_calibration[i].actual_std.z = sqrtf(variance_actual_z / (n - 1));

        sensor_calibration[i].reference_std.x = sqrtf(variance_ref_x / (n - 1));
        sensor_calibration[i].reference_std.y = sqrtf(variance_ref_y / (n - 1));
        sensor_calibration[i].reference_std.z = sqrtf(variance_ref_z / (n - 1));
    }
    else
    {
        // 只有一个样本,标准差为0
        sensor_calibration[i].actual_std.x = 0;
        sensor_calibration[i].actual_std.y = 0;
        sensor_calibration[i].actual_std.z = 0;

        sensor_calibration[i].reference_std.x = 0;
        sensor_calibration[i].reference_std.y = 0;
        sensor_calibration[i].reference_std.z = 0;
    }

    // 标记标定完成
    sensor_calibration[i].calibrated = 1;

    // 检查传感器是否标定成功 - 仅1个传感器
    if(sensor_calibration[i].calibrated == 1)
    {
        LOG_I("CALIB", "Calibration complete! Sensor calibrated.");
        return 0;  // 标定成功
    }
    else
    {
        LOG_W("CALIB", "Calibration failed! Sensor not calibrated.");
        return 1;  // 标定失败
    }
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
