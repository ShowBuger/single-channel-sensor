#ifndef __BOARD_CONFIG_H
#define __BOARD_CONFIG_H

/**
 * @file    board_config.h
 * @brief   Board-level hardware configuration — STM32H743ZIT6 (single-channel sensor board)
 *
 * When porting to a new MCU or board revision, only this file needs to change.
 * All BSP drivers read pin/peripheral assignments from these macros.
 */

#include "stm32h7xx_hal.h"

/* =========================================================================
 * MCU identity
 * ========================================================================= */
#define BOARD_MCU_FREQ_HZ       400000000U  /* CPU: HSI64 / 4 * 50 / 2 = 400 MHz (VOS1) */
#define BOARD_HCLK_FREQ_HZ      200000000U  /* AXI/AHB = CPU / 2 */

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
 * AK09974C (MA03) — sensor I/O level and I2C addresses
 * VIO must match the sensor's I/O rail: AK09974C_VIO_1V8 or AK09974C_VIO_1V2.
 * Address follows IF1/IF2 wiring: 0x21 = IF1->SDA/IF2->SCL, 0x22 = swapped.
 * ========================================================================= */
#define BOARD_AK09974C_VIO          AK09974C_VIO_1V8
#define BOARD_AK09974C_ACTUAL_ADDR  AK09974C_I2C_ADDR_CASE1
#define BOARD_AK09974C_REF_ADDR     AK09974C_I2C_ADDR_CASE2

/* =========================================================================
 * UART (USART1: TX = PB6, RX = PB7, AF7)
 * USART1 kernel clock defaults to PCLK2 (100 MHz): 921600 baud error < 0.1%
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
#define BSP_UART_GPIO_AF            GPIO_AF7_USART1

/* =========================================================================
 * Independent Watchdog (IWDG1)
 * 32 kHz LSI / prescaler 32 → 1 kHz tick → reload 1000 ≈ 1 s timeout
 * ========================================================================= */
#define BSP_IWDG_INSTANCE           IWDG1
#define BSP_IWDG_PRESCALER          IWDG_PRESCALER_32
#define BSP_IWDG_RELOAD             1000U

/* =========================================================================
 * Flash — persistent configuration storage
 * STM32H743ZIT6: 2 MB Flash, 2 banks x 8 sectors x 128 KB.
 * Last sector = Bank 2 / sector 7 = 0x081E0000 (excluded in linker script).
 * Programming unit is a 256-bit flash word (32 bytes).
 * ========================================================================= */
#define CONFIG_FLASH_PAGE_ADDR      0x081E0000U
#define CONFIG_FLASH_PAGE_SIZE      (128U * 1024U)  /* bytes — 128 KB sector on H743 */
#define CONFIG_FLASH_BANK           FLASH_BANK_2
#define CONFIG_FLASH_SECTOR         FLASH_SECTOR_7

#endif /* __BOARD_CONFIG_H */
