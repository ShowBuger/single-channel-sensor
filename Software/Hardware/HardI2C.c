#include "stm32f10x.h"                  // Device header
#include "Serial.h"
#include "Delay.h"

/**
  * 函    数:硬件I2C初始化
  * 参    数:无
  * 返 回 值:无
  * 说    明:I2C1使用PB6(SCL)和PB7(SDA), I2C2使用PB10(SCL)和PB11(SDA)
  */
void HardI2C_Init(void)
{
	/*开启时钟*/
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1 | RCC_APB1Periph_I2C2, ENABLE);	//开启I2C1和I2C2的时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);		//开启GPIOB的时钟

	/*GPIO初始化*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;				//复用开漏输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;		//I2C1: PB6(SCL), PB7(SDA)
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;	//I2C2: PB10(SCL), PB11(SDA)
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/*I2C初始化*/
	I2C_InitTypeDef I2C_InitStructure;
	I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;					//I2C模式
	I2C_InitStructure.I2C_ClockSpeed = 400000;					//时钟速度,400kHz
	I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;			//时钟占空比,Tlow/Thigh = 2
	I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;					//使能应答
	I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;	//7位地址模式
	I2C_InitStructure.I2C_OwnAddress1 = 0x00;					//STM32自身地址,从机模式下使用

	I2C_Init(I2C1, &I2C_InitStructure);							//初始化I2C1
	I2C_Init(I2C2, &I2C_InitStructure);							//初始化I2C2

	/*使能I2C*/
	I2C_Cmd(I2C1, ENABLE);										//使能I2C1
	I2C_Cmd(I2C2, ENABLE);										//使能I2C2
}

/**
  * 函    数:硬件I2C起始信号
  * 参    数:id I2C外设编号,0表示I2C1,1表示I2C2
  * 返 回 值:无
  */
void HardI2C_Start(uint8_t id)
{
	I2C_TypeDef* I2Cx = (id == 0) ? I2C1 : I2C2;
	I2C_GenerateSTART(I2Cx, ENABLE);							//生成起始信号
	while (I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS);	//等待EV5事件
}

/**
  * 函    数:硬件I2C停止信号
  * 参    数:id I2C外设编号,0表示I2C1,1表示I2C2
  * 返 回 值:无
  */
void HardI2C_Stop(uint8_t id)
{
	I2C_TypeDef* I2Cx = (id == 0) ? I2C1 : I2C2;
	I2C_GenerateSTOP(I2Cx, ENABLE);								//生成停止信号
}

/**
  * 函    数:硬件I2C发送一个字节
  * 参    数:id I2C外设编号,0表示I2C1,1表示I2C2
  *         Byte 要发送的一个字节数据,范围:0x00~0xFF
  * 返 回 值:无
  */
void HardI2C_SendByte(uint8_t id, uint8_t Byte)
{
	I2C_TypeDef* I2Cx = (id == 0) ? I2C1 : I2C2;
	I2C_SendData(I2Cx, Byte);									//发送一个字节
	while (I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_TRANSMITTING) != SUCCESS);	//等待EV8事件
}

/**
  * 函    数:硬件I2C接收一个字节
  * 参    数:id I2C外设编号,0表示I2C1,1表示I2C2
  * 返 回 值:接收到的一个字节数据,范围:0x00~0xFF
  */
uint8_t HardI2C_ReceiveByte(uint8_t id)
{
	I2C_TypeDef* I2Cx = (id == 0) ? I2C1 : I2C2;
	while (I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_RECEIVED) != SUCCESS);	//等待EV7事件
	return I2C_ReceiveData(I2Cx);								//返回接收到的数据
}

/**
  * 函    数:硬件I2C发送应答
  * 参    数:id I2C外设编号,0表示I2C1,1表示I2C2
  *         AckBit 应答位,0表示应答(ACK),1表示非应答(NACK)
  * 返 回 值:无
  */
void HardI2C_SendAck(uint8_t id, uint8_t AckBit)
{
	I2C_TypeDef* I2Cx = (id == 0) ? I2C1 : I2C2;
	if (AckBit == 0)
	{
		I2C_AcknowledgeConfig(I2Cx, ENABLE);					//应答
	}
	else
	{
		I2C_AcknowledgeConfig(I2Cx, DISABLE);					//非应答
	}
}

/**
  * 函    数:硬件I2C等待应答
  * 参    数:id I2C外设编号,0表示I2C1,1表示I2C2
  * 返 回 值:应答位,0表示收到应答,1表示未收到应答
  */
uint8_t HardI2C_WaitAck(uint8_t id)
{
	I2C_TypeDef* I2Cx = (id == 0) ? I2C1 : I2C2;
	uint16_t timeout = 0;
	while (I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_TRANSMITTED) != SUCCESS)	//等待EV8_2事件
	{
		timeout++;
		if (timeout > 50000)										//超时判断
		{
			return 1;											//返回非应答
		}
	}
	return 0;													//返回应答
}

/**
  * 函    数:扫描I2C地址
  * 参    数:id I2C外设编号,0表示I2C1,1表示I2C2
  * 返 回 值:无
  * 说    明:扫描0x01~0x7F的7位地址,发现设备通过串口打印地址
  */
void HardI2C_Scan(uint8_t id)
{
	uint8_t addr;
	Serial_Printf("I2C%d scan start\r\n", (id == 0) ? 1 : 2);
	for (addr = 1; addr < 128; addr++)
	{
		HardI2C_Start(id);
		HardI2C_SendByte(id, addr << 1); // 发送写地址
		if (HardI2C_WaitAck(id) == 0)
		{
			Serial_Printf("Found device at 0x%02X\r\n", addr);
		}
		HardI2C_Stop(id);
		Delay_ms(5);
	}
	Serial_Printf("I2C scan finished\r\n");
}
