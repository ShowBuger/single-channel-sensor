#ifndef __AK09974C_H
#define __AK09974C_H

#include <stdint.h>

/**
 * @file  ak09974c.h
 * @brief AKM AK09974C 3D霍尔磁传感器驱动（I2C模式）
 *
 * 依据 AK09974C 数据手册 250800026-E-00 (2025/09)，docs/ak09974c-en-datasheet-myakm.pdf
 *
 * 使用要点：
 *  - 上电后必须先写 VIO(20h)，否则其他寄存器无法访问（手册 9.1 / 12.3.5）。
 *  - 设置寄存器(31h~3Bh)只能在 Power-down(RUN=0) 时修改（手册 10.2.1.4）。
 *  - RUN(3Ch) 必须单独写入，不能用地址自增连写（手册 9.1 注**）。
 *  - 读完 11h~14h 的测量数据或访问 31h~3Ch 后 DRDY 清零（手册 10.3）。
 */

// I2C 静态地址：手册 11.1.1.4.1 图11.3（7位地址）
#define AK09974C_I2C_ADDR_CASE1         0x21  // IF1接SDA、IF2接SCL
#define AK09974C_I2C_ADDR_CASE2         0x22  // IF1接SCL、IF2接SDA
#define AK09974C_I2C_ADDR_BROADCAST     0x23  // 两种接法通用，仅可写

// 寄存器地址：手册表12.1
#define AK09974C_REG_WIA                0x00  // 4字节: 公司ID、设备ID、保留
#define AK09974C_REG_ST                 0x10  // 1字节: 状态
#define AK09974C_REG_ST_XYZ             0x11  // 7字节: ST + BX + BY + BZ
#define AK09974C_REG_ST_L16             0x12  // 3字节: ST + L[15:0]
#define AK09974C_REG_ST_L8              0x13  // 2字节: ST + LS[7:0]
#define AK09974C_REG_ST_CUSTOM          0x14  // 1~9字节: ST + MSET.CUS选择的数据
#define AK09974C_REG_VIO                0x20  // VIO电平设置（必须最先写）
#define AK09974C_REG_SRST               0x30  // 软复位
#define AK09974C_REG_RSV1               0x31  // 保留(2字节，固定0)
#define AK09974C_REG_RSV2               0x32
#define AK09974C_REG_RSV3               0x33
#define AK09974C_REG_THR_X              0x34  // 4字节: BOPX + BRPX
#define AK09974C_REG_THR_Y              0x35  // 4字节: BOPY + BRPY
#define AK09974C_REG_THR_Z              0x36  // 4字节: BOPZ + BRPZ
#define AK09974C_REG_THR_L              0x37  // 4字节: BOPL + BRPL
#define AK09974C_REG_SWSET              0x38  // 2字节: 开关功能设置
#define AK09974C_REG_LSET               0x39  // 线性/角度输出设置
#define AK09974C_REG_MSET               0x3A  // 测量设置(CUS/STEST/WIDE/DRI)
#define AK09974C_REG_MCYC               0x3B  // 测量周期
#define AK09974C_REG_RUN                0x3C  // 测量启动

// WIA：手册 12.3.1
#define AK09974C_COMPANY_ID             0x48
#define AK09974C_DEVICE_ID              0xC2

// ST 状态位：手册 12.3.2
#define AK09974C_ST_DRDY                0x01U
#define AK09974C_ST_SWL                 0x02U
#define AK09974C_ST_SWZ                 0x04U
#define AK09974C_ST_SWY                 0x08U
#define AK09974C_ST_SWX                 0x10U
#define AK09974C_ST_ERR                 0x20U  // 磁场超量程
#define AK09974C_ST_DOR                 0x40U  // 连续模式下数据被跳过

// VIO：手册 12.3.5。VIO[1:0]=01b 为1.8V，10b 为1.2V（9.1节写作"10h"系指二进制10）。
#define AK09974C_VIO_1V8                0x01U
#define AK09974C_VIO_1V2                0x02U

