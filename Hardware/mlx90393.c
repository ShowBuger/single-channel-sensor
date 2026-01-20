#include "MLX90393.h"
#include "HardI2C.h"
#include "Delay.h"
#include "stm32f10x.h"


/**
 * 函数: error_process
 * 功能: 处理MLX90393状态字节，检测并返回错误类型
 * 参数: status - 从MLX90393读取的状态字节
 * 返回: 错误类型
 *       MLX90393_ERROR_NONE (0x00)      - 无错误
 *       MLX90393_ERROR_BIT_SET (0x01)   - ERROR位被置位(命令被拒绝或ECC错误)
 *       MLX90393_ERROR_ECC (0x02)       - ECC双位错误(内存不可纠正错误)
 *       MLX90393_ERROR_DRDY (0x03)      - DRDY标志低时读取数据
 *
 * 状态字节格式(从bit 7到bit 0):
 * | BURST_MODE | WOC_MODE | SM_MODE | ERROR | SED | RS | D1 | D0 |
 * |    bit 7   |   bit 6  |  bit 5  | bit 4 | bit 3 | bit 2 | bit 1 | bit 0 |
 */
uint8_t error_process(uint8_t status)
{
    // 检查ERROR位(bit 4)
    if(status & MLX90393_STATUS_ERROR)
    {
        // ERROR位被置位可能原因:
        // 1. 命令被拒绝(如模式切换失败、突发模式中执行了不允许的RR/WR命令)
        // 2. 内存中检测到不可纠正的ECC错误(双位错误)
        // 3. 在DRDY标志为低时读取数据

        // 进一步判断是否为ECC错误(需要结合具体应用场景)
        // 这里返回通用的ERROR_BIT_SET
        return MLX90393_ERROR_BIT_SET;
    }

    // 检查SED位(bit 3) - 单错误检测位
    // 此位仅为信息性标志，表示非易失性内存中的单位错误已被纠正
    // 不影响操作，但可以作为信息记录
    if(status & MLX90393_STATUS_SED)
    {
        // SED位被置位表示内存单位错误已自动纠正
        // 这是信息性的，不视为错误
        // 可以在这里添加日志记录(如果需要)
    }

    // 检查RS位(bit 2) - 复位状态位
    // RT命令发出后，状态消息中会置位RS，然后在复位期间清除
    // 这是正常的复位指示，不视为错误

    // 检查D[1:0]位(bit 1-0) - 数据字节计数
    // 这些位仅在RR和RM命令后有意义，表示响应字节数 = 2^D[1:0] + 2
    // 对于其他命令，D[1:0]应该被忽略

    // 检查MODE位(bit 7-5)
    // 这些位指示传感器当前所处的模式，用于确认模式切换是否成功
    // 模式切换被拒绝时，预期的模式位会被清除，同时ERROR位置位

    return MLX90393_ERROR_NONE;
}

/**
 * 函数: MLX90393_getDataByteCount
 * 功能: 从状态字节中提取数据字节计数(D[1:0])
 * 参数: status - 状态字节
 * 返回: 期望的响应字节数 (2, 4, 6, 或 8)
 * 说明: 仅在RR(Read Register)和RM(Read Measurement)命令后有效
 *       响应字节数 = 2^D[1:0] + 2
 */
uint8_t MLX90393_getDataByteCount(uint8_t status)
{
    uint8_t d_bits = status & (MLX90393_STATUS_D0 | MLX90393_STATUS_D1);
    // 计算: 2^d_bits + 2
    return (1 << d_bits) + 2;
}

/**
 * 函数: MLX90393_isBurstMode
 * 功能: 检查传感器是否处于突发模式
 * 参数: status - 状态字节
 * 返回: 1 - 处于突发模式, 0 - 未处于突发模式
 */
uint8_t MLX90393_isBurstMode(uint8_t status)
{
    return (status & MLX90393_STATUS_BURST_MODE) ? 1 : 0;
}

/**
 * 函数: MLX90393_isWOCMode
 * 功能: 检查传感器是否处于Wake-on-Change模式
 * 参数: status - 状态字节
 * 返回: 1 - 处于WOC模式, 0 - 未处于WOC模式
 */
