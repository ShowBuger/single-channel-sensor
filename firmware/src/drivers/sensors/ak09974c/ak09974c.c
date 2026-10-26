#include "ak09974c.h"
#include "../../bsp/bsp_i2c_soft.h"
#include "../../bsp/bsp_delay.h"
#include <stddef.h>

/* 设置寄存器只能在 Power-down 下修改，连续测量运行中直接拒绝。 */
#define AK09974C_ERR_RUNNING    4
#define AK09974C_ERR_PARAM      5
#define AK09974C_ERR_TIMEOUT    6

/**
 * @brief 内部函数：连续写入一个寄存器的多个字节
 * @param dev 设备句柄
 * @param reg_addr 寄存器地址
 * @param data 数据
 * @param len 字节数（多字节寄存器按手册字节宽度一次写完）
 * @return 0=成功, 非0=失败
 */
static uint8_t AK09974C_WriteRegs(AK09974C_Handle_t *dev, uint8_t reg_addr, const uint8_t *data, uint8_t len)
{
    uint8_t i;

    IIC_Start(dev->i2c_id);
    I2C_WriteByte(dev->i2c_id, (dev->i2c_addr << 1) | 0x00);
    if (I2C_WaitAck(dev->i2c_id)) { IIC_Stop(dev->i2c_id); return 1; }

    I2C_WriteByte(dev->i2c_id, reg_addr);
    if (I2C_WaitAck(dev->i2c_id)) { IIC_Stop(dev->i2c_id); return 2; }

    for (i = 0; i < len; i++) {
        I2C_WriteByte(dev->i2c_id, data[i]);
        if (I2C_WaitAck(dev->i2c_id)) { IIC_Stop(dev->i2c_id); return 3; }
    }

    IIC_Stop(dev->i2c_id);
    return 0;
}

static uint8_t AK09974C_WriteReg(AK09974C_Handle_t *dev, uint8_t reg_addr, uint8_t data)
{
    return AK09974C_WriteRegs(dev, reg_addr, &data, 1);
}

/**
 * @brief 内部函数：随机地址读（手册 11.1.1.5.2.2）
 * @param dev 设备句柄
 * @param reg_addr 寄存器地址
 * @param data 数据输出
 * @param len 读取字节数
 * @return 0=成功, 非0=失败
 */
static uint8_t AK09974C_ReadRegs(AK09974C_Handle_t *dev, uint8_t reg_addr, uint8_t *data, uint8_t len)
{
    uint8_t i;

    IIC_Start(dev->i2c_id);
    I2C_WriteByte(dev->i2c_id, (dev->i2c_addr << 1) | 0x00);
    if (I2C_WaitAck(dev->i2c_id)) { IIC_Stop(dev->i2c_id); return 1; }

    I2C_WriteByte(dev->i2c_id, reg_addr);
    if (I2C_WaitAck(dev->i2c_id)) { IIC_Stop(dev->i2c_id); return 2; }

    IIC_Start(dev->i2c_id);
    I2C_WriteByte(dev->i2c_id, (dev->i2c_addr << 1) | 0x01);
    if (I2C_WaitAck(dev->i2c_id)) { IIC_Stop(dev->i2c_id); return 3; }

    for (i = 0; i < len; i++) {
        data[i] = I2C_ReadByte(dev->i2c_id);
        // 最后一个字节发送NACK
        I2C_SendAck(dev->i2c_id, (i < len - 1) ? 0 : 1);
    }

    IIC_Stop(dev->i2c_id);
    return 0;
}

static uint8_t AK09974C_UpdateMSET(AK09974C_Handle_t *dev, uint8_t mask, uint8_t value)
{
    uint8_t reg_val;
    uint8_t result;

    if (dev->running) return AK09974C_ERR_RUNNING;
    result = AK09974C_ReadRegs(dev, AK09974C_REG_MSET, &reg_val, 1);
    if (result) return result;
    reg_val = (uint8_t)((reg_val & ~mask) | (value & mask));
    return AK09974C_WriteReg(dev, AK09974C_REG_MSET, reg_val);
}

