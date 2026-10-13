#ifndef __BOARD_CONFIG_H
#define __BOARD_CONFIG_H

/**
 * @file    board_config.h
 * @brief   Board-level hardware configuration — STM32F103C8T6 (single-channel sensor board)
 *
 * When porting to a new MCU or board revision, only this file needs to change.
 * All BSP drivers read pin/peripheral assignments from these macros.
 */

#include "stm32f1xx_hal.h"

/* =========================================================================
 * MCU identity
 * ========================================================================= */
#define BOARD_MCU_FREQ_HZ       72000000U   /* PLL target: 72 MHz          */

/* =========================================================================
 * Software I2C bus (PB6 = SCL, PB7 = SDA)
 * ========================================================================= */
#define BSP_I2C_SCL_PORT        GPIOB
#define BSP_I2C_SCL_PIN         GPIO_PIN_6
#define BSP_I2C_SDA_PORT        GPIOB
#define BSP_I2C_SDA_PIN         GPIO_PIN_7
/** Enable GPIO clock for both SCL and SDA (same port here). */
#define BSP_I2C_GPIO_CLK_EN()   __HAL_RCC_GPIOB_CLK_ENABLE()

/* =========================================================================
 * UART (USART1: TX = PA9, RX = PA10)
 * ========================================================================= */
#define BSP_UART_INSTANCE           USART1
#define BSP_UART_BAUD               115200U

#define BSP_UART_TX_PORT            GPIOA
#define BSP_UART_TX_PIN             GPIO_PIN_9
#define BSP_UART_RX_PORT            GPIOA
#define BSP_UART_RX_PIN             GPIO_PIN_10

#define BSP_UART_IRQn               USART1_IRQn
/** Map to the actual IRQ handler symbol expected by the vector table. */
#define BSP_UART_IRQHandler         USART1_IRQHandler

#define BSP_UART_GPIO_CLK_EN()      __HAL_RCC_GPIOA_CLK_ENABLE()
#define BSP_UART_PERIPH_CLK_EN()    __HAL_RCC_USART1_CLK_ENABLE()

/* =========================================================================
 * Independent Watchdog (IWDG)
 * 40 kHz LSI / prescaler 32 → 1.25 kHz tick → reload 1250 ≈ 1 s timeout
 * ========================================================================= */
#define BSP_IWDG_PRESCALER          IWDG_PRESCALER_32
#define BSP_IWDG_RELOAD             1250U

/* =========================================================================
 * Flash — persistent configuration storage
 * STM32F103C8T6: 64 KB Flash, 1 KB pages → last page = 0x0800FC00
 * ========================================================================= */
#define CONFIG_FLASH_PAGE_ADDR      0x0800FC00U
#define CONFIG_FLASH_PAGE_SIZE      1024U   /* bytes — 1 KB page on F103C8 */

#endif /* __BOARD_CONFIG_H */
