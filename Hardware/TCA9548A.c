#include "stm32f10x.h"                  // Device header
#include "TCA9548A.h"
#include "Delay.h"

// TCA9548A RESET引脚定义 (根据原理图配置)
// TCA_RST1 连接到 PB12 (U5 - I2C1)
// TCA_RST2 连接到 PB13 (U6 - I2C2)
#define TCA9548A_RST1_GPIO_PORT    GPIOB
#define TCA9548A_RST1_GPIO_PIN     GPIO_Pin_12
#define TCA9548A_RST2_GPIO_PORT    GPIOB
#define TCA9548A_RST2_GPIO_PIN     GPIO_Pin_13

/**
  * 函    数:TCA9548A初始化
  * 参    数:无
  * 返 回 值:无
  * 说    明:配置RESET引脚为推挽输出,并释放复位(拉高)
  *         注意:需要根据实际硬件连接修改上面的引脚定义
  */
void TCA9548A_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    // 使能GPIO时钟 (根据实际引脚修改)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // 配置TCA_RST1引脚为推挽输出
    GPIO_InitStructure.GPIO_Pin = TCA9548A_RST1_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(TCA9548A_RST1_GPIO_PORT, &GPIO_InitStructure);

    // 配置TCA_RST2引脚为推挽输出
    GPIO_InitStructure.GPIO_Pin = TCA9548A_RST2_GPIO_PIN;
    GPIO_Init(TCA9548A_RST2_GPIO_PORT, &GPIO_InitStructure);

    // 复位TCA9548A (拉低RESET引脚)
    GPIO_ResetBits(TCA9548A_RST1_GPIO_PORT, TCA9548A_RST1_GPIO_PIN);
    GPIO_ResetBits(TCA9548A_RST2_GPIO_PORT, TCA9548A_RST2_GPIO_PIN);
    Delay_ms(10);  // 保持复位至少10ms

    // 释放复位 (拉高RESET引脚)
    GPIO_SetBits(TCA9548A_RST1_GPIO_PORT, TCA9548A_RST1_GPIO_PIN);
    GPIO_SetBits(TCA9548A_RST2_GPIO_PORT, TCA9548A_RST2_GPIO_PIN);
    Delay_ms(10);  // 等待TCA9548A启动
}

/**
  * 函    数:TCA9548A选择通道
  * 参    数:i2c_id I2C外设编号,0表示I2C1,1表示I2C2
  *         tca_addr TCA9548A的I2C地址,范围:0x70~0x77
  *         channel 要选择的通道,可以是单个通道或多个通道的组合
  * 返 回 值:0表示成功,1表示失败
  * 说    明:TCA9548A通过写入一个字节来控制8个通道,每个位对应一个通道
  */
uint8_t TCA9548A_SelectChannel(uint8_t i2c_id, uint8_t tca_addr, uint8_t channel)
{
    I2C_TypeDef* I2Cx = (i2c_id == 0) ? I2C1 : I2C2;
    uint32_t timeout;

    // 检查I2C总线是否忙
    timeout = 10000;
    while(I2C_GetFlagStatus(I2Cx, I2C_FLAG_BUSY))
    {
        if(--timeout == 0)
        {
            return 1;  // 总线忙超时
        }
    }

    // 发送起始信号
    I2C_GenerateSTART(I2Cx, ENABLE);
    timeout = 10000;
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_MODE_SELECT))
    {
        if(--timeout == 0)
        {
            I2C_GenerateSTOP(I2Cx, ENABLE);
            return 1;  // 超时失败
        }
    }

    // 发送设备地址(写模式)
    I2C_Send7bitAddress(I2Cx, (tca_addr << 1), I2C_Direction_Transmitter);
    timeout = 10000;
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
    {
        if(--timeout == 0)
        {
            I2C_GenerateSTOP(I2Cx, ENABLE);
            return 1;  // 超时失败
        }
    }

    // 发送通道选择字节
    I2C_SendData(I2Cx, channel);
    timeout = 10000;
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
    {
        if(--timeout == 0)
        {
            I2C_GenerateSTOP(I2Cx, ENABLE);
            return 1;  // 超时失败
        }
    }

    // 发送停止信号
    I2C_GenerateSTOP(I2Cx, ENABLE);

    return 0;  // 返回成功
}

/**
  * 函    数:TCA9548A读取当前通道状态
  * 参    数:i2c_id I2C外设编号,0表示I2C1,1表示I2C2
  *         tca_addr TCA9548A的I2C地址,范围:0x70~0x77
  *         channel 指向存储当前通道状态的变量
  * 返 回 值:0表示成功,1表示失败
  * 说    明:读取TCA9548A的控制寄存器,获取当前通道状态
  */
uint8_t TCA9548A_ReadChannel(uint8_t i2c_id, uint8_t tca_addr, uint8_t *channel)
{
    I2C_TypeDef* I2Cx = (i2c_id == 0) ? I2C1 : I2C2;
    uint32_t timeout;

    // 检查I2C总线是否忙
    timeout = 10000;
    while(I2C_GetFlagStatus(I2Cx, I2C_FLAG_BUSY))
    {
        if(--timeout == 0)
        {
            return 1;  // 总线忙超时
        }
    }

    // 发送起始信号
    I2C_GenerateSTART(I2Cx, ENABLE);
    timeout = 10000;
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_MODE_SELECT))
    {
        if(--timeout == 0)
        {
            I2C_GenerateSTOP(I2Cx, ENABLE);
            return 1;  // 超时失败
        }
    }

    // 发送设备地址(读模式)
    I2C_Send7bitAddress(I2Cx, (tca_addr << 1), I2C_Direction_Receiver);
    timeout = 10000;
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED))
    {
        if(--timeout == 0)
        {
            I2C_GenerateSTOP(I2Cx, ENABLE);
            return 1;  // 超时失败
        }
    }

    // 单字节接收序列：禁用ACK并生成STOP
    I2C_AcknowledgeConfig(I2Cx, DISABLE);
    I2C_GenerateSTOP(I2Cx, ENABLE);

    // 接收通道状态字节
    timeout = 10000;
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_RECEIVED))
    {
        if(--timeout == 0)
        {
            I2C_AcknowledgeConfig(I2Cx, ENABLE);
            return 1;  // 超时失败
        }
    }
    *channel = I2C_ReceiveData(I2Cx);

    // 恢复ACK配置
    I2C_AcknowledgeConfig(I2Cx, ENABLE);

    return 0;  // 返回成功
}

/**
  * 函    数:TCA9548A禁用所有通道
  * 参    数:i2c_id I2C外设编号,0表示I2C1,1表示I2C2
  *         tca_addr TCA9548A的I2C地址,范围:0x70~0x77
  * 返 回 值:0表示成功,1表示失败
  * 说    明:向TCA9548A写入0x00,关闭所有通道
  */
uint8_t TCA9548A_DisableAllChannels(uint8_t i2c_id, uint8_t tca_addr)
{
	return TCA9548A_SelectChannel(i2c_id, tca_addr, TCA9548A_CH_NONE);
}