uint8_t MLX90393_isWOCMode(uint8_t status)
{
    return (status & MLX90393_STATUS_WOC_MODE) ? 1 : 0;
}

/**
 * 函数: MLX90393_isSingleMode
 * 功能: 检查传感器是否处于单次测量模式
 * 参数: status - 状态字节
 * 返回: 1 - 处于单次测量模式, 0 - 未处于单次测量模式
 */
uint8_t MLX90393_isSingleMode(uint8_t status)
{
    return (status & MLX90393_STATUS_SM_MODE) ? 1 : 0;
}

//———————————————————————————————————————————————
// 初始化函数：根据 A1、A0 设置 I2C 地址，DRDY_pin 传入 -1 表示不使用 
//———————————————————————————————————————————————
uint8_t MLX90393_begin(MLX90393_Handle *handle, uint8_t A1, uint8_t A0, int DRDY_pin,uint8_t id)
{
    uint8_t status;
    if(handle)
    {
        handle->I2C_Address = MLX90393_I2C_BASE_ADDR | ((A1 ? 2 : 0) | (A0 ? 1 : 0));
        handle->DRDY_pin = DRDY_pin;
    }
    /* Exit Mode */
    status = MLX90393_exit_mode(handle, id);
    if(error_process(status) == MLX90393_ERROR_BIT_SET) return 1;   //错误退出
    Delay_ms(1) ;  // Wait for 1 ms

    /* Reset */
    status = MLX90393_reset(handle, id);
    if(error_process(status) == MLX90393_ERROR_BIT_SET) return 1;   //错误退出
    Delay_ms(2);

    /*Config OSR*/
    status = MLX90393_setOverSampling(handle, id, 3);
    if(error_process(status) == MLX90393_ERROR_BIT_SET) return 1;   //错误退出
    Delay_ms(1);

    /*Config Filter*/
    status = MLX90393_setDigitalFiltering(handle, id, 0);
    if(error_process(status) == MLX90393_ERROR_BIT_SET) return 1;   //错误退出
    Delay_ms(1);

//    /*执行内建自检(BIST)*/
//    if(MLX90393_selfTest(handle, id) != 0)
//    {
//        return 1;  // 自检失败
//    }
    return 0;
}

//———————————————————————————————————————————————
// 发送命令：发送单字节命令并读取响应状态字节
//———————————————————————————————————————————————
uint8_t MLX90393_sendCommand(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t cmd)
{
    uint8_t resp = 0xFF;  // 默认错误值
    I2C_TypeDef* I2Cx = (i2c_id == 0) ? I2C1 : I2C2;
    uint32_t timeout;

    // 检查I2C总线是否忙
    timeout = 10000;
    while(I2C_GetFlagStatus(I2Cx, I2C_FLAG_BUSY))
    {
        if(--timeout == 0) return 0xFF;  // 总线忙超时
    }

    // 发送命令
    I2C_GenerateSTART(I2Cx, ENABLE);
    timeout = 10000;
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_MODE_SELECT))
    {
        if(--timeout == 0)
        {
            I2C_GenerateSTOP(I2Cx, ENABLE);
            return 0xFF;  // 超时返回错误
        }
    }

    I2C_Send7bitAddress(I2Cx, handle->I2C_Address << 1, I2C_Direction_Transmitter);
    timeout = 10000;
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
    {
        if(--timeout == 0)
        {
            I2C_GenerateSTOP(I2Cx, ENABLE);
            return 0xFF;  // 超时返回错误
        }
    }

    I2C_SendData(I2Cx, cmd);
    timeout = 10000;
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
    {
        if(--timeout == 0)
        {
            I2C_GenerateSTOP(I2Cx, ENABLE);
            return 0xFF;  // 超时返回错误
        }
    }

    // 读取状态字节
    I2C_GenerateSTART(I2Cx, ENABLE);  // 重复起始
    timeout = 10000;
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_MODE_SELECT))
    {
        if(--timeout == 0)
        {
            I2C_GenerateSTOP(I2Cx, ENABLE);
            return 0xFF;  // 超时返回错误
        }
    }

    I2C_Send7bitAddress(I2Cx, handle->I2C_Address << 1, I2C_Direction_Receiver);
    timeout = 10000;
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED))
    {
        if(--timeout == 0)
        {
            I2C_GenerateSTOP(I2Cx, ENABLE);
            return 0xFF;  // 超时返回错误
        }
    }

    // 单字节接收序列：禁用ACK并生成STOP
    I2C_AcknowledgeConfig(I2Cx, DISABLE);
    I2C_GenerateSTOP(I2Cx, ENABLE);

    timeout = 10000;
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_RECEIVED))
    {
        if(--timeout == 0)
        {
            I2C_AcknowledgeConfig(I2Cx, ENABLE);
            return 0xFF;  // 超时返回错误
        }
    }
    resp = I2C_ReceiveData(I2Cx);

    // 恢复ACK配置
    I2C_AcknowledgeConfig(I2Cx, ENABLE);

    return resp;
}

