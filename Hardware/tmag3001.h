#ifndef __TMAG3001_H
#define __TMAG3001_H

#include "stm32f10x.h"

// Version: 3.5 with gain support - Force recompilation

// TMAG3001 I2C地址 (根据ADDR引脚连接) - 7位地址
#define TMAG3001_I2C_ADDR_GND    0x34  // ADDR接GND
#define TMAG3001_I2C_ADDR_VCC    0x36  // ADDR接VCC
#define TMAG3001_I2C_ADDR_SDA    0x37  // ADDR接SDA
#define TMAG3001_I2C_ADDR_SCL    0x38  // ADDR接SCL

// 寄存器地址
#define TMAG3001_REG_DEVICE_CONFIG_1    0x00
#define TMAG3001_REG_DEVICE_CONFIG_2    0x01
#define TMAG3001_REG_SENSOR_CONFIG_1    0x02
#define TMAG3001_REG_SENSOR_CONFIG_2    0x03
#define TMAG3001_REG_X_THR_CONFIG       0x04
#define TMAG3001_REG_Y_THR_CONFIG       0x05
#define TMAG3001_REG_Z_THR_CONFIG       0x06
#define TMAG3001_REG_T_CONFIG           0x07
#define TMAG3001_REG_INT_CONFIG_1       0x08
#define TMAG3001_REG_MAG_GAIN_CONFIG    0x09
#define TMAG3001_REG_MAG_OFFSET_CONFIG_1 0x0A
#define TMAG3001_REG_MAG_OFFSET_CONFIG_2 0x0B
#define TMAG3001_REG_I2C_ADDRESS        0x0C
#define TMAG3001_REG_DEVICE_ID          0x0D
#define TMAG3001_REG_MANUFACTURER_ID_LSB 0x0E
#define TMAG3001_REG_MANUFACTURER_ID_MSB 0x0F
#define TMAG3001_REG_T_MSB_RESULT       0x10
#define TMAG3001_REG_T_LSB_RESULT       0x11
#define TMAG3001_REG_X_MSB_RESULT       0x12
#define TMAG3001_REG_X_LSB_RESULT       0x13
#define TMAG3001_REG_Y_MSB_RESULT       0x14
#define TMAG3001_REG_Y_LSB_RESULT       0x15
#define TMAG3001_REG_Z_MSB_RESULT       0x16
#define TMAG3001_REG_Z_LSB_RESULT       0x17
#define TMAG3001_REG_CONV_STATUS        0x18
#define TMAG3001_REG_ANGLE_RESULT_MSB   0x19
#define TMAG3001_REG_ANGLE_RESULT_LSB   0x1A
#define TMAG3001_REG_MAGNITUDE_RESULT   0x1B
#define TMAG3001_REG_DEVICE_STATUS      0x1C

// 设备ID和制造商ID
#define TMAG3001_DEVICE_ID              0x01
#define TMAG3001_MANUFACTURER_ID        0x5449  // "TI"

// 操作模式
typedef enum {
    TMAG3001_MODE_STANDBY = 0x00,
    TMAG3001_MODE_SLEEP = 0x01,
    TMAG3001_MODE_CONTINUOUS = 0x02,
    TMAG3001_MODE_WAKEUP_AND_SLEEP = 0x03
} TMAG3001_OperatingMode_t;

// 磁场测量范围
typedef enum {
    TMAG3001_RANGE_40mT = 0x00,   // ±40mT (仅A1版本)
    TMAG3001_RANGE_80mT = 0x01,   // ±80mT (仅A1版本)
    TMAG3001_RANGE_120mT = 0x00,  // ±120mT (仅A2版本)
    TMAG3001_RANGE_240mT = 0x01   // ±240mT (仅A2版本)
} TMAG3001_Range_t;

// 磁场通道选择
typedef enum {
    TMAG3001_CH_NONE = 0x00,
    TMAG3001_CH_X = 0x01,
    TMAG3001_CH_Y = 0x02,
    TMAG3001_CH_XY = 0x03,
    TMAG3001_CH_Z = 0x04,
    TMAG3001_CH_XZ = 0x05,
    TMAG3001_CH_YZ = 0x06,
    TMAG3001_CH_XYZ = 0x07
} TMAG3001_MagChannel_t;

// 设备版本
typedef enum {
    TMAG3001_VERSION_A1 = 0,  // ±40mT/±80mT
    TMAG3001_VERSION_A2 = 1   // ±120mT/±240mT
} TMAG3001_Version_t;

