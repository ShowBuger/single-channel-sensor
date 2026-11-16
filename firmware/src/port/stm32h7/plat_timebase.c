/**
 * @file  plat_timebase.c
 * @brief HAL timebase moved to TIM6, freeing SysTick for FreeRTOS.
 *
 * P3: Before FreeRTOS integration, HAL_GetTick() and HAL_Delay() relied on SysTick.
 * FreeRTOS also needs SysTick for its scheduler tick, causing a conflict. This file
 * moves the HAL timebase to TIM6 (a basic timer), leaving SysTick exclusively for RTOS.
 *
 * TIM6 configuration:
 * - Clock: APB1 timer clock (240 MHz on H743 with default prescaler)
 * - Prescaler: set to generate 1 kHz tick (1 ms period)
 * - Priority: below configMAX_SYSCALL_INTERRUPT_PRIORITY so HAL can be called from RTOS
 *
 * This is called from plat_sys_init() before any HAL_Delay() or scheduler start.
 */

#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_tim.h"

static TIM_HandleTypeDef htim6;

/* Override the weak HAL_InitTick to use TIM6 instead of SysTick */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    RCC_ClkInitTypeDef clkconfig;
    uint32_t uwTimclock, uwAPB1Prescaler;
    uint32_t uwPrescalerValue;
    uint32_t pFLatency;

    /* Enable TIM6 clock */
    __HAL_RCC_TIM6_CLK_ENABLE();

    /* Get APB1 prescaler to calculate TIM6 input clock */
    HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);
    uwAPB1Prescaler = clkconfig.APB1CLKDivider;

    /* APB1 timer clock: if APB1 prescaler = 1, timer clock = APB1 clock; else timer clock = 2 × APB1 clock */
    if (uwAPB1Prescaler == RCC_HCLK_DIV1) {
        uwTimclock = HAL_RCC_GetPCLK1Freq();
    } else {
        uwTimclock = 2U * HAL_RCC_GetPCLK1Freq();
    }

    /* Configure TIM6 to generate 1 kHz tick (1 ms period) */
    uwPrescalerValue = (uwTimclock / 1000000U) - 1U;  /* 1 µs time base */

    htim6.Instance = TIM6;
    htim6.Init.Period = 1000U - 1U;                   /* 1000 µs = 1 ms */
    htim6.Init.Prescaler = uwPrescalerValue;
    htim6.Init.ClockDivision = 0;
    htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim6) != HAL_OK) {
        return HAL_ERROR;
    }

    /* Priority must be >= configMAX_SYSCALL_INTERRUPT_PRIORITY (5) so FromISR functions work.
     * Using priority 6 (lower number = higher priority in NVIC, but higher value in config).
     * In FreeRTOSConfig.h, configMAX_SYSCALL_INTERRUPT_PRIORITY = 5 << 4 = 80.
     * TIM6 priority = 6 << 4 = 96, which is numerically higher (lower urgency), meeting the requirement. */
    HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);

    /* Start the timer */
    if (HAL_TIM_Base_Start_IT(&htim6) != HAL_OK) {
        return HAL_ERROR;
    }

    return HAL_OK;
}

/* Override HAL_SuspendTick / HAL_ResumeTick (called by HAL_PWR APIs, not used in this app yet) */
void HAL_SuspendTick(void)
{
    __HAL_TIM_DISABLE_IT(&htim6, TIM_IT_UPDATE);
}

void HAL_ResumeTick(void)
{
    __HAL_TIM_ENABLE_IT(&htim6, TIM_IT_UPDATE);
}

/* TIM6 interrupt handler: increment HAL tick */
void TIM6_DAC_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim6);
}

/* TIM6 period elapsed callback: called by HAL_TIM_IRQHandler */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) {
        HAL_IncTick();
    }
}