//———————————————————————————————————————————————
// 读寄存器：发送 CMD_READ_REGISTER 并读取 3 字节（状态，高字节，低字节）
//———————————————————————————————————————————————
uint8_t MLX90393_readRegister(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t reg, uint16_t *data)
{
    uint8_t buffer[3];
    I2C_TypeDef* I2Cx = (i2c_id == 0) ? I2C1 : I2C2;

    // 发送读寄存器命令
    I2C_GenerateSTART(I2Cx, ENABLE);
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_MODE_SELECT));

    I2C_Send7bitAddress(I2Cx, handle->I2C_Address << 1, I2C_Direction_Transmitter);
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    I2C_SendData(I2Cx, MLX90393_CMD_READ_REGISTER);
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_SendData(I2Cx, (reg & 0x3F) << 2);
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    // 读取3字节数据
    I2C_GenerateSTART(I2Cx, ENABLE);  // 重复起始
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_MODE_SELECT));

    I2C_Send7bitAddress(I2Cx, handle->I2C_Address << 1, I2C_Direction_Receiver);
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED));

    // 读取第1个字节 (状态) - 发送ACK
    I2C_AcknowledgeConfig(I2Cx, ENABLE);
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_RECEIVED));
    buffer[0] = I2C_ReceiveData(I2Cx);

    // 读取第2个字节 (高字节) - 发送ACK
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_RECEIVED));
    buffer[1] = I2C_ReceiveData(I2Cx);

    // 读取第3个字节 (低字节) - 发送NACK并STOP
    I2C_AcknowledgeConfig(I2Cx, DISABLE);
    I2C_GenerateSTOP(I2Cx, ENABLE);
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_RECEIVED));
    buffer[2] = I2C_ReceiveData(I2Cx);

    // 恢复ACK配置
    I2C_AcknowledgeConfig(I2Cx, ENABLE);

    *data = (buffer[1] << 8) | buffer[2];
    return buffer[0]; // 状态字节
}

//———————————————————————————————————————————————
// 写寄存器：发送 CMD_WRITE_REGISTER，写入 2 字节数据及寄存器地址
//———————————————————————————————————————————————
uint8_t MLX90393_writeRegister(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t reg, uint16_t data)
{
    uint8_t resp;
    uint8_t MSB = data >> 8;
    uint8_t LSB = data & 0xFF;
    I2C_TypeDef* I2Cx = (i2c_id == 0) ? I2C1 : I2C2;

    // 发送写寄存器命令及数据
    I2C_GenerateSTART(I2Cx, ENABLE);
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_MODE_SELECT));

    I2C_Send7bitAddress(I2Cx, handle->I2C_Address << 1, I2C_Direction_Transmitter);
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    I2C_SendData(I2Cx, MLX90393_CMD_WRITE_REGISTER);
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_SendData(I2Cx, MSB);
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_SendData(I2Cx, LSB);
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_SendData(I2Cx, (reg & 0x3F) << 2);
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    // 读取状态字节
    I2C_GenerateSTART(I2Cx, ENABLE);  // 重复起始
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_MODE_SELECT));

    I2C_Send7bitAddress(I2Cx, handle->I2C_Address << 1, I2C_Direction_Receiver);
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED));

    // 单字节接收序列：禁用ACK并生成STOP
    I2C_AcknowledgeConfig(I2Cx, DISABLE);
    I2C_GenerateSTOP(I2Cx, ENABLE);

    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_RECEIVED));
    resp = I2C_ReceiveData(I2Cx);

    // 恢复ACK配置
    I2C_AcknowledgeConfig(I2Cx, ENABLE);

    return resp;
}