// 转换平均次数
typedef enum {
    TMAG3001_AVG_1X = 0x00,
    TMAG3001_AVG_2X = 0x01,
    TMAG3001_AVG_4X = 0x02,
    TMAG3001_AVG_8X = 0x03,
    TMAG3001_AVG_16X = 0x04,
    TMAG3001_AVG_32X = 0x05
} TMAG3001_ConvAvg_t;

// 磁场增益配置 (MAG_GAIN_CONFIG)
// 注意：TMAG3001硬件只支持0.5x-2.0x范围
typedef enum {
    TMAG3001_GAIN_0_5 = 0x00,  // 0.5x gain
    TMAG3001_GAIN_0_6 = 0x01,  // 0.6x gain
    TMAG3001_GAIN_0_7 = 0x02,  // 0.7x gain
    TMAG3001_GAIN_0_8 = 0x03,  // 0.8x gain
    TMAG3001_GAIN_0_9 = 0x04,  // 0.9x gain
    TMAG3001_GAIN_1_0 = 0x05,  // 1.0x gain (default)
    TMAG3001_GAIN_1_1 = 0x06,  // 1.1x gain
    TMAG3001_GAIN_1_2 = 0x07,  // 1.2x gain
    TMAG3001_GAIN_1_3 = 0x08,  // 1.3x gain
    TMAG3001_GAIN_1_4 = 0x09,  // 1.4x gain
    TMAG3001_GAIN_1_5 = 0x0A,  // 1.5x gain
    TMAG3001_GAIN_1_6 = 0x0B,  // 1.6x gain
    TMAG3001_GAIN_1_7 = 0x0C,  // 1.7x gain
    TMAG3001_GAIN_1_8 = 0x0D,  // 1.8x gain
    TMAG3001_GAIN_1_9 = 0x0E,  // 1.9x gain
    TMAG3001_GAIN_2_0 = 0x0F   // 2.0x gain (最大硬件增益)
} TMAG3001_MagGain_t;

// 灵敏度常数 (LSB/mT)
#define TMAG3001_SENSITIVITY_A1_40mT   885.0f
#define TMAG3001_SENSITIVITY_A1_80mT   446.0f
#define TMAG3001_SENSITIVITY_A2_120mT  296.0f
#define TMAG3001_SENSITIVITY_A2_240mT  148.0f

// 设备句柄
typedef struct {
    uint8_t i2c_id;              // I2C外设编号, 0=I2C1, 1=I2C2
    uint8_t i2c_addr;            // I2C设备地址(7位)
    TMAG3001_Version_t version;  // 设备版本
    TMAG3001_Range_t range;      // 测量范围
    TMAG3001_MagGain_t gain;     // 增益设置
} TMAG3001_Handle_t;

// 测量结果结构
typedef struct {
    int16_t x_raw;
    int16_t y_raw;
    int16_t z_raw;
    float x_mT;
    float y_mT;
    float z_mT;
} TMAG3001_Result_t;

// 函数声明
uint8_t TMAG3001_Init(TMAG3001_Handle_t *dev, uint8_t i2c_id, uint8_t i2c_addr, TMAG3001_Version_t version);
uint8_t TMAG3001_ReadID(TMAG3001_Handle_t *dev, uint8_t *device_id, uint16_t *manufacturer_id);
uint8_t TMAG3001_SetOperatingMode(TMAG3001_Handle_t *dev, TMAG3001_OperatingMode_t mode);
uint8_t TMAG3001_SetMagChannels(TMAG3001_Handle_t *dev, TMAG3001_MagChannel_t channels);
uint8_t TMAG3001_SetRange(TMAG3001_Handle_t *dev, TMAG3001_Range_t range);
uint8_t TMAG3001_SetConvAvg(TMAG3001_Handle_t *dev, TMAG3001_ConvAvg_t avg);
uint8_t TMAG3001_SetMagGain(TMAG3001_Handle_t *dev, TMAG3001_MagGain_t gain);
uint8_t TMAG3001_TriggerConversion(TMAG3001_Handle_t *dev);
uint8_t TMAG3001_ReadConvStatus(TMAG3001_Handle_t *dev, uint8_t *status);
uint8_t TMAG3001_ReadMagneticData(TMAG3001_Handle_t *dev, TMAG3001_Result_t *result);

// 扩展测量范围的辅助函数
uint8_t TMAG3001_ReadMagneticDataWithOversampling(TMAG3001_Handle_t *dev, TMAG3001_Result_t *result, uint8_t oversample_count);
float TMAG3001_ApplyDigitalGain(float raw_value, float digital_gain);

// Force compilation check - gain support added
#endif // __TMAG3001_H
