#include "tmag3001.h"
#include "../bsp/bsp_i2c_soft.h"
#include "../bsp/bsp_delay.h"
// Updated with gain support for TMAG3001

/**
 * @brief 内部函数：写入单个寄存器
 * @param dev 设备句柄
 * @param reg_addr 寄存器地址
 * @param data 要写入的数据
 * @return 0=成功, 非0=失败
 */
static uint8_t TMAG3001_WriteReg(TMAG3001_Handle_t *dev, uint8_t reg_addr, uint8_t data)
{
    uint8_t result;

    // 1. 发送起始信号
    IIC_Start(dev->i2c_id);

    // 2. 发送设备地址+写
    I2C_WriteByte(dev->i2c_id, (dev->i2c_addr << 1) | 0x00);

    // 3. 等待应答
    result = I2C_WaitAck(dev->i2c_id);
    if (result != 0) {
        IIC_Stop(dev->i2c_id);
        return 1;
    }

    // 4. 发送寄存器地址
    I2C_WriteByte(dev->i2c_id, reg_addr);

    // 5. 等待应答
    result = I2C_WaitAck(dev->i2c_id);
    if (result != 0) {
        IIC_Stop(dev->i2c_id);
        return 2;
    }

    // 6. 发送数据
    I2C_WriteByte(dev->i2c_id, data);

    // 7. 等待应答
    result = I2C_WaitAck(dev->i2c_id);
    if (result != 0) {
        IIC_Stop(dev->i2c_id);
        return 3;
    }

    // 8. 发送停止信号
    IIC_Stop(dev->i2c_id);

    return 0;
}

/**
 * @brief 内部函数：读取单个寄存器
 * @param dev 设备句柄
 * @param reg_addr 寄存器地址
 * @param data 数据输出指针
 * @return 0=成功, 非0=失败
 */
static uint8_t TMAG3001_ReadReg(TMAG3001_Handle_t *dev, uint8_t reg_addr, uint8_t *data)
{
    uint8_t result;

    // 1. 发送起始信号
    IIC_Start(dev->i2c_id);

    // 2. 发送设备地址+写
    I2C_WriteByte(dev->i2c_id, (dev->i2c_addr << 1) | 0x00);

    // 3. 等待应答
    result = I2C_WaitAck(dev->i2c_id);
    if (result != 0) {
        IIC_Stop(dev->i2c_id);
        return 1;
    }

    // 4. 发送寄存器地址
    I2C_WriteByte(dev->i2c_id, reg_addr);

    // 5. 等待应答
    result = I2C_WaitAck(dev->i2c_id);
    if (result != 0) {
        IIC_Stop(dev->i2c_id);
        return 2;
    }

    // 6. 发送重新起始信号
    IIC_Start(dev->i2c_id);

    // 7. 发送设备地址+读
    I2C_WriteByte(dev->i2c_id, (dev->i2c_addr << 1) | 0x01);

    // 8. 等待应答
    result = I2C_WaitAck(dev->i2c_id);
    if (result != 0) {
        IIC_Stop(dev->i2c_id);
        return 3;
    }

    // 9. 读取数据
    *data = I2C_ReadByte(dev->i2c_id);

    // 10. 发送非应答(NACK)
    I2C_SendAck(dev->i2c_id, 1);

    // 11. 发送停止信号
    IIC_Stop(dev->i2c_id);

    return 0;
}

/**
 * @brief 内部函数：连续读取多个寄存器
 * @param dev 设备句柄
 * @param reg_addr 起始寄存器地址
 * @param data 数据输出指针
 * @param len 读取长度
 * @return 0=成功, 非0=失败
 */
