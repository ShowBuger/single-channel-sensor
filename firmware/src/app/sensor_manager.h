#ifndef __SENSOR_MANAGER_H
#define __SENSOR_MANAGER_H

#include <stdint.h>
#include "../drivers/sensors/sensor_hal.h"
#include "algo_params.h"

// 传感器数据类型定义（统一使用HAL层的SensorData_t）
typedef SensorData_t SensorData;

// 传感器数据结构 - 包含实际传感器和参考传感器的数据
typedef AlgoPair_t SensorPair_Data;
typedef AlgoCalibration_t SensorPair_Calibration;

extern uint8_t SENSOR_List[1], SENSOR_Run[1];
extern SensorPair_Data sensor_data[1], processed_sensor_data[1];
extern SensorPair_Calibration sensor_calibration[1];
extern AlgoTiming_t sensor_timing;
void Sensor_ProcessSample(const AlgoPair_t *sample, const AlgoTiming_t *timing);
void Sensor_ResetAlgorithm(void);
const AlgoCounters_t *Sensor_AlgoCounters(void);
typedef uint8_t (*SensorCalibrationExecutor)(void);
void Sensor_SetCalibrationExecutor(SensorCalibrationExecutor executor);
uint8_t Sensor_CalibrateOwned(void);
typedef struct {
    uint16_t version, samples, attempts;
    uint8_t temperature_valid, failure; /* 1=stopped, 2=too few, 3=valid ratio */
    uint32_t timestamp_us, duration_us;
    float rate_hz, actual_temperature, reference_temperature;
    uint32_t checksum;
} SensorCalibrationMetadata;
extern SensorCalibrationMetadata sensor_calibration_metadata;
uint8_t Sensor_SetFormat(uint8_t v2);
uint8_t Sensor_SetReferenceOutput(uint8_t enabled);
uint8_t Sensor_GetFormat(void);
void Sensor_SetTrace(uint8_t enabled);
uint8_t Sensor_GetTrace(void);
uint16_t Sensor_GetNodeBypass(void);
uint8_t Sensor_SetNode(uint8_t node,uint8_t enabled);
uint8_t Sensor_TakeTrace(AlgoTrace_t *trace);
int Sensor_FormatTrace(const AlgoTrace_t *trace,unsigned node,char *buf,size_t size);

// FLAG_RES 标志位定义 (每一位代表一个使能标志)
extern uint8_t FLAG_RES;

// FLAG_RES 位掩码定义
#define FLAG_CALIBRATION_ENABLE         (1 << 0)  // Bit 0: 标定使能标志
#define FLAG_DEAD_BAND_ENABLE           (1 << 1)  // Bit 1: 死区过滤使能标志
#define FLAG_SLIDING_WINDOW_FILTER      (1 << 2)  // Bit 2: 滑动窗口滤波使能标志
#define FLAG_PREDICTIVE_COMPENSATION    (1 << 3)  // Bit 3: 预测补偿使能标志
#define FLAG_MAPPING_ENABLE             (1 << 4)  // Bit 4: 映射使能标志
#define FLAG_SMOOTH_TRANSITION_ENABLE   (1 << 5)  // Bit 5: 平滑过渡使能标志
#define FLAG_Z_POSITIVE_FILTER          (1 << 6)  // Bit 6: Z轴正值过滤使能标志
#define FLAG_SPIKE_FILTER               (1 << 7)  // v2 robust spike rejection (default off)

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
/* Name of the sensor driver compiled into this firmware, e.g. "AK09974C". */
const char *Sensor_GetName(void);
void print_processed_sensor_data(uint8_t sensor_index);

/**
 * @brief Format processed sensor data into a string buffer (P5: for TxQueue)
 * @param sensor_index Sensor index (0 for single-channel system)
 * @param buf Output buffer
 * @param bufsize Buffer size
 * @return Number of bytes written (excluding null terminator), or 0 if data invalid
 *
 * P5: T_PROC calls this to format data, then sends result via TxQueue to T_TX.
 * Bare-metal code can still call print_processed_sensor_data() for direct output.
 */
int format_processed_sensor_data(uint8_t sensor_index, char *buf, size_t bufsize);

void print_calibration_data(uint8_t sensor_index);
uint8_t perform_sensor_calibration(void);

// 配置管理函数
void Config_Init_All(void);
uint8_t Config_Save_All(void);
uint8_t Mapping_Value_Save(void);

/* Global runtime config — set by Config_Init_All, read/written by ML SET / ML GETCFG */
#include "config_manager.h"
extern Config_TypeDef g_config;

#endif