//———————————————————————————————————————————————
// 复位：发送 RESET 命令并延时 2ms
//———————————————————————————————————————————————
uint8_t MLX90393_reset(MLX90393_Handle *handle, uint8_t i2c_id)
{
    return MLX90393_sendCommand(handle, i2c_id, MLX90393_CMD_RESET);;
}

//———————————————————————————————————————————————
// 执行内建自检(Built-In Self Test)
// 功能：测试片上线圈，施加Z方向磁场
// 步骤：
//   1. 测量基准Z值（BIST关闭）
//   2. 读取寄存器0x00
//   3. 设置BIST位(bit 7)为1
//   4. 测量Z值（BIST开启）
//   5. 清除BIST位
//   6. 比较两次测量差值
// 返回：0 - 自检通过, -1 - 自检失败
//———————————————————————————————————————————————
int8_t MLX90393_selfTest(MLX90393_Handle *handle, uint8_t i2c_id)
{
    uint8_t status;
    uint16_t reg_value;
    MLX90393_RawData baseline_data, bist_data;
    int32_t z_diff;

    // 1. 先测量基准Z值（BIST关闭状态）
    status = MLX90393_startMeasurement(handle, i2c_id, MLX90393_FLAG_Z);
    if(error_process(status) == MLX90393_ERROR_BIT_SET)
    {
        return -1;  // 启动测量失败
    }

    Delay_ms(10);  // 等待测量完成

    status = MLX90393_readMeasurement(handle, i2c_id, MLX90393_FLAG_Z, &baseline_data);
    if(error_process(status) == MLX90393_ERROR_BIT_SET)
    {
        return -1;  // 读取基准值失败
    }

    // 2. 读取寄存器0x00的当前值
    status = MLX90393_readRegister(handle, i2c_id, 0x00, &reg_value);
    if(error_process(status) == MLX90393_ERROR_BIT_SET)
    {
        return -1;  // 读取寄存器失败
    }

    // 3. 设置BIST位(bit 7)为1，启用自检
    reg_value |= (1 << BIST_SHIFT);
    status = MLX90393_writeRegister(handle, i2c_id, 0x00, reg_value);
    if(error_process(status) == MLX90393_ERROR_BIT_SET)
    {
        return -1;  // 写入失败
    }

    // 4. 启动Z轴单次测量（BIST会在Z方向施加磁场）
    status = MLX90393_startMeasurement(handle, i2c_id, MLX90393_FLAG_Z);
    if(error_process(status) == MLX90393_ERROR_BIT_SET)
    {
        return -1;  // 启动测量失败
    }

    Delay_ms(10);  // 等待测量完成

    // 5. 读取BIST开启时的Z轴测量结果
    status = MLX90393_readMeasurement(handle, i2c_id, MLX90393_FLAG_Z, &bist_data);
    if(error_process(status) == MLX90393_ERROR_BIT_SET)
    {
        return -1;  // 读取测量失败
    }

    // 6. 清除BIST位，恢复正常模式
    reg_value &= ~(1 << BIST_SHIFT);
    status = MLX90393_writeRegister(handle, i2c_id, 0x00, reg_value);
    if(error_process(status) == MLX90393_ERROR_BIT_SET)
    {
        return -1;  // 清除BIST位失败
    }

    // 7. 计算两次测量的差值（作为有符号数处理）
    z_diff = (int32_t)((int16_t)bist_data.z) - (int32_t)((int16_t)baseline_data.z);

    // 8. 验证自检结果
    // BIST应该产生一个明显的磁场变化
    // 差值的绝对值应该足够大，表明BIST线圈工作正常
    // 典型差值应该在几百到几千的范围内（取决于配置）
    if(z_diff < 0) z_diff = -z_diff;  // 取绝对值

    // 检查差值是否超过阈值（阈值定义在.h文件中，可调整）
    if(z_diff < BIST_MIN_DIFF)
    {
        return -1;  // 自检失败：BIST磁场变化不明显
    }

    return 0;  // 自检通过
}

