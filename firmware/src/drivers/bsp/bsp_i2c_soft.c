#include "bsp_i2c_soft.h"
#include "bsp_delay.h"
#include "log_manager.h"
#include "plat.h"

/* Bit-banged I2C on top of plat_swi2c_*; timing unchanged from the original
 * driver (10 us half periods, ~40-50 kHz). Pin assignment comes from the board. */
#define IIC_SCL_HIGH(i)  plat_swi2c_scl((i), 1)
#define IIC_SCL_LOW(i)   plat_swi2c_scl((i), 0)
#define IIC_SDA_HIGH(i)  plat_swi2c_sda((i), 1)
#define IIC_SDA_LOW(i)   plat_swi2c_sda((i), 0)
#define IIC_SDA_READ(i)  plat_swi2c_sda_read(i)

void SoftI2C_Init(void)
{
    uint8_t i;
    for (i = 0; i < plat_swi2c_bus_count(); i++)
        plat_swi2c_init(i);
}

void IIC_Start(uint8_t idx)
{
    IIC_SDA_HIGH(idx);
    IIC_SCL_HIGH(idx);
    Delay_us(10);
    IIC_SDA_LOW(idx);
    Delay_us(10);
    IIC_SCL_LOW(idx);
}

void IIC_Stop(uint8_t idx)
{
    IIC_SDA_LOW(idx);
    IIC_SCL_LOW(idx);
    Delay_us(10);
    IIC_SCL_HIGH(idx);
    Delay_us(10);
    IIC_SDA_HIGH(idx);
    Delay_us(10);
}

void I2C_WriteByte(uint8_t idx, uint8_t data)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        IIC_SCL_LOW(idx);
        Delay_us(5);
        if (data & 0x80)
            IIC_SDA_HIGH(idx);
        else
            IIC_SDA_LOW(idx);
        data <<= 1;
        Delay_us(5);
        IIC_SCL_HIGH(idx);
        Delay_us(10);
    }
    IIC_SCL_LOW(idx);
}

uint8_t I2C_ReadByte(uint8_t idx)
{
    uint8_t i, data = 0;
    IIC_SDA_HIGH(idx);
    for (i = 0; i < 8; i++)
    {
        IIC_SCL_LOW(idx);
        Delay_us(10);
        IIC_SCL_HIGH(idx);
        data <<= 1;
        if (IIC_SDA_READ(idx))
            data |= 0x01;
        Delay_us(10);
    }
    IIC_SCL_LOW(idx);
    return data;
}

uint8_t I2C_WaitAck(uint8_t idx)
{
    uint8_t timeout = 0;
    IIC_SDA_HIGH(idx);
    Delay_us(5);
    IIC_SCL_HIGH(idx);
    Delay_us(5);
    while (IIC_SDA_READ(idx))
    {
        timeout++;
        if (timeout > 250)
        {
            IIC_Stop(idx);
            return 1;
        }
    }
    IIC_SCL_LOW(idx);
    return 0;
}

void I2C_SendAck(uint8_t idx, uint8_t ack)
{
    IIC_SCL_LOW(idx);
    if (ack == 0)
        IIC_SDA_LOW(idx);
    else
        IIC_SDA_HIGH(idx);
    Delay_us(5);
    IIC_SCL_HIGH(idx);
    Delay_us(10);
    IIC_SCL_LOW(idx);
    IIC_SDA_HIGH(idx);
}

uint8_t I2C_DevicePresent(uint8_t idx, uint8_t addr)
{
    uint8_t result;

    if (addr >= 0x80)
        return 1;

    IIC_Start(idx);
    I2C_WriteByte(idx, (uint8_t)(addr << 1));
    result = I2C_WaitAck(idx);
    IIC_Stop(idx);
    return result;
}

void I2C_ScanDevices(uint8_t idx)
{
    uint8_t addr, found = 0;
    LOG_I("I2C", "Scanning I2C bus...");
    for (addr = 0x00; addr < 0x80; addr++)
    {
        if (I2C_DevicePresent(idx, addr) == 0)
        {
            LOG_I("I2C", "Found device at 0x%02X", addr);
            found++;
        }
        Delay_us(10);
    }
    if (found == 0)
        LOG_W("I2C", "No I2C devices found.");
    else
        LOG_I("I2C", "Total %d device(s) found.", found);
}
