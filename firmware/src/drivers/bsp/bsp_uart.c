#include "bsp_uart.h"
#include "stm32l4xx_hal.h"
#include "board_config.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static UART_HandleTypeDef huart1;

uint8_t Serial_RxData;
uint8_t Serial_RxFlag;

#define CMD_BUFFER_SIZE 64
static char    cmdBuffer[CMD_BUFFER_SIZE];
static uint8_t cmdIndex     = 0;
static uint8_t cmdReadyFlag = 0;

void Serial_Init(void)
{
    /* GPIO + peripheral clocks */
    BSP_UART_GPIO_CLK_EN();
    BSP_UART_PERIPH_CLK_EN();

    GPIO_InitTypeDef gpio = {0};

    /* TX: PB6, AF7 (USART1) - STM32L4 requires explicit Alternate function */
    gpio.Pin       = BSP_UART_TX_PIN;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART1;  /* STM32L4 specific */
    HAL_GPIO_Init(BSP_UART_TX_PORT, &gpio);

    /* RX: PB7, AF7 (USART1) */
    gpio.Pin       = BSP_UART_RX_PIN;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Alternate = GPIO_AF7_USART1;  /* STM32L4 specific */
    HAL_GPIO_Init(BSP_UART_RX_PORT, &gpio);

    huart1.Instance          = BSP_UART_INSTANCE;
    huart1.Init.BaudRate     = BSP_UART_BAUD;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);

    /* Enable RX interrupt */
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);

    HAL_NVIC_SetPriority(BSP_UART_IRQn, 1, 1);
    HAL_NVIC_EnableIRQ(BSP_UART_IRQn);
}

void Serial_SendByte(uint8_t byte)
{
    HAL_UART_Transmit(&huart1, &byte, 1, HAL_MAX_DELAY);
}

void Serial_SendArray(uint8_t *arr, uint16_t len)
{
    HAL_UART_Transmit(&huart1, arr, len, HAL_MAX_DELAY);
}

void Serial_SendString(char *str)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)str, (uint16_t)strlen(str), HAL_MAX_DELAY);
}

void Serial_SendNumber(uint32_t num, uint8_t len)
{
    uint8_t i;
    uint32_t pow10 = 1;
    for (i = 1; i < len; i++) pow10 *= 10;
    for (i = 0; i < len; i++)
    {
        Serial_SendByte((uint8_t)(num / pow10 % 10 + '0'));
        pow10 /= 10;
    }
}

void Serial_SendFloat(float val)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.4f", val);
    Serial_SendString(buf);
}

void Serial_Printf(char *fmt, ...)
{
    char buf[100];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial_SendString(buf);
}

/* Redirect printf to USART1 */
int fputc(int ch, FILE *f)
{
    (void)f;
    Serial_SendByte((uint8_t)ch);
    return ch;
}

uint8_t Serial_GetRxFlag(void)
{
    if (Serial_RxFlag)
    {
        Serial_RxFlag = 0;
        return 1;
    }
    return 0;
}

uint8_t Serial_GetRxData(void)
{
    return Serial_RxData;
}

uint8_t Serial_GetCommandFlag(void)
{
    if (cmdReadyFlag)
    {
        cmdReadyFlag = 0;
        return 1;
    }
    return 0;
}

char *Serial_GetCommandBuffer(void)
{
    return cmdBuffer;
}

void Serial_ProcessCommand(void)
{
    /* Command processing delegated to main.c */
}

/* UART IRQ handler — symbol resolved from board_config.h */
void BSP_UART_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE))
    {
        /* 读RDR即清除RXNE */
        Serial_RxData = (uint8_t)(huart1.Instance->RDR & 0xFF);
        Serial_RxFlag = 1;

        if (Serial_RxData == '\r' || Serial_RxData == '\n')
        {
            if (cmdIndex > 0)
            {
                cmdBuffer[cmdIndex] = '\0';
                cmdReadyFlag = 1;
                cmdIndex     = 0;
            }
        }
        else if (cmdIndex < CMD_BUFFER_SIZE - 1)
        {
            cmdBuffer[cmdIndex++] = Serial_RxData;
        }
    }

    HAL_UART_IRQHandler(&huart1);
}
