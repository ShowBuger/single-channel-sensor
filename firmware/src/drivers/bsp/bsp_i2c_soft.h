#ifndef __SOFTI2C_H
#define __SOFTI2C_H

#include "stm32f10x.h"

void SoftI2C_Init(void);
void IIC_Start(uint8_t iic_index);
void IIC_Stop(uint8_t iic_index);
void I2C_WriteByte(uint8_t iic_index, uint8_t data);
uint8_t I2C_ReadByte(uint8_t iic_index);
uint8_t I2C_WaitAck(uint8_t iic_index);
void I2C_SendAck(uint8_t iic_index, uint8_t ack);
void I2C_ScanDevices(uint8_t iic_index);

#endif
