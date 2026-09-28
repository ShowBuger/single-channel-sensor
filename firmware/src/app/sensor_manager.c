#include "sensor_manager.h"
#include "mlx90393.h"
#include "TCA9548A.h"
#include "Delay.h"
#include <math.h>
#include "serial.h"
#include "config_storage.h"

#define XYZT_FLAGS (MLX90393_FLAG_X | MLX90393_FLAG_Y | MLX90393_FLAG_Z ) // 测量所有轴
#define MEASURE_INTERVAL 8  //测量延时根据OSR和FLT决定

// FLAG_RES: 使用每一位代表不同的使能标志
// Bit 0: 标定使能
// Bit 1: 死区过滤使能
// Bit 2: 滑动窗口滤波使能
// Bit 3: 预测补偿使能
// Bit 4: 映射使能
// Bit 5: 平滑过渡使能
// 初始值: 0x3B = 0011 1011 (标定、死区、预测补偿、映射、平滑过渡 开启，滑动窗口滤波 关闭)
uint8_t FLAG_RES = 0x01;

uint8_t SENSOR_List[1] = {0};   // 传感器状态列表,0-无传感器,1-有传感器
uint8_t SENSOR_Run[1] = {0};    // 传感器运行状态

// 定义2个MLX90393传感器句柄
/*实际传感器数组 - 1个*/
MLX90393_Handle mlx_actual_sensors[1];

/*参考传感器数组 - 1个*/
MLX90393_Handle mlx_reference_sensors[1];

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


uint8_t Scan_Ports(void)
{
    uint8_t sensor_count = 0;
    /*扫描MLX90393传感器自检 - 仅1对传感器*/
    if(MLX90393_begin(&mlx_actual_sensors[0],0,0,-1,0) == 0 && MLX90393_begin(&mlx_reference_sensors[0],0,1,-1,0) == 0)
    {
        SENSOR_List[0] = 1;
        sensor_count++ ;
    }
    else
    {
        SENSOR_List[0] = 0;
        SENSOR_Run[0] = 0;
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
       printf("Sensor ready for measurement.\r\n");
       return 0;
   }
   else
   {
       printf("Sensor start failed!\r\n");
       return 1;
   }
}