/**
 * @brief 初始化AK09974C设备
 * @param dev 设备句柄
 * @param i2c_id I2C外设编号
 * @param i2c_addr I2C地址(7位)
 * @param vio VIO设置 AK09974C_VIO_1V8 / AK09974C_VIO_1V2
 * @return 0=成功, 100=公司ID错误, 101=设备ID错误, 其他=通信失败
 * @note 初始化后处于Power-down、单次测量、常规量程、Normal驱动（芯片复位值）
 */
uint8_t AK09974C_Init(AK09974C_Handle_t *dev, uint8_t i2c_id, uint8_t i2c_addr, uint8_t vio)
{
    uint8_t result;
    uint8_t company_id, device_id;

    dev->i2c_id = i2c_id;
    dev->i2c_addr = i2c_addr;
    dev->running = 0;
    dev->last_status = 0;

    // 1. 写VIO：上电后其他寄存器在此之前不可访问
    result = AK09974C_SetVIO(dev, vio);
    if (result) return result;
    Delay_us(100);  // Twait >= 100µs（手册 8.2）

    // 2. 软复位：MCU复位后芯片可能仍在连续测量，回到Power-down并恢复默认值（VIO保留）
    result = AK09974C_SoftReset(dev);
    if (result) return result;

    // 3. 校验ID
    result = AK09974C_ReadID(dev, &company_id, &device_id);
    if (result) return result;
    if (company_id != AK09974C_COMPANY_ID) return 100;
    if (device_id != AK09974C_DEVICE_ID) return 101;

    return 0;
}

/**
 * @brief 设置VIO电平（必须在访问其他设置寄存器之前写入）
 */
uint8_t AK09974C_SetVIO(AK09974C_Handle_t *dev, uint8_t vio)
{
    uint8_t result;

    if (vio != AK09974C_VIO_1V8 && vio != AK09974C_VIO_1V2) return AK09974C_ERR_PARAM;
    result = AK09974C_WriteReg(dev, AK09974C_REG_VIO, vio);
    if (result == 0) dev->vio = vio;
    return result;
}

/**
 * @brief 软复位：除VIO外所有寄存器恢复默认，进入Power-down
 */
uint8_t AK09974C_SoftReset(AK09974C_Handle_t *dev)
{
    uint8_t result = AK09974C_WriteReg(dev, AK09974C_REG_SRST, AK09974C_SRST_RESET);
    if (result) return result;
    Delay_ms(1);

    dev->running = 0;
    dev->range = AK09974C_RANGE_NORMAL;
    dev->drive = AK09974C_DRIVE_NORMAL;
    dev->mcyc = AK09974C_MCYC_SINGLE;
    return 0;
}

/**
 * @brief 读取公司ID和设备ID
 */
uint8_t AK09974C_ReadID(AK09974C_Handle_t *dev, uint8_t *company_id, uint8_t *device_id)
{
    uint8_t data[2];
    uint8_t result = AK09974C_ReadRegs(dev, AK09974C_REG_WIA, data, 2);
    if (result) return result;
    *company_id = data[0];
    *device_id = data[1];
    return 0;
}

/**
 * @brief 设置量程(MSET.WIDE)
 */
uint8_t AK09974C_SetRange(AK09974C_Handle_t *dev, AK09974C_Range_t range)
{
    uint8_t result;

    if (range > AK09974C_RANGE_WIDE) return AK09974C_ERR_PARAM;
    result = AK09974C_UpdateMSET(dev, AK09974C_MSET_WIDE, range ? AK09974C_MSET_WIDE : 0);
    if (result == 0) dev->range = range;
    return result;
}

/**
 * @brief 设置驱动模式(MSET.DRI)
 * @note 已配置的连续测量周期若小于该驱动的最小周期则拒绝，先调整MCYC
 */
uint8_t AK09974C_SetDrive(AK09974C_Handle_t *dev, AK09974C_Drive_t drive)
{
    uint8_t result;

    if (drive > AK09974C_DRIVE_LOW_POWER) return AK09974C_ERR_PARAM;
    if (dev->mcyc != AK09974C_MCYC_SINGLE && dev->mcyc < AK09974C_MinMeasureCycle(drive))
        return AK09974C_ERR_PARAM;
    result = AK09974C_UpdateMSET(dev, AK09974C_MSET_DRI_MASK, (uint8_t)drive);
    if (result == 0) dev->drive = drive;
    return result;
}

