#include "DMA.h"

uint16_t DMA_Size = 4;
void dma_Init(uint32_t AddrA,uint32_t AddrB,uint16_t size){
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1,ENABLE); 
    DMA_InitTypeDef DMA_InitStructure;
    DMA_InitStructure.DMA_PeripheralBaseAddr = AddrA; 
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;  
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Enable;     
    DMA_InitStructure.DMA_MemoryBaseAddr = AddrB;     
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;     
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable; 
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = size;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Enable;
    DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);
    DMA_Cmd(DMA1_Channel1,DISABLE);
}
void dma_Transfer(void){
    DMA_Cmd(DMA1_Channel1,DISABLE);
    DMA_SetCurrDataCounter(DMA1_Channel1,DMA_Size);
    DMA_Cmd(DMA1_Channel1,ENABLE);
    
    while(DMA_GetFlagStatus(DMA1_FLAG_TC1) == RESET);
    DMA_ClearFlag(DMA1_FLAG_TC1);
}
void USART_DMA_Config(void)
{
 
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
    
 
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA,&GPIO_InitStructure);
    

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA,&GPIO_InitStructure);

    USART_InitTypeDef USART_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1,ENABLE);
    
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    
    USART_Init(USART1,&USART_InitStructure);
    

    DMA_InitTypeDef DMA_InitStructure;

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1,ENABLE);
    
    DMA_DeInit(DMA1_Channel4);
    
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR; 
    DMA_InitStructure.DMA_MemoryBaseAddr = 0;    
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST; 
    DMA_InitStructure.DMA_BufferSize = 0; 
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable; 
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable; 
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte; 
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal; 
    DMA_InitStructure.DMA_Priority = DMA_Priority_Medium; 
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable; 
    
    DMA_Init(DMA1_Channel4,&DMA_InitStructure);

    USART_DMACmd(USART1,USART_DMAReq_Tx,ENABLE);

    USART_Cmd(USART1,ENABLE);
    

    NVIC_InitTypeDef NVIC_InitStructure;
    
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    DMA_ITConfig(DMA1_Channel4,DMA_IT_TC,ENABLE);
}
void USART_DMA_Send(uint8_t *buffer,uint16_t size)
{

    DMA_Cmd(DMA1_Channel4,DISABLE);
    

    DMA1_Channel4->CMAR = (uint32_t)buffer;
    DMA1_Channel4->CNDTR = size;
    

    DMA_ClearFlag(DMA1_FLAG_TC4);
    
    DMA_Cmd(DMA1_Channel4,ENABLE);
    
}

void DMA1_Channel4_IRQHandler(void)
{
    if(DMA_GetITStatus(DMA1_IT_TC4) != RESET)
    {
     
        DMA_ClearITPendingBit(DMA1_IT_TC4);
    }
}
