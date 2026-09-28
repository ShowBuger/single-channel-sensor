#ifndef MLX90393_H
#define MLX90393_H

#include <stdint.h>

// 基础 I2C 地址（修改为 0x10）
#define MLX90393_I2C_BASE_ADDR   0x10

// 命令定义
#define MLX90393_CMD_EXIT             0x80
#define MLX90393_CMD_START_BURST      0x10
#define MLX90393_CMD_WAKE_ON_CHANGE   0x20
#define MLX90393_CMD_START_MEASUREMENT 0x30
#define MLX90393_CMD_READ_MEASUREMENT  0x40
#define MLX90393_CMD_READ_REGISTER     0x50
#define MLX90393_CMD_WRITE_REGISTER    0x60
#define MLX90393_CMD_MEMORY_RECALL     0xD0
#define MLX90393_CMD_MEMORY_STORE      0xE0
#define MLX90393_CMD_RESET             0xF0

// 测量标志（低4位控制温度T及 X、Y、Z 轴）
#define MLX90393_FLAG_T  0x01
#define MLX90393_FLAG_X  0x02
#define MLX90393_FLAG_Y  0x04
#define MLX90393_FLAG_Z  0x08

// 状态字节位定义 (Table 12: Status Byte Definition)
#define MLX90393_STATUS_D0           0x01  // bit 0: D[0]
#define MLX90393_STATUS_D1           0x02  // bit 1: D[1]
#define MLX90393_STATUS_RS           0x04  // bit 2: Reset status
#define MLX90393_STATUS_SED          0x08  // bit 3: Single Error Detection
#define MLX90393_STATUS_ERROR        0x10  // bit 4: Error bit
#define MLX90393_STATUS_SM_MODE      0x20  // bit 5: Single Measurement Mode
#define MLX90393_STATUS_WOC_MODE     0x40  // bit 6: Wake-on-Change Mode
#define MLX90393_STATUS_BURST_MODE   0x80  // bit 7: Burst Mode

// 错误类型定义
#define MLX90393_ERROR_NONE          0x00  // 无错误
#define MLX90393_ERROR_BIT_SET       0x01  // ERROR位被置位
#define MLX90393_ERROR_ECC           0x02  // ECC错误(内存双位错误)
#define MLX90393_ERROR_DRDY          0x03  // 在DRDY未就绪时读取数据

// BIST自检阈值定义
#define BIST_MIN_DIFF                100   // BIST前后Z轴差值最小阈值（可根据实际调整）

// 寄存器地址及掩码
#define GAIN_SEL_REG    0x00
#define GAIN_SEL_MASK   0x0070
#define GAIN_SEL_SHIFT  4

#define HALLCONF_REG    0x00
#define HALLCONF_MASK   0x000F
#define HALLCONF_SHIFT  0

#define BIST_REG        0x00
#define BIST_MASK       0x0080
#define BIST_SHIFT      7

#define TCMP_EN_REG     0x01
#define TCMP_EN_MASK    0x0400
#define TCMP_EN_SHIFT   10

#define RES_XYZ_REG     0x02
#define RES_XYZ_MASK    0x07E0
#define RES_XYZ_SHIFT   5

#define OSR_REG         0x02
#define OSR_MASK        0x0003
#define OSR_SHIFT       0

#define DIG_FLT_REG     0x02
#define DIG_FLT_MASK    0x001C
#define DIG_FLT_SHIFT   2

#define BURST_DATA_RATE_REG    0x01
#define BURST_DATA_RATE_MASK   0x007F
#define BURST_DATA_RATE_SHIFT  0

// 设备句柄
typedef struct {
    uint8_t I2C_Address;
    int DRDY_pin; // 若不使用可设为 -1
} MLX90393_Handle;

// 原始测量数据（每项占 2 字节）
typedef struct {
    uint16_t t;
    uint16_t x;
    uint16_t y;
    uint16_t z;
} MLX90393_RawData;

// 转换后的数据（温度单位 °C，磁场单位 µT）
typedef struct {
    float t;
    float x;
    float y;
    float z;
} MLX90393_Data;

/* 函数原型 */

// 状态字节错误处理函数
uint8_t error_process(uint8_t status);

// 获取状态字节中的数据字节计数(仅RR/RM命令有效)
// 返回值: 期望的响应字节数 = 2, 4, 6, 或 8
uint8_t MLX90393_getDataByteCount(uint8_t status);

// 检查传感器是否处于特定模式
uint8_t MLX90393_isBurstMode(uint8_t status);      // 检查是否处于突发模式
uint8_t MLX90393_isWOCMode(uint8_t status);        // 检查是否处于唤醒变化模式
uint8_t MLX90393_isSingleMode(uint8_t status);     // 检查是否处于单次测量模式

// 初始化（根据 A1、A0 确定 I2C 地址，并设置 DRDY 引脚）
uint8_t MLX90393_begin(MLX90393_Handle *handle, uint8_t A1, uint8_t A0, int DRDY_pin , uint8_t id);

// 发送命令，返回响应状态字节
uint8_t MLX90393_sendCommand(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t cmd);

// 寄存器读写接口
uint8_t MLX90393_readRegister(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t reg, uint16_t *data);
uint8_t MLX90393_writeRegister(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t reg, uint16_t data);

// 复位传感器
uint8_t MLX90393_reset(MLX90393_Handle *handle, uint8_t i2c_id);

// 执行内建自检(Built-In Self Test)
// 返回: 0 - 自检通过, -1 - 自检失败
int8_t MLX90393_selfTest(MLX90393_Handle *handle, uint8_t i2c_id);

// 启动单次测量（flags 用于选择 T, X, Y, Z）
uint8_t MLX90393_startMeasurement(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t flags);

// 读取测量数据（返回数据格式：Byte0 为状态，其后各轴占 2 字节）
uint8_t MLX90393_readMeasurement(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t flags, MLX90393_RawData *raw);

// 数据转换函数（使用默认配置：增益7、hallconf = 0x0C、分辨率 0、温度补偿关闭）
// 温度转换公式： T = 25 + (raw.t - 46244) / 45.2
MLX90393_Data MLX90393_convertRaw(MLX90393_RawData raw);

// 以下为配置接口，复现 Arduino 初始化时的设置
uint8_t MLX90393_setGainSel(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t gain);
uint8_t MLX90393_setResolution(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t res_x, uint8_t res_y, uint8_t res_z);
uint8_t MLX90393_setOverSampling(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t osr);
uint8_t MLX90393_setDigitalFiltering(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t flt);
uint8_t MLX90393_setTemperatureCompensation(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t enable);
uint8_t MLX90393_setBurstDataRate(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t data_rate);
uint8_t MLX90393_startBurst(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t flags);
uint8_t MLX90393_exit_mode(MLX90393_Handle *handle, uint8_t i2c_id);
#endif // MLX90393_H