// SRST / RUN
#define AK09974C_SRST_RESET             0x01U
#define AK09974C_RUN_START              0x01U
#define AK09974C_RUN_STOP               0x00U

// MSET 位定义：手册 12.3.11
#define AK09974C_MSET_DRI_MASK          0x03U
#define AK09974C_MSET_WIDE              0x04U
#define AK09974C_MSET_STEST             0x08U
#define AK09974C_MSET_CUS_SHIFT         4
#define AK09974C_MSET_CUS_MASK          0xF0U
#define AK09974C_CUS_Z                  0x01U
#define AK09974C_CUS_Y                  0x02U
#define AK09974C_CUS_X                  0x04U
#define AK09974C_CUS_LIN                0x08U

// SWSET 位定义：手册 12.3.9（16位，高字节在前）
#define AK09974C_SWSET_IBI_SWL_N        0x0001U
#define AK09974C_SWSET_IBI_SWL_P        0x0002U
#define AK09974C_SWSET_IBI_SWZ_N        0x0004U
#define AK09974C_SWSET_IBI_SWZ_P        0x0008U
#define AK09974C_SWSET_IBI_SWY_N        0x0010U
#define AK09974C_SWSET_IBI_SWY_P        0x0020U
#define AK09974C_SWSET_IBI_SWX_N        0x0040U
#define AK09974C_SWSET_IBI_SWX_P        0x0080U
#define AK09974C_SWSET_IBI_DRDY         0x0100U
#define AK09974C_SWSET_IBI_ERR          0x0200U
#define AK09974C_SWSET_TH_SEL_REL       0x0800U  // 0=绝对阈值, 1=相对阈值
#define AK09974C_SWSET_SWL_POL          0x1000U
#define AK09974C_SWSET_SWZ_POL          0x2000U
#define AK09974C_SWSET_SWY_POL          0x4000U
#define AK09974C_SWSET_SWX_POL          0x8000U

// LSET 位定义：手册 12.3.10
#define AK09974C_LSET_D_MASK            0x03U
#define AK09974C_LSET_D_POL             0x04U
#define AK09974C_LSET_N_SHIFT           3
#define AK09974C_LSET_N_MASK            0x18U
#define AK09974C_LSET_N_POL             0x20U
#define AK09974C_LSET_ROT_LIN           0x40U

// MCYC：00h=单次测量，01h~FFh=连续测量，周期 = MCYC * 0.5ms（手册 12.3.12）
#define AK09974C_MCYC_SINGLE            0x00U

// 灵敏度 (µT/LSB)：手册 8.3.1 / 8.3.2 典型值
#define AK09974C_SENSITIVITY_NORMAL_uT  1.1f   // ±36mT
#define AK09974C_SENSITIVITY_WIDE_uT    6.1f   // XY ±78mT, Z ±200mT

// 自检判定范围：手册 10.5
#define AK09974C_SELFTEST_XY_MIN        (-15)
#define AK09974C_SELFTEST_XY_MAX        15
#define AK09974C_SELFTEST_Z_MIN         25
#define AK09974C_SELFTEST_Z_MAX         75

// 量程
typedef enum {
    AK09974C_RANGE_NORMAL = 0,  // X/Y/Z ±36mT, 1.1µT/LSB
    AK09974C_RANGE_WIDE   = 1   // X/Y ±78mT, Z ±200mT, 6.1µT/LSB
} AK09974C_Range_t;

// 驱动模式(精度/功耗)：DRI[1:0]
typedef enum {
    AK09974C_DRIVE_SUPER_LOW_NOISE = 0,  // 8.162ms, 最小周期MCYC=0x11
    AK09974C_DRIVE_LOW_NOISE       = 1,  // 2.927ms, 最小周期MCYC=0x06
    AK09974C_DRIVE_NORMAL          = 2,  // 0.658ms, 最小周期MCYC=0x02（复位值）
    AK09974C_DRIVE_LOW_POWER       = 3   // 0.418ms, 最小周期MCYC=0x01
} AK09974C_Drive_t;

