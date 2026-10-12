#include "stm32f1xx_hal.h"
#include "tca9548a.h"
#include "../bsp/bsp_i2c_soft.h"
#include "../bsp/bsp_delay.h"

#define TCA9548A_RST1_PORT  GPIOB
#define TCA9548A_RST1_PIN   GPIO_PIN_12
#define TCA9548A_RST2_PORT  GPIOB
#define TCA9548A_RST2_PIN   GPIO_PIN_13

void TCA9548A_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Pin   = TCA9548A_RST1_PIN | TCA9548A_RST2_PIN;
    HAL_GPIO_Init(GPIOB, &gpio);

    HAL_GPIO_WritePin(TCA9548A_RST1_PORT, TCA9548A_RST1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TCA9548A_RST2_PORT, TCA9548A_RST2_PIN, GPIO_PIN_RESET);
    Delay_ms(10);
    HAL_GPIO_WritePin(TCA9548A_RST1_PORT, TCA9548A_RST1_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TCA9548A_RST2_PORT, TCA9548A_RST2_PIN, GPIO_PIN_SET);
    Delay_ms(10);
}

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