uint8_t sensor_stop()
{
   SENSOR_Run[0] = 0;
   return 0;	
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
   MLX90393_RawData raw_actual, raw_reference;

    sensor_data[i].valid = 1;
    if(SENSOR_Run[i] == 0)
    {
        sensor_data[i].valid = 0;  // 标记数据无效
        return 1;
    }
    /*启动实际传感器单次测量*/
    status = MLX90393_startMeasurement(&mlx_actual_sensors[i], 0, XYZT_FLAGS);
    if(error_process(status) != MLX90393_ERROR_NONE)
    {
        sensor_data[i].valid = 0;  // 启动失败,标记无效
        return 1;
    }
    /*启动参考传感器单次测量*/
    status = MLX90393_startMeasurement(&mlx_reference_sensors[i], 0, XYZT_FLAGS);
    if(error_process(status) != MLX90393_ERROR_NONE)
    {
        sensor_data[i].valid = 0;  // 启动失败,标记无效
        return 1;
    }

    /*等待测量完成*/
    Delay_ms(MEASURE_INTERVAL);

    /*读取实际传感器原始数据*/
    status = MLX90393_readMeasurement(&mlx_actual_sensors[i], 0, XYZT_FLAGS, &raw_actual);
    if(error_process(status) != MLX90393_ERROR_NONE)
    {
       sensor_data[i].valid = 0;  // 读取失败,标记无效
       return 1;
    }

    /*读取参考传感器原始数据*/
    status = MLX90393_readMeasurement(&mlx_reference_sensors[i], 0, XYZT_FLAGS, &raw_reference);
    if(error_process(status) != MLX90393_ERROR_NONE)
    {
       sensor_data[i].valid = 0;  // 读取失败,标记无效
       return 1;
    }
    /*转换为物理量*/
    sensor_data[i].actual = MLX90393_convertRaw(raw_actual);
    sensor_data[i].reference = MLX90393_convertRaw(raw_reference);

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
static void update_reference_history(uint8_t sensor_index, MLX90393_Data newData)
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
    MLX90393_Data predictedReference;
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
    MLX90393_Data predictedRefDelta;
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
    threshold_x = DEAD_VALUE * sensor_calibration[sensor_index].actual_std.x;
    threshold_y = DEAD_VALUE * sensor_calibration[sensor_index].actual_std.y;
    threshold_z = DEAD_VALUE * sensor_calibration[sensor_index].actual_std.z;

    if(fabsf(processed_sensor_data[sensor_index].actual.x) < threshold_x)
        processed_sensor_data[sensor_index].actual.x = 0.0f;
    if(fabsf(processed_sensor_data[sensor_index].actual.y) < threshold_y)
        processed_sensor_data[sensor_index].actual.y = 0.0f;
    if(fabsf(processed_sensor_data[sensor_index].actual.z) < threshold_z)
        processed_sensor_data[sensor_index].actual.z = 0.0f;

    // 参考传感器死区滤波
    threshold_x = DEAD_VALUE * sensor_calibration[sensor_index].reference_std.x;
    threshold_y = DEAD_VALUE * sensor_calibration[sensor_index].reference_std.y;
    threshold_z = DEAD_VALUE * sensor_calibration[sensor_index].reference_std.z;

    if(fabsf(processed_sensor_data[sensor_index].reference.x) < threshold_x)
        processed_sensor_data[sensor_index].reference.x = 0.0f;
    if(fabsf(processed_sensor_data[sensor_index].reference.y) < threshold_y)
        processed_sensor_data[sensor_index].reference.y = 0.0f;
    if(fabsf(processed_sensor_data[sensor_index].reference.z) < threshold_z)
        processed_sensor_data[sensor_index].reference.z = 0.0f;
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
    float threshold_x_actual = SPIKE_FILTER_THRESHOLD_MULTIPLIER * sensor_calibration[sensor_index].actual_std.x;
    float threshold_y_actual = SPIKE_FILTER_THRESHOLD_MULTIPLIER * sensor_calibration[sensor_index].actual_std.y;
    float threshold_z_actual = SPIKE_FILTER_THRESHOLD_MULTIPLIER * sensor_calibration[sensor_index].actual_std.z;

    float threshold_x_ref = SPIKE_FILTER_THRESHOLD_MULTIPLIER * sensor_calibration[sensor_index].reference_std.x;
    float threshold_y_ref = SPIKE_FILTER_THRESHOLD_MULTIPLIER * sensor_calibration[sensor_index].reference_std.y;
    float threshold_z_ref = SPIKE_FILTER_THRESHOLD_MULTIPLIER * sensor_calibration[sensor_index].reference_std.z;

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
float X_MAPPING_VALUE = 0.1;
float Y_MAPPING_VALUE = 0.1;
float Z_MAPPING_VALUE = -0.1;
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
    if(abs_x < threshold_x_actual && abs_x < STABILITY_THRESHOLD)
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
    if(abs_y < threshold_y_actual && abs_y < STABILITY_THRESHOLD)
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
    if(abs_z < threshold_z_actual && abs_z < STABILITY_THRESHOLD)
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
    if(abs_x < threshold_x_ref && abs_x < STABILITY_THRESHOLD)
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
    if(abs_y < threshold_y_ref && abs_y < STABILITY_THRESHOLD)
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
    if(abs_z < threshold_z_ref && abs_z < STABILITY_THRESHOLD)
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
        }
    }

    // 预测补偿 - 使用参考传感器预测并补偿实际传感器
    if(FLAG_RES & FLAG_PREDICTIVE_COMPENSATION)
    {
        if(sensor_calibration[i].calibrated == 1)
        {
            apply_predictive_compensation(i);
        }
    }

    // 死区滤波
    if(FLAG_RES & FLAG_DEAD_BAND_ENABLE)
    {
        if(sensor_calibration[i].calibrated == 1)
        {
            apply_dead_band_filter(i);
        }
    }
    // 滑动平均滤波
    if(FLAG_RES & FLAG_SLIDING_WINDOW_FILTER)
    {
        if(sensor_calibration[i].calibrated == 1)
        {
            apply_sliding_average_filter(i);
        }
    }
    // 映射
    if(FLAG_RES & FLAG_MAPPING_ENABLE)
    {
        if(sensor_calibration[i].calibrated == 1)
        {
            apply_mapping(i);
        }
    }

    // 稳定性检测和平滑过渡
    if(FLAG_RES & FLAG_SMOOTH_TRANSITION_ENABLE)
    {
        if(sensor_calibration[i].calibrated == 1)
        {
            apply_stability_and_transition(i);
        }
    }
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

    printf("=== Calibration Data [%d] ===\n", sensor_index);
    printf("Actual Offset: X=%.2f, Y=%.2f, Z=%.2f\n",
            sensor_calibration[sensor_index].actual_offset.x,
            sensor_calibration[sensor_index].actual_offset.y,
            sensor_calibration[sensor_index].actual_offset.z);
    printf("Reference Offset: X=%.2f, Y=%.2f, Z=%.2f\n",
            sensor_calibration[sensor_index].reference_offset.x,
            sensor_calibration[sensor_index].reference_offset.y,
            sensor_calibration[sensor_index].reference_offset.z);
    printf("Actual Std: X=%.2f, Y=%.2f, Z=%.2f\n",
            sensor_calibration[sensor_index].actual_std.x,
            sensor_calibration[sensor_index].actual_std.y,
            sensor_calibration[sensor_index].actual_std.z);
}

/**
 * 函数: print_sensor_data
 * 功能: 打印传感器数据(用于调试)
 * 参数: sensor_index - 传感器索引 (0-4)
 * 返回: 无
 */
