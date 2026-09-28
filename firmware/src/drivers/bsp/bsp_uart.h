#ifndef __SERIAL_H
#define __SERIAL_H

#include <stdio.h>
#include <stdint.h>

void Serial_Init(void);
void Serial_SendByte(uint8_t Byte);
void Serial_SendArray(uint8_t *Array, uint16_t Length);
void Serial_SendString(char *String);
void Serial_SendNumber(uint32_t Number, uint8_t Length);
void Serial_Printf(char *format, ...);
void Serial_SendFloat(float value);
uint8_t Serial_GetRxFlag(void);
uint8_t Serial_GetRxData(void);

// 命令处理相关
void Serial_ProcessCommand(void);
uint8_t Serial_GetCommandFlag(void);
char* Serial_GetCommandBuffer(void);

#endif