static uint8_t TMAG3001_ReadRegs(TMAG3001_Handle_t *dev, uint8_t reg_addr, uint8_t *data, uint8_t len)
{
    uint8_t result;
    uint8_t i;

    // 1. 发送起始信号
    IIC_Start(dev->i2c_id);

    // 2. 发送设备地址+写
    I2C_WriteByte(dev->i2c_id, (dev->i2c_addr << 1) | 0x00);

    // 3. 等待应答
    result = I2C_WaitAck(dev->i2c_id);
    if (result != 0) {
        IIC_Stop(dev->i2c_id);
        return 1;
    }

    // 4. 发送寄存器地址
    I2C_WriteByte(dev->i2c_id, reg_addr);

    // 5. 等待应答
    result = I2C_WaitAck(dev->i2c_id);
    if (result != 0) {
        IIC_Stop(dev->i2c_id);
        return 2;
    }

    // 6. 发送重新起始信号
    IIC_Start(dev->i2c_id);

    // 7. 发送设备地址+读
    I2C_WriteByte(dev->i2c_id, (dev->i2c_addr << 1) | 0x01);

    // 8. 等待应答
    result = I2C_WaitAck(dev->i2c_id);
    if (result != 0) {
        IIC_Stop(dev->i2c_id);
        return 3;
    }

    // 9. 连续读取数据
    for (i = 0; i < len; i++) {
        data[i] = I2C_ReadByte(dev->i2c_id);

        if (i < len - 1) {
            // 不是最后一个字节,发送ACK
            I2C_SendAck(dev->i2c_id, 0);
        } else {
            // 最后一个字节,发送NACK
            I2C_SendAck(dev->i2c_id, 1);
        }
    }

    // 10. 发送停止信号
    IIC_Stop(dev->i2c_id);

    return 0;
}

/**
 * @brief 内部函数：获取灵敏度
 * @param dev 设备句柄
 * @return 灵敏度(LSB/mT)
 */
static float TMAG3001_GetSensitivity(TMAG3001_Handle_t *dev)
{
    float base_sensitivity;

    // 获取基础灵敏度
    if (dev->version == TMAG3001_VERSION_A1) {
        base_sensitivity = (dev->range == TMAG3001_RANGE_40mT) ? TMAG3001_SENSITIVITY_A1_40mT : TMAG3001_SENSITIVITY_A1_80mT;
    } else {
        base_sensitivity = (dev->range == TMAG3001_RANGE_120mT) ? TMAG3001_SENSITIVITY_A2_120mT : TMAG3001_SENSITIVITY_A2_240mT;
    }

    // 计算增益因子 (增益 = 0.5 + gain_index * 0.1)
    float gain_factor = 0.5f + ((float)dev->gain * 0.1f);

    // 返回调整后的灵敏度 (增益越大，灵敏度越高，LSB/mT值越小)
    return base_sensitivity / gain_factor;
}

/**
 * @brief 初始化TMAG3001设备
 * @param dev 设备句柄
 * @param i2c_id I2C外设编号, 0=I2C1, 1=I2C2
 * @param i2c_addr I2C地址(7位)
 * @param version 设备版本(A1或A2)
 * @return 0=成功, 非0=失败
 */
uint8_t TMAG3001_Init(TMAG3001_Handle_t *dev, uint8_t i2c_id, uint8_t i2c_addr, TMAG3001_Version_t version)
{
    uint8_t result;
    uint8_t device_id;
    uint16_t manufacturer_id;

    // 保存配置
    dev->i2c_id = i2c_id;
    dev->i2c_addr = i2c_addr;
    dev->version = version;
    // 使用最大量程避免饱和
    dev->range = (version == TMAG3001_VERSION_A1) ? TMAG3001_RANGE_80mT : TMAG3001_RANGE_240mT;
    // 默认增益
    dev->gain = TMAG3001_GAIN_1_0;

    // 读取并验证设备ID
    result = TMAG3001_ReadID(dev, &device_id, &manufacturer_id);
    if (result != 0) return result;

    if (device_id != TMAG3001_DEVICE_ID || manufacturer_id != TMAG3001_MANUFACTURER_ID) {
//        return 100;  // ID验证失败
    }

    // 配置默认设置
    // 1. 设置为连续测量模式
    result = TMAG3001_SetOperatingMode(dev, TMAG3001_MODE_CONTINUOUS);
    if (result != 0) return result;

    // 2. 启用XYZ三轴测量
    result = TMAG3001_SetMagChannels(dev, TMAG3001_CH_XYZ);
    if (result != 0) return result;

    // 3. 设置默认范围
    result = TMAG3001_SetRange(dev, dev->range);
    if (result != 0) return result;

    // 4. 设置平均次数为1x
    result = TMAG3001_SetConvAvg(dev, TMAG3001_AVG_16X);
    if (result != 0) return result;

    // 5. 设置默认增益为1.0x
    result = TMAG3001_SetMagGain(dev, TMAG3001_GAIN_2_0);
    if (result != 0) return result;

    return 0;
}

