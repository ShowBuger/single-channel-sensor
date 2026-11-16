#include "bsp_uart.h"
#include "plat.h"
#include "board.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

uint8_t Serial_RxData;
uint8_t Serial_RxFlag;

#define CMD_BUFFER_SIZE 64
static char             cmdBuffer[CMD_BUFFER_SIZE];
static uint8_t          cmdIndex     = 0;
static volatile uint8_t cmdReadyFlag = 0;

/* Interrupt context: assemble one command line, terminated by CR or LF. */
static void on_rx_byte(uint8_t b)
{
    Serial_RxData = b;
    Serial_RxFlag = 1;
    if (b == '\r' || b == '\n') {
        if (cmdIndex > 0) {
            cmdBuffer[cmdIndex] = '\0';
            cmdReadyFlag = 1;
            cmdIndex     = 0;
        }
    } else if (cmdIndex < CMD_BUFFER_SIZE - 1) {
        cmdBuffer[cmdIndex++] = (char)b;
    }
}

void Serial_Init(void)
{
    plat_uart_init(BOARD_UART_BAUD, on_rx_byte);
}

void Serial_SendByte(uint8_t byte)
{
    plat_uart_write(&byte, 1);
}

void Serial_SendArray(uint8_t *arr, uint16_t len)
{
    plat_uart_write(arr, len);
}

void Serial_SendString(char *str)
{
    plat_uart_write((const uint8_t *)str, (uint16_t)strlen(str));
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
