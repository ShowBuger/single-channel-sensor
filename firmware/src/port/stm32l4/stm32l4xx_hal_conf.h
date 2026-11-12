#ifndef STM32L4XX_HAL_CONF_H
#define STM32L4XX_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Module selection: only enable what we use ---- */
#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_IWDG_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

/* ---- Oscillator values ---- */
#define HSI_VALUE               16000000U   /* Internal 16MHz oscillator */
#define HSE_VALUE               8000000U    /* Not used, but keep for compatibility */
#define HSE_STARTUP_TIMEOUT     100U
#define LSI_VALUE               32000U      /* L4 series LSI is 32kHz */
#define LSE_VALUE               32768U
#define LSE_STARTUP_TIMEOUT     5000U
#define MSI_VALUE               4000000U
#define HSI48_VALUE             48000000U   /* Referenced by HAL RCC_Ex; not used */
#define EXTERNAL_SAI1_CLOCK_VALUE 2097000U  /* Referenced by HAL RCC_Ex; not used */

/* ---- VDD / tick ---- */
#define VDD_VALUE               3300U
#define TICK_INT_PRIORITY       0x0FU
#define USE_RTOS                0U
#define PREFETCH_ENABLE         1U
#define INSTRUCTION_CACHE_ENABLE 1U
#define DATA_CACHE_ENABLE       1U

/* ---- Assert ---- */
#ifdef USE_FULL_ASSERT
  #define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
  void assert_failed(uint8_t *file, uint32_t line);
#else
  #define assert_param(expr) ((void)0U)
#endif

/* ---- Device header (brings in stdint.h, CMSIS, register map) ---- */
#include "stm32l4xx.h"

/* ---- HAL module headers ---- */
#include "stm32l4xx_hal_rcc.h"
#include "stm32l4xx_hal_rcc_ex.h"
#include "stm32l4xx_hal_gpio.h"
#include "stm32l4xx_hal_gpio_ex.h"
#include "stm32l4xx_hal_exti.h"
#include "stm32l4xx_hal_dma.h"
#include "stm32l4xx_hal_dma_ex.h"
#include "stm32l4xx_hal_flash.h"
#include "stm32l4xx_hal_flash_ex.h"
#include "stm32l4xx_hal_pwr.h"
#include "stm32l4xx_hal_pwr_ex.h"
#include "stm32l4xx_hal_cortex.h"
#include "stm32l4xx_hal_uart.h"
#include "stm32l4xx_hal_uart_ex.h"
#include "stm32l4xx_hal_iwdg.h"

#ifdef __cplusplus
}
#endif

#endif /* STM32L4XX_HAL_CONF_H */
