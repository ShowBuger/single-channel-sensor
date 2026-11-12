/* STM32F1 port: host UART (blocking TX, per-byte RX interrupt). */
#include "plat.h"
#include "board_pins.h"

static UART_HandleTypeDef s_uart;
static plat_uart_rx_cb    s_on_rx;

void plat_uart_init(uint32_t baud, plat_uart_rx_cb on_rx)
{
    GPIO_InitTypeDef gpio = {0};

    s_on_rx = on_rx;
    BOARD_UART_GPIO_CLK_EN();
    BOARD_UART_CLK_EN();

    /* F1: TX is alternate-function push-pull, RX is a plain input (fixed mapping). */
    gpio.Pin   = BOARD_UART_TX_PIN;
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(BOARD_UART_TX_PORT, &gpio);
    gpio.Pin   = BOARD_UART_RX_PIN;
    gpio.Mode  = GPIO_MODE_INPUT;
    gpio.Pull  = GPIO_PULLUP;
    HAL_GPIO_Init(BOARD_UART_RX_PORT, &gpio);

    s_uart.Instance          = BOARD_UART_INSTANCE;
    s_uart.Init.BaudRate     = baud;
    s_uart.Init.WordLength   = UART_WORDLENGTH_8B;
    s_uart.Init.StopBits     = UART_STOPBITS_1;
    s_uart.Init.Parity       = UART_PARITY_NONE;
    s_uart.Init.Mode         = UART_MODE_TX_RX;
    s_uart.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    s_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&s_uart);

    __HAL_UART_ENABLE_IT(&s_uart, UART_IT_RXNE);
    HAL_NVIC_SetPriority(BOARD_UART_IRQn, 1, 1);
    HAL_NVIC_EnableIRQ(BOARD_UART_IRQn);
}

void plat_uart_write(const uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(&s_uart, (uint8_t *)data, len, HAL_MAX_DELAY);
}

void BOARD_UART_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&s_uart, UART_FLAG_RXNE)) {
        uint8_t b = (uint8_t)(s_uart.Instance->DR & 0xFFU);    /* reading DR clears RXNE */
        if (s_on_rx) s_on_rx(b);
        __HAL_UART_CLEAR_FLAG(&s_uart, UART_FLAG_RXNE);
    }
    HAL_UART_IRQHandler(&s_uart);
}
