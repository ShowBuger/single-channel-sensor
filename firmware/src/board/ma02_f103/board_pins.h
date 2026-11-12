#ifndef BOARD_PINS_H
#define BOARD_PINS_H

/**
 * @file  board_pins.h
 * @brief MA02 on STM32F103C8T6 — pin and peripheral assignments.
 *
 * Uses STM32F1 HAL types; include only from src/port/stm32f1/.
 */

#include "stm32f1xx_hal.h"

/* Clock: HSE 8 MHz * 9 = 72 MHz, falls back to HSI/2 * 16 = 64 MHz */
#define BOARD_MCU_FREQ_HZ        72000000U

/* Software I2C bus 0: PB6 = SCL, PB7 = SDA */
#define BOARD_SWI2C0_SCL_PORT    GPIOB
#define BOARD_SWI2C0_SCL_PIN     GPIO_PIN_6
#define BOARD_SWI2C0_SDA_PORT    GPIOB
#define BOARD_SWI2C0_SDA_PIN     GPIO_PIN_7
#define BOARD_SWI2C_CLK_EN()     __HAL_RCC_GPIOB_CLK_ENABLE()

/* Host UART: USART1, TX = PA9, RX = PA10 (F1 has fixed AF mapping) */
#define BOARD_UART_INSTANCE      USART1
#define BOARD_UART_TX_PORT       GPIOA
#define BOARD_UART_TX_PIN        GPIO_PIN_9
#define BOARD_UART_RX_PORT       GPIOA
#define BOARD_UART_RX_PIN        GPIO_PIN_10
#define BOARD_UART_IRQn          USART1_IRQn
#define BOARD_UART_IRQHandler    USART1_IRQHandler
#define BOARD_UART_GPIO_CLK_EN() __HAL_RCC_GPIOA_CLK_ENABLE()
#define BOARD_UART_CLK_EN()      __HAL_RCC_USART1_CLK_ENABLE()

/* IWDG: LSI 40 kHz / 32 = 1.25 kHz, reload 1250 -> ~1 s */
#define BOARD_IWDG_PRESCALER     IWDG_PRESCALER_32
#define BOARD_IWDG_RELOAD        1250U

/* Config store: last 1 KB page of 64 KB Flash (excluded in the linker script). */
#define BOARD_CFG_ADDR           0x0800FC00U
#define BOARD_CFG_SIZE           1024U

#endif /* BOARD_PINS_H */
