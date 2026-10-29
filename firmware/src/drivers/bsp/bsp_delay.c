#include "bsp_delay.h"
#include "stm32h7xx_hal.h"

/**
 * @brief  Microsecond delay using DWT cycle counter
 * @param  us  Delay in microseconds
 */
void Delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    /* DWT counts CPU cycles; on H7 HCLK = CPU/2, so use SystemCoreClock */
    uint32_t ticks = us * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - start) < ticks);
}

/**
 * @brief  Millisecond delay using HAL_Delay
 * @param  ms  Delay in milliseconds
 */
void Delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

/**
 * @brief  Second delay
 * @param  s  Delay in seconds
 */
void Delay_s(uint32_t s)
{
    while (s--)
    {
        HAL_Delay(1000);
    }
}

/**
 * @brief  Enable DWT cycle counter (call once at startup)
 */
void Delay_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->LAR    = 0xC5ACCE55U;   /* Cortex-M7: unlock DWT for software access */
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}
