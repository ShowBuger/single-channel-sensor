#ifndef __SERIAL_H
#define __SERIAL_H

#include <stdio.h>
#include <stdint.h>

void    Serial_Init(void);
void    Serial_SendByte(uint8_t byte);
void    Serial_SendArray(uint8_t *arr, uint16_t len);
void    Serial_SendString(char *str);
void    Serial_SendNumber(uint32_t num, uint8_t len);
void    Serial_Printf(char *fmt, ...);
void    Serial_SendFloat(float val);
uint8_t Serial_GetRxFlag(void);
uint8_t Serial_GetRxData(void);
uint8_t Serial_GetCommandFlag(void);
char   *Serial_GetCommandBuffer(void);
void    Serial_ProcessCommand(void);

#endif
