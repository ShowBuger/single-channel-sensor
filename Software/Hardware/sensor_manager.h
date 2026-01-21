#ifndef __SENSOR_MANAGER_H
#define __SENSOR_MANAGER_H

#include <stdint.h>
#include "mlx90393.h"

// 传感器数据结构 - 包含实际传感器和参考传感器的数据
typedef struct {
    MLX90393_Data actual;      // 实际传感器数据
    MLX90393_Data reference;   // 参考传感器数据
    uint8_t valid;             // 数据有效标志 (0-无效, 1-有效)
} SensorPair_Data;

// 传感器标定值结构 - 存储每对传感器的标定值
typedef struct {
    MLX90393_Data actual_offset;      // 实际传感器标定偏移量（均值）
    MLX90393_Data reference_offset;   // 参考传感器标定偏移量（均值）
    MLX90393_Data actual_std;         // 实际传感器标准差
    MLX90393_Data reference_std;      // 参考传感器标准差
    uint8_t calibrated;               // 标定完成标志 (0-未标定, 1-已标定)
} SensorPair_Calibration;

// 传感器历史数据结构 - 用于预测补偿
typedef struct {
    MLX90393_Data history[3];    // 历史数据缓冲区（3个数据点）
    MLX90393_Data velocity;      // 速度（变化率）
    MLX90393_Data acceleration;  // 加速度
    uint8_t historyIndex;        // 当前写入索引
    uint8_t historyCount;        // 已存储的历史数据数量
} SensorHistory;

// 滑动平均滤波结构
#define SLIDING_WINDOW_SIZE 5  // 滑动窗口大小
typedef struct {
    float buffer_x[SLIDING_WINDOW_SIZE];  // X轴数据缓冲区
    float buffer_y[SLIDING_WINDOW_SIZE];  // Y轴数据缓冲区
    float buffer_z[SLIDING_WINDOW_SIZE];  // Z轴数据缓冲区
    uint8_t index;                         // 当前写入索引
    uint8_t count;                         // 已存储的数据数量
    uint8_t initialized;                   // 初始化标志
} SlidingAverage;

// 稳定性检测和平滑过渡结构
#define STABILITY_CHECK_SAMPLES 10  // 稳定性检测所需的采样次数
#define STABILITY_THRESHOLD 0.1f    // 稳定性阈值 (μT)，小于此值才考虑过渡到0
typedef struct {
    uint8_t stable_count_x;         // X轴稳定计数
    uint8_t stable_count_y;         // Y轴稳定计数
    uint8_t stable_count_z;         // Z轴稳定计数
    float transition_factor;        // 过渡系数 (0.0 - 1.0)，1.0表示不衰减，0.0表示完全归零
    uint8_t in_transition;          // 是否正在过渡中
} StabilityTracker;


#define DEAD_VALUE 8.5f
// 尖峰滤波结构
#define SPIKE_FILTER_THRESHOLD_MULTIPLIER 20.0f  // 尖峰检测阈值倍数（相对于标准差）
typedef struct {
    MLX90393_Data last_valid_actual;      // 上一次有效的实际传感器数据
    MLX90393_Data last_valid_reference;   // 上一次有效的参考传感器数据
    uint8_t initialized;                   // 初始化标志
} SpikeFilter;

// 全局变量声明
extern uint8_t SENSOR_List[1];  // 传感器状态列表
extern uint8_t SENSOR_Run[1];   // 传感器运行状态
extern SensorPair_Data sensor_data[1];  // 1对传感器的数据
extern SensorPair_Data processed_sensor_data[1];  // 1对传感器的处理后数据
extern SensorPair_Calibration sensor_calibration[1];  // 1对传感器的标定值

// FLAG_RES 标志位定义 (每一位代表一个使能标志)
extern uint8_t FLAG_RES;

// FLAG_RES 位掩码定义
#define FLAG_CALIBRATION_ENABLE         (1 << 0)  // Bit 0: 标定使能标志
#define FLAG_DEAD_BAND_ENABLE           (1 << 1)  // Bit 1: 死区过滤使能标志
#define FLAG_SLIDING_WINDOW_FILTER      (1 << 2)  // Bit 2: 滑动窗口滤波使能标志
#define FLAG_PREDICTIVE_COMPENSATION    (1 << 3)  // Bit 3: 预测补偿使能标志
#define FLAG_MAPPING_ENABLE             (1 << 4)  // Bit 4: 映射使能标志
#define FLAG_SMOOTH_TRANSITION_ENABLE   (1 << 5)  // Bit 5: 平滑过渡使能标志

// 参考传感器历史数据（用于预测补偿）
extern SensorHistory reference_sensor_history[1];

// 实际传感器滑动平均滤波器
extern SlidingAverage actual_sensor_filter[1];
// 参考传感器滑动平均滤波器
extern SlidingAverage reference_sensor_filter[1];

// 实际传感器稳定性跟踪器
extern StabilityTracker actual_sensor_stability[1];
// 参考传感器稳定性跟踪器
extern StabilityTracker reference_sensor_stability[1];

// 尖峰滤波器
extern SpikeFilter spike_filters[1];

// 映射系数
extern float X_MAPPING_VALUE;
extern float Y_MAPPING_VALUE;
extern float Z_MAPPING_VALUE;

// 函数声明
uint8_t Scan_Ports(void);
uint8_t sensor_start(void);
uint8_t get_sensor_data(void);
void data_process(void);
void print_sensor_data(uint8_t sensor_index);
uint8_t sensor_stop(void);
void print_processed_sensor_data(uint8_t sensor_index);
void print_calibration_data(uint8_t sensor_index);
uint8_t perform_sensor_calibration(void);

// 配置管理函数
void Config_Init_All(void);
uint8_t Config_Save_All(void);
uint8_t Mapping_Value_Save(void);

#endif

