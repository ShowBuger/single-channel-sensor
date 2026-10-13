#include "bsp_i2c_soft.h"
#include "bsp_delay.h"
#include "bsp_uart.h"
#include "stm32f1xx_hal.h"
#include "board_config.h"

typedef struct {
    GPIO_TypeDef* scl_port;
    uint16_t      scl_pin;
    GPIO_TypeDef* sda_port;
    uint16_t      sda_pin;
} i2c_group;

#define IIC_NUM 1

static i2c_group iic[IIC_NUM] = {
    { BSP_I2C_SCL_PORT, BSP_I2C_SCL_PIN,
      BSP_I2C_SDA_PORT, BSP_I2C_SDA_PIN }
};

#define IIC_SCL_HIGH(i)  HAL_GPIO_WritePin(iic[i].scl_port, iic[i].scl_pin, GPIO_PIN_SET)
#define IIC_SCL_LOW(i)   HAL_GPIO_WritePin(iic[i].scl_port, iic[i].scl_pin, GPIO_PIN_RESET)
#define IIC_SDA_HIGH(i)  HAL_GPIO_WritePin(iic[i].sda_port, iic[i].sda_pin, GPIO_PIN_SET)
#define IIC_SDA_LOW(i)   HAL_GPIO_WritePin(iic[i].sda_port, iic[i].sda_pin, GPIO_PIN_RESET)
#define IIC_SDA_READ(i)  HAL_GPIO_ReadPin(iic[i].sda_port, iic[i].sda_pin)

void SoftI2C_Init(void)
{
    BSP_I2C_GPIO_CLK_EN();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode  = GPIO_MODE_OUTPUT_OD;   /* open-drain */
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    int i;
    for (i = 0; i < IIC_NUM; i++)
    {
        gpio.Pin = iic[i].scl_pin;
        HAL_GPIO_Init(iic[i].scl_port, &gpio);
        HAL_GPIO_WritePin(iic[i].scl_port, iic[i].scl_pin, GPIO_PIN_SET);

        gpio.Pin = iic[i].sda_pin;
        HAL_GPIO_Init(iic[i].sda_port, &gpio);
        HAL_GPIO_WritePin(iic[i].sda_port, iic[i].sda_pin, GPIO_PIN_SET);
    }
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
        if (IIC_SDA_READ(idx) == GPIO_PIN_SET)
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
    while (IIC_SDA_READ(idx) == GPIO_PIN_SET)
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

void I2C_ScanDevices(uint8_t idx)
{
    uint8_t addr, found = 0;
    Serial_Printf("Scanning I2C bus...\r\n");
    for (addr = 0x00; addr < 0x80; addr++)
    {
        IIC_Start(idx);
        I2C_WriteByte(idx, addr << 1);
        if (I2C_WaitAck(idx) == 0)
        {
            Serial_Printf("Found device at 0x%02X\r\n", addr);
            found++;
        }
        IIC_Stop(idx);
        Delay_us(10);
    }
    if (found == 0)
        Serial_Printf("No I2C devices found.\r\n");
    else
        Serial_Printf("Total %d device(s) found.\r\n", found);
}
