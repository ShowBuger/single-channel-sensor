#ifndef BOARD_PINS_H
#define BOARD_PINS_H

/**
 * @file  board_pins.h
 * @brief MA01 on STM32L432KCU6 — pin and peripheral assignments.
 *
 * Uses STM32L4 HAL types; include only from src/port/stm32l4/.
 */

#include "stm32l4xx_hal.h"

/* Clock: HSI16 * 10 / 2 = 80 MHz */
#define BOARD_MCU_FREQ_HZ        80000000U

/* Software I2C bus 0: PA9 = SCL, PA10 = SDA (I2C1 AF exists on L432, unused) */
#define BOARD_SWI2C0_SCL_PORT    GPIOA
#define BOARD_SWI2C0_SCL_PIN     GPIO_PIN_9
#define BOARD_SWI2C0_SDA_PORT    GPIOA
#define BOARD_SWI2C0_SDA_PIN     GPIO_PIN_10
#define BOARD_SWI2C_CLK_EN()     __HAL_RCC_GPIOA_CLK_ENABLE()

/* Host UART: USART1, TX = PB6, RX = PB7, AF7 */
#define BOARD_UART_INSTANCE      USART1
#define BOARD_UART_TX_PORT       GPIOB
#define BOARD_UART_TX_PIN        GPIO_PIN_6
#define BOARD_UART_RX_PORT       GPIOB
#define BOARD_UART_RX_PIN        GPIO_PIN_7
#define BOARD_UART_AF            GPIO_AF7_USART1
#define BOARD_UART_IRQn          USART1_IRQn
#define BOARD_UART_IRQHandler    USART1_IRQHandler
#define BOARD_UART_GPIO_CLK_EN() __HAL_RCC_GPIOB_CLK_ENABLE()
#define BOARD_UART_CLK_EN()      __HAL_RCC_USART1_CLK_ENABLE()

/* IWDG: LSI 32 kHz / 32 = 1 kHz, reload 1250 -> ~1.25 s (as on the MA01 branch) */
#define BOARD_IWDG_PRESCALER     IWDG_PRESCALER_32
#define BOARD_IWDG_RELOAD        1250U

/* Config store: last 2 KB page of 256 KB Flash (excluded in the linker script). */
#define BOARD_CFG_ADDR           0x0803F000U
#define BOARD_CFG_SIZE           2048U

#endif /* BOARD_PINS_H */
