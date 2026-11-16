#ifndef BOARD_PINS_H
#define BOARD_PINS_H

/**
 * @file  board_pins.h
 * @brief MA03 on STM32H743ZIT6 — pin and peripheral assignments.
 *
 * Uses STM32H7 HAL types; include only from src/port/stm32h7/.
 */

#include "stm32h7xx_hal.h"

/* Clock: HSI64 / 4 * 50 / 2 = 400 MHz CPU (VOS1), HCLK = 200 MHz */
#define BOARD_MCU_FREQ_HZ        400000000U

/* Software I2C bus 0: PA9 = SCL, PA10 = SDA (1V8 side through PCA9306).
 * On H743 these pins have no I2C alternate function. */
#define BOARD_SWI2C0_SCL_PORT    GPIOA
#define BOARD_SWI2C0_SCL_PIN     GPIO_PIN_9
#define BOARD_SWI2C0_SDA_PORT    GPIOA
#define BOARD_SWI2C0_SDA_PIN     GPIO_PIN_10
#define BOARD_SWI2C_CLK_EN()     __HAL_RCC_GPIOA_CLK_ENABLE()

/* Host UART: USART1, TX = PB6, RX = PB7, AF7. Kernel clock PCLK2 = 100 MHz. */
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

/* IWDG1: LSI 32 kHz / 32 = 1 kHz, reload 1000 -> ~1 s */
#define BOARD_IWDG_INSTANCE      IWDG1
#define BOARD_IWDG_PRESCALER     IWDG_PRESCALER_32
#define BOARD_IWDG_RELOAD        1000U

/* Config store: last sector of bank 2 (excluded in the linker script).
 * 2 MB Flash = 2 banks x 8 x 128 KB. Program unit = 256-bit flash word. */
#define BOARD_CFG_ADDR           0x081E0000U
#define BOARD_CFG_SIZE           (128U * 1024U)
#define BOARD_CFG_BANK           FLASH_BANK_2
#define BOARD_CFG_SECTOR         FLASH_SECTOR_7

#endif /* BOARD_PINS_H */
