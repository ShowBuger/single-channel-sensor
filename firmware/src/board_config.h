#ifndef __BOARD_CONFIG_H
#define __BOARD_CONFIG_H

/**
 * @file    board_config.h
 * @brief   Board-level hardware configuration — STM32L432KCU6 (single-channel sensor board)
 *
 * When porting to a new MCU or board revision, only this file needs to change.
 * All BSP drivers read pin/peripheral assignments from these macros.
 */

#include "stm32l4xx_hal.h"

/* =========================================================================
 * MCU identity
 * ========================================================================= */
#define BOARD_MCU_FREQ_HZ       80000000U   /* PLL target: HSI16 * 10 / 2 = 80 MHz */

/* =========================================================================
 * Software I2C bus (PA9 = SCL, PA10 = SDA)
 * ========================================================================= */
#define BSP_I2C_SCL_PORT        GPIOA
#define BSP_I2C_SCL_PIN         GPIO_PIN_9
#define BSP_I2C_SDA_PORT        GPIOA
#define BSP_I2C_SDA_PIN         GPIO_PIN_10
/** Enable GPIO clock for both SCL and SDA (same port here). */
#define BSP_I2C_GPIO_CLK_EN()   __HAL_RCC_GPIOA_CLK_ENABLE()

/* =========================================================================
 * UART (USART1: TX = PB6, RX = PB7)
 * ========================================================================= */
#define BSP_UART_INSTANCE           USART1
#define BSP_UART_BAUD               921600U   /* Max stable rate: 8x faster than 115200 */

#define BSP_UART_TX_PORT            GPIOB
#define BSP_UART_TX_PIN             GPIO_PIN_6
#define BSP_UART_RX_PORT            GPIOB
#define BSP_UART_RX_PIN             GPIO_PIN_7

#define BSP_UART_IRQn               USART1_IRQn
/** Map to the actual IRQ handler symbol expected by the vector table. */
#define BSP_UART_IRQHandler         USART1_IRQHandler

#define BSP_UART_GPIO_CLK_EN()      __HAL_RCC_GPIOB_CLK_ENABLE()
#define BSP_UART_PERIPH_CLK_EN()    __HAL_RCC_USART1_CLK_ENABLE()

/* =========================================================================
 * Independent Watchdog (IWDG)
 * 32 kHz LSI / prescaler 32 → 1 kHz tick → reload 1000 ≈ 1 s timeout
 * ========================================================================= */
#define BSP_IWDG_PRESCALER          IWDG_PRESCALER_32
#define BSP_IWDG_RELOAD             1000U

/* =========================================================================
 * Flash — persistent configuration storage
 * STM32L432KCU6: 256 KB Flash, 2 KB pages → last page = 0x0803F000
 * ========================================================================= */
#define CONFIG_FLASH_PAGE_ADDR      0x0803F000U
#define CONFIG_FLASH_PAGE_SIZE      2048U   /* bytes — 2 KB page on L432 */

#endif /* __BOARD_CONFIG_H */
