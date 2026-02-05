#include "stm32f10x.h"
#include "SoftI2C.h"
#include "Delay.h"
#include <stdio.h>


typedef struct {
    GPIO_TypeDef* scl_port;
    uint16_t scl_pin;
    GPIO_TypeDef* sda_port;
    uint16_t sda_pin;
}i2c_group;


/* 支持2组I2C:
   iic[0] = I2C1替代: PB6(SCL), PB7(SDA)
   iic[1] = I2C2替代: PB10(SCL), PB11(SDA) */
#define iic_num 2


#define IIC_GPIO_HIGH GPIO_SetBits
#define IIC_GPIO_LOW GPIO_ResetBits



i2c_group iic[iic_num]={
 {GPIOB, GPIO_Pin_6,  GPIOB, GPIO_Pin_7},   /* I2C1: PB6(SCL), PB7(SDA) */
 {GPIOB, GPIO_Pin_10, GPIOB, GPIO_Pin_11}   /* I2C2: PB10(SCL), PB11(SDA) */
};

void SoftI2C_Init()
{
    int i = 0;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOA, ENABLE);  // ʹ��GBIOB��GPIOA��ʱ��

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;  //����Ϊ��©���
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;  
    for(i = 0; i < iic_num; i++)
    {
        GPIO_InitStructure.GPIO_Pin = iic[i].scl_pin;
        if(iic[i].scl_port == GPIOB)
        {
            GPIO_Init(GPIOB, &GPIO_InitStructure);
            GPIO_SetBits(GPIOB, iic[i].scl_pin);
        }
        else if(iic[i].scl_port == GPIOA)
        {
            GPIO_Init(GPIOA, &GPIO_InitStructure);
            GPIO_SetBits(GPIOA, iic[i].scl_pin);            
        }
        GPIO_InitStructure.GPIO_Pin = iic[i].sda_pin;
        if(iic[i].sda_port == GPIOB)
        {
            GPIO_Init(GPIOB, &GPIO_InitStructure);
            GPIO_SetBits(GPIOB, iic[i].sda_pin);
        }
        else if(iic[i].sda_port == GPIOA)
        {
            GPIO_Init(GPIOA, &GPIO_InitStructure);
            GPIO_SetBits(GPIOA, iic[i].sda_pin);             
        }
    }
}


void IIC_Start(uint8_t iic_index)
{
    IIC_GPIO_HIGH(iic[iic_index].sda_port, iic[iic_index].sda_pin);
    IIC_GPIO_HIGH(iic[iic_index].scl_port, iic[iic_index].scl_pin);
    Delay_us(10);
    IIC_GPIO_LOW(iic[iic_index].sda_port, iic[iic_index].sda_pin);
    Delay_us(10);
    IIC_GPIO_LOW(iic[iic_index].scl_port, iic[iic_index].scl_pin);
}

void IIC_Stop(uint8_t iic_index)
{
    IIC_GPIO_LOW(iic[iic_index].sda_port, iic[iic_index].sda_pin);
    IIC_GPIO_LOW(iic[iic_index].scl_port, iic[iic_index].scl_pin);
    Delay_us(10);
    IIC_GPIO_HIGH(iic[iic_index].scl_port, iic[iic_index].scl_pin);
    Delay_us(10);
    IIC_GPIO_HIGH(iic[iic_index].sda_port, iic[iic_index].sda_pin);
    Delay_us(10);
}
void I2C_WriteByte(uint8_t iic_index, uint8_t data)
{
    uint8_t i;
    for(i = 0; i < 8; i++)
    {
        IIC_GPIO_LOW(iic[iic_index].scl_port, iic[iic_index].scl_pin);
        Delay_us(5);

        if(data & 0x80)
        {
            IIC_GPIO_HIGH(iic[iic_index].sda_port, iic[iic_index].sda_pin);
        }
        else
        {
            IIC_GPIO_LOW(iic[iic_index].sda_port, iic[iic_index].sda_pin);
        }
        data <<= 1;
        Delay_us(5);

        IIC_GPIO_HIGH(iic[iic_index].scl_port, iic[iic_index].scl_pin);
        Delay_us(10);
    }
    IIC_GPIO_LOW(iic[iic_index].scl_port, iic[iic_index].scl_pin);
}

uint8_t I2C_ReadByte(uint8_t iic_index)
{
    uint8_t i;
    uint8_t data = 0;

    IIC_GPIO_HIGH(iic[iic_index].sda_port, iic[iic_index].sda_pin);

    for(i = 0; i < 8; i++)
    {
        IIC_GPIO_LOW(iic[iic_index].scl_port, iic[iic_index].scl_pin);
        Delay_us(10);

        IIC_GPIO_HIGH(iic[iic_index].scl_port, iic[iic_index].scl_pin);
        data <<= 1;

        if(GPIO_ReadInputDataBit(iic[iic_index].sda_port, iic[iic_index].sda_pin))
        {
            data |= 0x01;
        }
        Delay_us(10);
    }
    IIC_GPIO_LOW(iic[iic_index].scl_port, iic[iic_index].scl_pin);

    return data;
}

uint8_t I2C_WaitAck(uint8_t iic_index)
{
    uint8_t timeout = 0;

    IIC_GPIO_HIGH(iic[iic_index].sda_port, iic[iic_index].sda_pin);
    Delay_us(5);
    IIC_GPIO_HIGH(iic[iic_index].scl_port, iic[iic_index].scl_pin);
    Delay_us(5);

    while(GPIO_ReadInputDataBit(iic[iic_index].sda_port, iic[iic_index].sda_pin))
    {
        timeout++;
        if(timeout > 250)
        {
            IIC_Stop(iic_index);
            return 1;
        }
    }

    IIC_GPIO_LOW(iic[iic_index].scl_port, iic[iic_index].scl_pin);
    return 0;
}

void I2C_SendAck(uint8_t iic_index, uint8_t ack)
{
    IIC_GPIO_LOW(iic[iic_index].scl_port, iic[iic_index].scl_pin);
    if(ack == 0)
    {
        IIC_GPIO_LOW(iic[iic_index].sda_port, iic[iic_index].sda_pin);
    }
    else
    {
        IIC_GPIO_HIGH(iic[iic_index].sda_port, iic[iic_index].sda_pin);
    }
    Delay_us(5);

    IIC_GPIO_HIGH(iic[iic_index].scl_port, iic[iic_index].scl_pin);
    Delay_us(10);

    IIC_GPIO_LOW(iic[iic_index].scl_port, iic[iic_index].scl_pin);
    IIC_GPIO_HIGH(iic[iic_index].sda_port, iic[iic_index].sda_pin);
}

void I2C_ScanDevices(uint8_t iic_index)
{
    uint8_t addr;
    uint8_t ack;
    uint8_t found_count = 0;

    printf("Scanning I2C bus (0x00-0x7F)...\r\n");

    for(addr = 0x00; addr < 0x80; addr++)
    {
        IIC_Start(iic_index);
        I2C_WriteByte(iic_index, (addr << 1));
        ack = I2C_WaitAck(iic_index);
        IIC_Stop(iic_index);

        if(ack == 0)
        {
            printf("Found device at address: 0x%02X\r\n", addr);
            found_count++;
        }
        Delay_us(10);
    }

    if(found_count == 0)
    {
        printf("No I2C devices found.\r\n");
    }
    else
    {
        printf("Total %d device(s) found.\r\n", found_count);
    }
}