/**
 * @brief 读取设备ID和制造商ID
 * @param dev 设备句柄
 * @param device_id 设备ID输出
 * @param manufacturer_id 制造商ID输出
 * @return 0=成功, 非0=失败
 */
uint8_t TMAG3001_ReadID(TMAG3001_Handle_t *dev, uint8_t *device_id, uint16_t *manufacturer_id)
{
    uint8_t result;
    uint8_t data[3];

    // 读取设备ID和制造商ID (连续3个寄存器)
    result = TMAG3001_ReadRegs(dev, TMAG3001_REG_DEVICE_ID, data, 3);
    if (result != 0) return result;

    *device_id = data[0];
    *manufacturer_id = (data[2] << 8) | data[1];

    return 0;
}

/**
 * @brief 设置操作模式
 * @param dev 设备句柄
 * @param mode 操作模式
 * @return 0=成功, 非0=失败
 */
uint8_t TMAG3001_SetOperatingMode(TMAG3001_Handle_t *dev, TMAG3001_OperatingMode_t mode)
{
    uint8_t result;
    uint8_t reg_val;

    result = TMAG3001_ReadReg(dev, TMAG3001_REG_DEVICE_CONFIG_2, &reg_val);
    if (result != 0) return result;

    reg_val &= ~0x03;  // 清除OPERATING_MODE位
    reg_val |= (mode & 0x03);

    return TMAG3001_WriteReg(dev, TMAG3001_REG_DEVICE_CONFIG_2, reg_val);
}

/**
 * @brief 设置磁场测量通道
 * @param dev 设备句柄
 * @param channels 通道选择
 * @return 0=成功, 非0=失败
 */
uint8_t TMAG3001_SetMagChannels(TMAG3001_Handle_t *dev, TMAG3001_MagChannel_t channels)
{
    uint8_t result;
    uint8_t reg_val;

    result = TMAG3001_ReadReg(dev, TMAG3001_REG_SENSOR_CONFIG_1, &reg_val);
    if (result != 0) return result;

    reg_val &= ~0xF0;  // 清除MAG_CH_EN位
    reg_val |= ((channels & 0x0F) << 4);

    return TMAG3001_WriteReg(dev, TMAG3001_REG_SENSOR_CONFIG_1, reg_val);
}

/**
 * @brief 设置磁场测量范围
 * @param dev 设备句柄
 * @param range 测量范围
 * @return 0=成功, 非0=失败
 */
uint8_t TMAG3001_SetRange(TMAG3001_Handle_t *dev, TMAG3001_Range_t range)
{
    uint8_t result;
    uint8_t reg_val;

    result = TMAG3001_ReadReg(dev, TMAG3001_REG_SENSOR_CONFIG_2, &reg_val);
    if (result != 0) return result;

    reg_val &= ~0xC0;  // 清除RANGE位
    reg_val |= ((range & 0x01) << 6);

    result = TMAG3001_WriteReg(dev, TMAG3001_REG_SENSOR_CONFIG_2, reg_val);
    if (result == 0) {
        dev->range = range;
    }

    return result;
}

/**
 * @brief 设置转换平均次数
 * @param dev 设备句柄
 * @param avg 平均次数
 * @return 0=成功, 非0=失败
 */
uint8_t TMAG3001_SetConvAvg(TMAG3001_Handle_t *dev, TMAG3001_ConvAvg_t avg)
{
    uint8_t result;
    uint8_t reg_val;

    result = TMAG3001_ReadReg(dev, TMAG3001_REG_DEVICE_CONFIG_2, &reg_val);
    if (result != 0) return result;

    reg_val &= ~0x1C;  // 清除CONV_AVG位
    reg_val |= ((avg & 0x07) << 2);

    return TMAG3001_WriteReg(dev, TMAG3001_REG_DEVICE_CONFIG_2, reg_val);
}

/**
 * @brief 设置磁场增益
 * @param dev 设备句柄
 * @param gain 增益值
 * @return 0=成功, 非0=失败
 */
uint8_t TMAG3001_SetMagGain(TMAG3001_Handle_t *dev, TMAG3001_MagGain_t gain)
{
    uint8_t result = TMAG3001_WriteReg(dev, TMAG3001_REG_MAG_GAIN_CONFIG, gain & 0x0F);
    if (result == 0) {
        dev->gain = gain;
    }
    return result;
}

/**
 * @brief 触发一次转换(用于Standby或Wake-up模式)
 * @param dev 设备句柄
 * @return 0=成功, 非0=失败
 */
