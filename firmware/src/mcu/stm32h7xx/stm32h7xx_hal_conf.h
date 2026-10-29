#ifndef STM32H7XX_HAL_CONF_H
#define STM32H7XX_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Module selection: only enable what we use ---- */
#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_MDMA_MODULE_ENABLED     /* Referenced by HAL DMA on H7 */
#define HAL_EXTI_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_IWDG_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

/* ---- Oscillator values ---- */
#define HSE_VALUE               25000000U   /* Not used, but keep for compatibility */
#define HSE_STARTUP_TIMEOUT     100U
#define CSI_VALUE               4000000U
#define HSI_VALUE               64000000U   /* Internal 64MHz oscillator (H7) */
#define LSE_VALUE               32768U
#define LSE_STARTUP_TIMEOUT     5000U
#define LSI_VALUE               32000U      /* H7 series LSI is 32kHz */
#define EXTERNAL_CLOCK_VALUE    12288000U   /* Referenced by HAL RCC_Ex; not used */

/* ---- VDD / tick ---- */
#define VDD_VALUE               3300U
#define TICK_INT_PRIORITY       0x0FU
#define USE_RTOS                0U
#define USE_SD_TRANSCEIVER      0U
#define USE_SPI_CRC             0U
#define USE_FLASH_ECC           0U

#define USE_HAL_UART_REGISTER_CALLBACKS 0U

/* ---- Assert ---- */
#ifdef USE_FULL_ASSERT
  #define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
  void assert_failed(uint8_t *file, uint32_t line);
#else
  #define assert_param(expr) ((void)0U)
#endif

/* ---- Device header (brings in stdint.h, CMSIS, register map) ---- */
#include "stm32h7xx.h"

/* ---- HAL module headers ---- */
#include "stm32h7xx_hal_rcc.h"
#include "stm32h7xx_hal_gpio.h"
#include "stm32h7xx_hal_dma.h"
#include "stm32h7xx_hal_mdma.h"
#include "stm32h7xx_hal_exti.h"
#include "stm32h7xx_hal_cortex.h"
#include "stm32h7xx_hal_flash.h"
#include "stm32h7xx_hal_pwr.h"
#include "stm32h7xx_hal_uart.h"
#include "stm32h7xx_hal_iwdg.h"

#ifdef __cplusplus
}
#endif

#endif /* STM32H7XX_HAL_CONF_H */