/**
 * @brief 设置14h CUSTOM寄存器输出内容(MSET.CUS[3:0])
 * @param cus_mask AK09974C_CUS_* 组合
 */
uint8_t AK09974C_SetCustomOutput(AK09974C_Handle_t *dev, uint8_t cus_mask)
{
    if (cus_mask > 0x0F) return AK09974C_ERR_PARAM;
    return AK09974C_UpdateMSET(dev, AK09974C_MSET_CUS_MASK, (uint8_t)(cus_mask << AK09974C_MSET_CUS_SHIFT));
}

uint8_t AK09974C_ReadMSET(AK09974C_Handle_t *dev, uint8_t *mset)
{
    return AK09974C_ReadRegs(dev, AK09974C_REG_MSET, mset, 1);
}

/**
 * @brief 各驱动模式允许的最小连续测量周期（手册表10.2）
 */
uint8_t AK09974C_MinMeasureCycle(AK09974C_Drive_t drive)
{
    static const uint8_t min_mcyc[4] = { 0x11, 0x06, 0x02, 0x01 };
    return min_mcyc[drive & 0x03];
}

/**
 * @brief 设置测量周期
 * @param mcyc 0=单次测量；1~255=连续测量，周期 = mcyc * 0.5ms
 */
uint8_t AK09974C_SetMeasureCycle(AK09974C_Handle_t *dev, uint8_t mcyc)
{
    uint8_t result;

    if (dev->running) return AK09974C_ERR_RUNNING;
    if (mcyc != AK09974C_MCYC_SINGLE && mcyc < AK09974C_MinMeasureCycle(dev->drive))
        return AK09974C_ERR_PARAM;
    result = AK09974C_WriteReg(dev, AK09974C_REG_MCYC, mcyc);
    if (result == 0) dev->mcyc = mcyc;
    return result;
}

/**
 * @brief 写RUN=1：单次模式启动一次测量，连续模式开始周期测量
 * @note RUN必须单独写入（手册 9.1）
 */
uint8_t AK09974C_Start(AK09974C_Handle_t *dev)
{
    uint8_t result = AK09974C_WriteReg(dev, AK09974C_REG_RUN, AK09974C_RUN_START);
    if (result == 0) dev->running = (dev->mcyc != AK09974C_MCYC_SINGLE);
    return result;
}

/**
 * @brief 写RUN=0：进入Power-down，之后才能修改设置寄存器
 */
uint8_t AK09974C_Stop(AK09974C_Handle_t *dev)
{
    uint8_t result = AK09974C_WriteReg(dev, AK09974C_REG_RUN, AK09974C_RUN_STOP);
    if (result == 0) dev->running = 0;
    return result;
}

/**
 * @brief 读取ST(10h)，不影响DRDY
 */
uint8_t AK09974C_ReadStatus(AK09974C_Handle_t *dev, uint8_t *status)
{
    uint8_t result = AK09974C_ReadRegs(dev, AK09974C_REG_ST, status, 1);
    if (result == 0) dev->last_status = *status;
    return result;
}

float AK09974C_GetSensitivity(const AK09974C_Handle_t *dev)
{
    return (dev->range == AK09974C_RANGE_WIDE) ? AK09974C_SENSITIVITY_WIDE_uT
                                               : AK09974C_SENSITIVITY_NORMAL_uT;
}

/**
 * @brief 读取ST+XYZ(11h, 7字节)，读完后DRDY清零
 * @note 超量程时ST.ERR=1，数据为限幅值，由调用方决定是否丢弃
 */
uint8_t AK09974C_ReadMagneticData(AK09974C_Handle_t *dev, AK09974C_Result_t *result)
{
    uint8_t data[7];
    float sensitivity;
    uint8_t status = AK09974C_ReadRegs(dev, AK09974C_REG_ST_XYZ, data, 7);
    if (status) return status;

    dev->last_status = data[0];
    result->status = data[0];
    result->x_raw = (int16_t)((data[1] << 8) | data[2]);
    result->y_raw = (int16_t)((data[3] << 8) | data[4]);
    result->z_raw = (int16_t)((data[5] << 8) | data[6]);

    sensitivity = AK09974C_GetSensitivity(dev);
    result->x_uT = (float)result->x_raw * sensitivity;
    result->y_uT = (float)result->y_raw * sensitivity;
    result->z_uT = (float)result->z_raw * sensitivity;
    return 0;
}

