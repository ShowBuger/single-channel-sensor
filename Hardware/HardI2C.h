#ifndef __HARDI2C_H
#define __HARDI2C_H

void HardI2C_Init(void);
void HardI2C_Start(uint8_t id);
void HardI2C_Stop(uint8_t id);
void HardI2C_SendByte(uint8_t id, uint8_t Byte);
uint8_t HardI2C_ReceiveByte(uint8_t id);
void HardI2C_SendAck(uint8_t id, uint8_t AckBit);
uint8_t HardI2C_WaitAck(uint8_t id);
void HardI2C_Scan(uint8_t id);

#endif