void print_sensor_data(uint8_t sensor_index)
{
    if(sensor_index >= 1) return;
    if(sensor_data[sensor_index].valid == 0) return;

    // 打印实际传感器数据
    printf("sensor%d:%.2f, %.2f, %.2f\n",
            sensor_index,
            sensor_data[sensor_index].actual.x,
            sensor_data[sensor_index].actual.y,
            sensor_data[sensor_index].actual.z);
}

/**
 * 函数: print_processed_sensor_data
 * 功能: 打印处理后的传感器数据(用于调试)
 * 参数: sensor_index - 传感器索引 (0-4)
 * 返回: 无
 */
void print_processed_sensor_data(uint8_t sensor_index)
{
    if(processed_sensor_data[sensor_index].valid == 0) return;

    // 打印实际传感器处理后数据
    printf("sensor%d:%.2f, %.2f, %.2f\n",
            sensor_index,
            processed_sensor_data[sensor_index].actual.x,
            processed_sensor_data[sensor_index].actual.y,
            processed_sensor_data[sensor_index].actual.z);
//    // 打印参考传感器处理后数据
//    printf("sensor%d:%.2f, %.2f, %.2f\n",
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
    MLX90393_Data actual_samples[1][10];    // 实际传感器采样数据
    MLX90393_Data reference_samples[1][10]; // 参考传感器采样数据
    uint8_t sample_valid[1][10] = {0};      // 采样有效标志

    // 临时累加器 - 用于存储累加和 - 仅1个传感器
    struct {
        float actual_x, actual_y, actual_z, actual_t;
        float reference_x, reference_y, reference_z, reference_t;
        uint8_t sample_count;  // 有效采样计数
    } accumulator[1] = {0};

    // 清空标定数据
    sensor_calibration[i].calibrated = 0;

    printf("Calibrating sensors (10 samples)...\r\n");

    // 连续读取10次数据
    for(j = 0; j < calibration_samples; j++)
    {
        // 读取所有传感器数据
        if(get_sensor_data() != 0)
        {
            j--;
            continue;  // 读取失败,跳过本次采样
        }

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
        printf("please start sensor!");
        return 1;  // 传感器未运行,跳过
    }

    // 检查是否有足够的有效采样(至少5次)
    if(accumulator[i].sample_count < 5)
    {
        sensor_calibration[i].calibrated = 0;  // 标定失败
        printf("sensor calibrate failed!");
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
        printf("Calibration complete! Sensor calibrated.\r\n");
        return 0;  // 标定成功
    }
    else
    {
        printf("Calibration failed! Sensor not calibrated.\r\n");
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
    uint8_t loaded_flag;
    float loaded_x_map, loaded_y_map, loaded_z_map;

    // 初始化配置存储模块
    Config_Init();

    // 从Flash读取配置
    if(Config_Load(&loaded_flag, &loaded_x_map, &loaded_y_map, &loaded_z_map) == 0)
    {
        // 成功读取到有效配置
        FLAG_RES = loaded_flag;
        X_MAPPING_VALUE = loaded_x_map;
        Y_MAPPING_VALUE = loaded_y_map;
        Z_MAPPING_VALUE = loaded_z_map;
        printf("Config loaded from Flash:\r\n");
        printf("  FLAG_RES = 0x%02X\r\n", FLAG_RES);
        printf("  Mapping: X=%.2f, Y=%.2f, Z=%.2f\r\n",
               X_MAPPING_VALUE, Y_MAPPING_VALUE, Z_MAPPING_VALUE);
    }
    else
    {
        // Flash中无有效数据，使用默认值并保存
        FLAG_RES = 0x3B;
        X_MAPPING_VALUE = 0.1f;
        Y_MAPPING_VALUE = 0.1f;
        Z_MAPPING_VALUE = -0.1f;
        Config_Save(FLAG_RES, X_MAPPING_VALUE, Y_MAPPING_VALUE, Z_MAPPING_VALUE);
        printf("Config restored to default:\r\n");
        printf("  FLAG_RES = 0x%02X\r\n", FLAG_RES);
        printf("  Mapping: X=%.2f, Y=%.2f, Z=%.2f\r\n",
               X_MAPPING_VALUE, Y_MAPPING_VALUE, Z_MAPPING_VALUE);
    }
}

/**
 * 函数: Config_Save_All
 * 功能: 保存所有配置参数到Flash
 * 参数: 无
 * 返回: 0-成功, 1-失败
 */
uint8_t Config_Save_All(void)
{
    if(Config_Save(FLAG_RES, X_MAPPING_VALUE, Y_MAPPING_VALUE, Z_MAPPING_VALUE) == 0)
    {
        printf("Config saved to Flash:\r\n");
        printf("  FLAG_RES = 0x%02X\r\n", FLAG_RES);
        printf("  Mapping: X=%.2f, Y=%.2f, Z=%.2f\r\n",
               X_MAPPING_VALUE, Y_MAPPING_VALUE, Z_MAPPING_VALUE);
        return 0;
    }
    else
    {
        printf("Failed to save config to Flash\r\n");
        return 1;
    }
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