/**
 * @brief 读取ST+L[15:0](12h)，0.5°/LSB，有效范围±360
 */
uint8_t AK09974C_ReadLinear16(AK09974C_Handle_t *dev, uint8_t *status, int16_t *linear)
{
    uint8_t data[3];
    uint8_t result = AK09974C_ReadRegs(dev, AK09974C_REG_ST_L16, data, 3);
    if (result) return result;
    dev->last_status = data[0];
    if (status) *status = data[0];
    *linear = (int16_t)((data[1] << 8) | data[2]);
    return 0;
}

/**
 * @brief 读取ST+LS[7:0](13h)，1°/LSB
 */
uint8_t AK09974C_ReadLinear8(AK09974C_Handle_t *dev, uint8_t *status, int8_t *linear)
{
    uint8_t data[2];
    uint8_t result = AK09974C_ReadRegs(dev, AK09974C_REG_ST_L8, data, 2);
    if (result) return result;
    dev->last_status = data[0];
    if (status) *status = data[0];
    *linear = (int8_t)data[1];
    return 0;
}

/**
 * @brief 读取ST+CUSTOM(14h)，内容由MSET.CUS决定，左对齐输出
 * @param len 1~9字节（含ST）
 */
uint8_t AK09974C_ReadCustom(AK09974C_Handle_t *dev, uint8_t *data, uint8_t len)
{
    uint8_t result;

    if (len == 0 || len > 9) return AK09974C_ERR_PARAM;
    result = AK09974C_ReadRegs(dev, AK09974C_REG_ST_CUSTOM, data, len);
    if (result == 0) dev->last_status = data[0];
    return result;
}

/**
 * @brief 设置开关功能阈值(34h~37h)
 * @param bop 动作阈值，格式同测量数据；X/Y/Z低8位芯片固定为0
 * @param brp 释放阈值，应小于bop
 */
uint8_t AK09974C_SetThreshold(AK09974C_Handle_t *dev, AK09974C_Axis_t axis, int16_t bop, int16_t brp)
{
    uint8_t data[4];

    if (dev->running) return AK09974C_ERR_RUNNING;
    if (axis > AK09974C_AXIS_L || bop <= brp) return AK09974C_ERR_PARAM;
    data[0] = (uint8_t)((uint16_t)bop >> 8);
    data[1] = (uint8_t)bop;
    data[2] = (uint8_t)((uint16_t)brp >> 8);
    data[3] = (uint8_t)brp;
    return AK09974C_WriteRegs(dev, (uint8_t)(AK09974C_REG_THR_X + axis), data, 4);
}

uint8_t AK09974C_GetThreshold(AK09974C_Handle_t *dev, AK09974C_Axis_t axis, int16_t *bop, int16_t *brp)
{
    uint8_t data[4];
    uint8_t result;

    if (axis > AK09974C_AXIS_L) return AK09974C_ERR_PARAM;
    result = AK09974C_ReadRegs(dev, (uint8_t)(AK09974C_REG_THR_X + axis), data, 4);
    if (result) return result;
    *bop = (int16_t)((data[0] << 8) | data[1]);
    *brp = (int16_t)((data[2] << 8) | data[3]);
    return 0;
}

/**
 * @brief 设置开关功能(38h)，位定义见 AK09974C_SWSET_*
 */
uint8_t AK09974C_SetSwitch(AK09974C_Handle_t *dev, uint16_t swset)
{
    uint8_t data[2];

    if (dev->running) return AK09974C_ERR_RUNNING;
    data[0] = (uint8_t)((swset >> 8) & ~0x04U);  // SWSET[10]固定为0
    data[1] = (uint8_t)swset;
    return AK09974C_WriteRegs(dev, AK09974C_REG_SWSET, data, 2);
}

