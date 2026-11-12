#include "tca9548a.h"
#include "../../bsp/bsp_i2c_soft.h"
#include "../../bsp/bsp_delay.h"

uint8_t TCA9548A_SelectChannel(uint8_t i2c_id, uint8_t tca_addr, uint8_t channel)
{
    IIC_Start(i2c_id);
    I2C_WriteByte(i2c_id, tca_addr << 1);
    if (I2C_WaitAck(i2c_id)) { IIC_Stop(i2c_id); return 1; }
    I2C_WriteByte(i2c_id, channel);
    if (I2C_WaitAck(i2c_id)) { IIC_Stop(i2c_id); return 1; }
    IIC_Stop(i2c_id);
    return 0;
}

uint8_t TCA9548A_ReadChannel(uint8_t i2c_id, uint8_t tca_addr, uint8_t *channel)
{
    IIC_Start(i2c_id);
    I2C_WriteByte(i2c_id, (tca_addr << 1) | 0x01);
    if (I2C_WaitAck(i2c_id)) { IIC_Stop(i2c_id); return 1; }
    *channel = I2C_ReadByte(i2c_id);
    I2C_SendAck(i2c_id, 1);   /* NACK */
    IIC_Stop(i2c_id);
    return 0;
}

uint8_t TCA9548A_DisableAllChannels(uint8_t i2c_id, uint8_t tca_addr)
{
    return TCA9548A_SelectChannel(i2c_id, tca_addr, TCA9548A_CH_NONE);
}