//———————————————————————————————————————————————
// 启动单次测量：发送 CMD_START_MEASUREMENT，flags 控制 T, X, Y, Z 的选择
//———————————————————————————————————————————————
uint8_t MLX90393_startMeasurement(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t flags)
{
    uint8_t cmd = MLX90393_CMD_START_MEASUREMENT | (flags & 0x0F);
    // 调用 sendCommand 发送命令并返回状态字节
    return MLX90393_sendCommand(handle, i2c_id, cmd);
}
//———————————————————————————————————————————————
//———————————————————————————————————————————————
uint8_t MLX90393_readMeasurement(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t flags, MLX90393_RawData *raw)
{
    uint8_t cmd = MLX90393_CMD_READ_MEASUREMENT | (flags & 0x0F);
    uint8_t buffer[9];
    uint8_t count = 1;
    uint8_t i;
    I2C_TypeDef* I2Cx = (i2c_id == 0) ? I2C1 : I2C2;

    if(flags & MLX90393_FLAG_T) count += 2;
    if(flags & MLX90393_FLAG_X) count += 2;
    if(flags & MLX90393_FLAG_Y) count += 2;
    if(flags & MLX90393_FLAG_Z) count += 2;

    // 发送读测量命令
    I2C_GenerateSTART(I2Cx, ENABLE);
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_MODE_SELECT));

    I2C_Send7bitAddress(I2Cx, handle->I2C_Address << 1, I2C_Direction_Transmitter);
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    I2C_SendData(I2Cx, cmd);
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    // 读取数据
    I2C_GenerateSTART(I2Cx, ENABLE);  // 重复起始
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_MODE_SELECT));

    I2C_Send7bitAddress(I2Cx, handle->I2C_Address << 1, I2C_Direction_Receiver);
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED));

    // 读取所有字节
    I2C_AcknowledgeConfig(I2Cx, ENABLE);
    for(i = 0; i < count; i++){
        if(i == count - 1) {
            // 最后一个字节：禁用ACK并生成STOP
            I2C_AcknowledgeConfig(I2Cx, DISABLE);
            I2C_GenerateSTOP(I2Cx, ENABLE);
        }
        while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_RECEIVED));
        buffer[i] = I2C_ReceiveData(I2Cx);
    }

    // 恢复ACK配置
    I2C_AcknowledgeConfig(I2Cx, ENABLE);

    // 解析数据
    uint8_t index = 1;
    if(flags & MLX90393_FLAG_T){
         raw->t = (buffer[index] << 8) | buffer[index+1];
         index += 2;
    } else {
         raw->t = 0;
    }
    if(flags & MLX90393_FLAG_X){
         raw->x = (buffer[index] << 8) | buffer[index+1];
         index += 2;
    } else {
         raw->x = 0;
    }
    if(flags & MLX90393_FLAG_Y){
         raw->y = (buffer[index] << 8) | buffer[index+1];
         index += 2;
    } else {
         raw->y = 0;
    }
    if(flags & MLX90393_FLAG_Z){
         raw->z = (buffer[index] << 8) | buffer[index+1];
         index += 2;
    } else {
         raw->z = 0;
    }
    return buffer[0];
}

