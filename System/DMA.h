#ifndef __DMA_H
#define __DMA_H
#include "stm32f10x.h"

void dma_Init(uint32_t AddrA,uint32_t AddrB,uint16_t size);
void dma_Transfer(void);
void USART_DMA_Config(void);
void USART_DMA_Send(uint8_t *buffer,uint16_t size);
#endif