// 阈值通道
typedef enum {
    AK09974C_AXIS_X = 0,
    AK09974C_AXIS_Y = 1,
    AK09974C_AXIS_Z = 2,
    AK09974C_AXIS_L = 3   // 线性/角度数据
} AK09974C_Axis_t;

// 设备句柄
typedef struct {
    uint8_t i2c_id;              // I2C外设编号
    uint8_t i2c_addr;            // I2C设备地址(7位)
    uint8_t vio;                 // VIO设置 AK09974C_VIO_*
    AK09974C_Range_t range;      // 当前量程
    AK09974C_Drive_t drive;      // 当前驱动模式
    uint8_t mcyc;                // 当前测量周期，0=单次
    uint8_t running;             // RUN位镜像(连续模式下为1)
    uint8_t last_status;         // 最近一次读到的ST
} AK09974C_Handle_t;

// 测量结果结构
typedef struct {
    uint8_t status;              // 与数据同批读出的ST
    int16_t x_raw;
    int16_t y_raw;
    int16_t z_raw;
    float x_uT;
    float y_uT;
    float z_uT;
} AK09974C_Result_t;

// 初始化与识别
uint8_t AK09974C_Init(AK09974C_Handle_t *dev, uint8_t i2c_id, uint8_t i2c_addr, uint8_t vio);
uint8_t AK09974C_SetVIO(AK09974C_Handle_t *dev, uint8_t vio);
uint8_t AK09974C_SoftReset(AK09974C_Handle_t *dev);
uint8_t AK09974C_ReadID(AK09974C_Handle_t *dev, uint8_t *company_id, uint8_t *device_id);

// 测量设置（需在Power-down下调用）
uint8_t AK09974C_SetRange(AK09974C_Handle_t *dev, AK09974C_Range_t range);
uint8_t AK09974C_SetDrive(AK09974C_Handle_t *dev, AK09974C_Drive_t drive);
uint8_t AK09974C_SetCustomOutput(AK09974C_Handle_t *dev, uint8_t cus_mask);
uint8_t AK09974C_ReadMSET(AK09974C_Handle_t *dev, uint8_t *mset);
uint8_t AK09974C_SetMeasureCycle(AK09974C_Handle_t *dev, uint8_t mcyc);
uint8_t AK09974C_MinMeasureCycle(AK09974C_Drive_t drive);

// 测量控制
uint8_t AK09974C_Start(AK09974C_Handle_t *dev);
uint8_t AK09974C_Stop(AK09974C_Handle_t *dev);
uint8_t AK09974C_ReadStatus(AK09974C_Handle_t *dev, uint8_t *status);
uint8_t AK09974C_ReadMagneticData(AK09974C_Handle_t *dev, AK09974C_Result_t *result);
uint8_t AK09974C_ReadLinear16(AK09974C_Handle_t *dev, uint8_t *status, int16_t *linear);
uint8_t AK09974C_ReadLinear8(AK09974C_Handle_t *dev, uint8_t *status, int8_t *linear);
uint8_t AK09974C_ReadCustom(AK09974C_Handle_t *dev, uint8_t *data, uint8_t len);
float   AK09974C_GetSensitivity(const AK09974C_Handle_t *dev);

// 开关/阈值/线性输出（需在Power-down下调用）
uint8_t AK09974C_SetThreshold(AK09974C_Handle_t *dev, AK09974C_Axis_t axis, int16_t bop, int16_t brp);
uint8_t AK09974C_GetThreshold(AK09974C_Handle_t *dev, AK09974C_Axis_t axis, int16_t *bop, int16_t *brp);
uint8_t AK09974C_SetSwitch(AK09974C_Handle_t *dev, uint16_t swset);
uint8_t AK09974C_GetSwitch(AK09974C_Handle_t *dev, uint16_t *swset);
uint8_t AK09974C_SetLinear(AK09974C_Handle_t *dev, uint8_t lset);
uint8_t AK09974C_GetLinear(AK09974C_Handle_t *dev, uint8_t *lset);

// 自检：返回0=通过，result可为NULL
uint8_t AK09974C_SelfTest(AK09974C_Handle_t *dev, AK09974C_Result_t *result);

#endif // __AK09974C_H