//———————————————————————————————————————————————
// 数据转换函数：将原始数据转换为物理量
// 默认配置：增益 7（gain multiplier = 1.0）、hallconf = 0x0C（xy = 0.150, z = 0.242），温度公式：T = 25 + (raw.t - 46244) / 45.2
//———————————————————————————————————————————————
MLX90393_Data MLX90393_convertRaw(MLX90393_RawData raw)
{
    MLX90393_Data data;
    float gain_factor = 1.0f;
    float xy_sens = 0.150f;
    float z_sens = 0.242f;
    
    data.x = ((int16_t)raw.x) * xy_sens * gain_factor;
    data.y = ((int16_t)raw.y) * xy_sens * gain_factor;
    data.z = ((int16_t)raw.z) * z_sens * gain_factor;
    data.t = 25.0f + (((float)raw.t - 46244.0f) / 45.2f);
    
    return data;
}

uint8_t MLX90393_setGainSel(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t gain)
{
    // 写入寄存器 GAIN_SEL_REG：gain 值左移 GAIN_SEL_SHIFT
    uint16_t value = ((uint16_t)gain << GAIN_SEL_SHIFT);
    return MLX90393_writeRegister(handle, i2c_id, GAIN_SEL_REG, value);
}

uint8_t MLX90393_setResolution(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t res_x, uint8_t res_y, uint8_t res_z)
{
    // 分辨率：组合 X/Y/Z，每项 2 位：res_xyz = (res_z << 4) | (res_y << 2) | res_x，
    // 再左移 RES_XYZ_SHIFT 写入寄存器
    uint16_t res_xyz = (((res_z & 0x03) << 4) | ((res_y & 0x03) << 2) | (res_x & 0x03));
    uint16_t value = res_xyz << RES_XYZ_SHIFT;
    return MLX90393_writeRegister(handle, i2c_id, RES_XYZ_REG, value);
}

uint8_t MLX90393_setOverSampling(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t osr)
{
    uint16_t value = ((uint16_t)osr << OSR_SHIFT);
    return MLX90393_writeRegister(handle, i2c_id, OSR_REG, value);
}

uint8_t MLX90393_setDigitalFiltering(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t flt)
{
    uint16_t value = ((uint16_t)flt << DIG_FLT_SHIFT);
    return MLX90393_writeRegister(handle, i2c_id, DIG_FLT_REG, value);
}

uint8_t MLX90393_setTemperatureCompensation(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t enable)
{
    uint16_t value = ((uint16_t)(enable ? 1 : 0) << TCMP_EN_SHIFT);
    return MLX90393_writeRegister(handle, i2c_id, TCMP_EN_REG, value);
}

/**
 * 函数: MLX90393_setBurstDataRate
 * 功能: 设置突发模式数据速率
 * 参数: handle - MLX90393设备句柄
 *       i2c_id - I2C总线ID
 *       data_rate - 数据速率值 (0-63)
 *                   TINTERVAL = data_rate * 20ms
 *                   当data_rate = 0时为连续突发模式(无延迟)
 * 返回: 状态字节
 * 说明: 根据数据手册，BURST_DATA_RATE位于寄存器0x01的bit 0-6
 *       设置范围: 0-63 (0x00-0x3F)
 *       时间间隔 = BURST_DATA_RATE * 20ms
 *       例如: data_rate = 10, TINTERVAL = 200ms (5Hz采样率)
 */
uint8_t MLX90393_setBurstDataRate(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t data_rate)
{
    // 限制data_rate范围在0-63之间 (7位，但只使用低7位)
    data_rate = data_rate & 0x7F;
    uint16_t value = ((uint16_t)data_rate << BURST_DATA_RATE_SHIFT);
    return MLX90393_writeRegister(handle, i2c_id, BURST_DATA_RATE_REG, value);
}

uint8_t MLX90393_startBurst(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t flags)
{
    // 构造突发模式命令
    uint8_t cmd = MLX90393_CMD_START_BURST | (flags & 0x0F);
    // 发送命令并返回状态字节
    return MLX90393_sendCommand(handle, i2c_id, cmd);
}

uint8_t MLX90393_exit_mode(MLX90393_Handle *handle, uint8_t i2c_id)
{
    // 发送退出测量模式的命令，CMD_EXIT 定义为 0x80
    return MLX90393_sendCommand(handle, i2c_id, MLX90393_CMD_EXIT);
}