uint8_t AK09974C_GetSwitch(AK09974C_Handle_t *dev, uint16_t *swset)
{
    uint8_t data[2];
    uint8_t result = AK09974C_ReadRegs(dev, AK09974C_REG_SWSET, data, 2);
    if (result) return result;
    *swset = (uint16_t)((data[0] << 8) | data[1]);
    return 0;
}

/**
 * @brief 设置线性/角度输出计算(39h)，位定义见 AK09974C_LSET_*
 */
uint8_t AK09974C_SetLinear(AK09974C_Handle_t *dev, uint8_t lset)
{
    if (dev->running) return AK09974C_ERR_RUNNING;
    return AK09974C_WriteReg(dev, AK09974C_REG_LSET, lset & 0x7FU);
}

uint8_t AK09974C_GetLinear(AK09974C_Handle_t *dev, uint8_t *lset)
{
    return AK09974C_ReadRegs(dev, AK09974C_REG_LSET, lset, 1);
}

/**
 * @brief 自检（手册 10.5）：内部磁源激励后单次测量并检查XYZ范围
 * @param result 可为NULL；非NULL时返回自检原始数据
 * @return 0=通过, 7=数据超出判定范围, 其他=通信/超时错误
 * @note 需在Power-down下调用；结束后恢复原MSET/MCYC，保持Power-down
 */
uint8_t AK09974C_SelfTest(AK09974C_Handle_t *dev, AK09974C_Result_t *result)
{
    AK09974C_Result_t local;
    AK09974C_Result_t *r = result ? result : &local;
    uint8_t saved_mcyc = dev->mcyc;
    uint8_t status = 0;
    uint8_t ret;
    uint8_t i;

    if (dev->running) return AK09974C_ERR_RUNNING;

    // 自检仅在单次测量模式下有效
    ret = AK09974C_WriteReg(dev, AK09974C_REG_MCYC, AK09974C_MCYC_SINGLE);
    if (ret == 0) ret = AK09974C_UpdateMSET(dev, AK09974C_MSET_STEST, AK09974C_MSET_STEST);
    if (ret == 0) ret = AK09974C_WriteReg(dev, AK09974C_REG_RUN, AK09974C_RUN_START);

    // 自检固定使用超低噪声驱动，最长约9ms
    for (i = 0; ret == 0 && i < 20; i++) {
        Delay_ms(1);
        ret = AK09974C_ReadStatus(dev, &status);
        if (ret == 0 && (status & AK09974C_ST_DRDY)) break;
    }
    if (ret == 0 && !(status & AK09974C_ST_DRDY)) ret = AK09974C_ERR_TIMEOUT;
    if (ret == 0) {
        uint8_t data[7];
        ret = AK09974C_ReadRegs(dev, AK09974C_REG_ST_XYZ, data, 7);
        if (ret == 0) {
            r->status = data[0];
            r->x_raw = (int16_t)((data[1] << 8) | data[2]);
            r->y_raw = (int16_t)((data[3] << 8) | data[4]);
            r->z_raw = (int16_t)((data[5] << 8) | data[6]);
            r->x_uT = (float)r->x_raw * AK09974C_SENSITIVITY_WIDE_uT;
            r->y_uT = (float)r->y_raw * AK09974C_SENSITIVITY_WIDE_uT;
            r->z_uT = (float)r->z_raw * AK09974C_SENSITIVITY_WIDE_uT;
        }
    }

    // 无论结果如何都恢复原设置
    {
        uint8_t restore = AK09974C_UpdateMSET(dev, AK09974C_MSET_STEST, 0);
        if (restore == 0) restore = AK09974C_WriteReg(dev, AK09974C_REG_MCYC, saved_mcyc);
        if (ret == 0) ret = restore;
    }
    if (ret) return ret;

    if (r->x_raw < AK09974C_SELFTEST_XY_MIN || r->x_raw > AK09974C_SELFTEST_XY_MAX ||
        r->y_raw < AK09974C_SELFTEST_XY_MIN || r->y_raw > AK09974C_SELFTEST_XY_MAX ||
        r->z_raw < AK09974C_SELFTEST_Z_MIN  || r->z_raw > AK09974C_SELFTEST_Z_MAX)
        return 7;
    return 0;
}