uint8_t TMAG3001_TriggerConversion(TMAG3001_Handle_t *dev)
{
    uint8_t result;
    uint8_t reg_val;

    result = TMAG3001_ReadReg(dev, TMAG3001_REG_DEVICE_CONFIG_1, &reg_val);
    if (result != 0) return result;

    reg_val |= 0x01;  // 设置CONV_START位

    return TMAG3001_WriteReg(dev, TMAG3001_REG_DEVICE_CONFIG_1, reg_val);
}

/**
 * @brief 读取转换状态
 * @param dev 设备句柄
 * @param status 状态输出
 * @return 0=成功, 非0=失败
 */
uint8_t TMAG3001_ReadConvStatus(TMAG3001_Handle_t *dev, uint8_t *status)
{
    return TMAG3001_ReadReg(dev, TMAG3001_REG_CONV_STATUS, status);
}

/**
 * @brief 读取磁场数据
 * @param dev 设备句柄
 * @param result 结果结构
 * @return 0=成功, 非0=失败
 */
uint8_t TMAG3001_ReadMagneticData(TMAG3001_Handle_t *dev, TMAG3001_Result_t *result)
{
    uint8_t status;
    uint8_t data[6];
    float sensitivity;

    // 读取X, Y, Z轴数据 (6个寄存器)
    status = TMAG3001_ReadRegs(dev, TMAG3001_REG_X_MSB_RESULT, data, 6);
    if (status != 0) return status;

    // 组合MSB和LSB (12位数据，符号扩展到16位)
    result->x_raw = (int16_t)((data[0] << 8) | data[1]);
    result->y_raw = (int16_t)((data[2] << 8) | data[3]);
    result->z_raw = (int16_t)((data[4] << 8) | data[5]);

    // 右移4位以获得12位有符号数据
    result->x_raw >>= 4;
    result->y_raw >>= 4;
    result->z_raw >>= 4;

    // 转换为物理单位 (mT)
    sensitivity = TMAG3001_GetSensitivity(dev);
    result->x_mT = (float)result->x_raw / sensitivity;
    result->y_mT = (float)result->y_raw / sensitivity;
    result->z_mT = (float)result->z_raw / sensitivity;

    return 0;
}

/**
 * @brief 使用过采样读取磁场数据（提高信噪比）
 * @param dev 设备句柄
 * @param result 结果结构
 * @param oversample_count 过采样次数 (1-32)
 * @return 0=成功, 非0=失败
 */
uint8_t TMAG3001_ReadMagneticDataWithOversampling(TMAG3001_Handle_t *dev, TMAG3001_Result_t *result, uint8_t oversample_count)
{
    uint8_t status;
    uint32_t x_sum = 0, y_sum = 0, z_sum = 0;
    uint8_t i;

    if (oversample_count == 0 || oversample_count > 32) {
        oversample_count = 4; // 默认4次过采样
    }

    // 读取多次数据并求平均
    for (i = 0; i < oversample_count; i++) {
        TMAG3001_Result_t temp_result;

        status = TMAG3001_ReadMagneticData(dev, &temp_result);
        if (status != 0) return status;

        x_sum += temp_result.x_raw;
        y_sum += temp_result.y_raw;
        z_sum += temp_result.z_raw;

        // 短暂延迟确保连续转换
        Delay_us(100);
    }

    // 计算平均值
    result->x_raw = (int16_t)(x_sum / oversample_count);
    result->y_raw = (int16_t)(y_sum / oversample_count);
    result->z_raw = (int16_t)(z_sum / oversample_count);

    // 转换为物理单位
    float sensitivity = TMAG3001_GetSensitivity(dev);
    result->x_mT = (float)result->x_raw / sensitivity;
    result->y_mT = (float)result->y_raw / sensitivity;
    result->z_mT = (float)result->z_raw / sensitivity;

    return 0;
}

/**
 * @brief 应用数字增益放大（软件放大）
 * @param raw_value 原始值
 * @param digital_gain 数字增益倍数
 * @return 放大后的值
 */
float TMAG3001_ApplyDigitalGain(float raw_value, float digital_gain)
{
    if (digital_gain < 1.0f) digital_gain = 1.0f;
    if (digital_gain > 10.0f) digital_gain = 10.0f; // 限制最大数字增益

    return raw_value * digital_gain;
}
