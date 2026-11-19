/* STM32H7 port: millisecond tick (HAL/SysTick) and DWT cycle counter. */
#include "plat.h"
#include "board_pins.h"

void plat_time_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->LAR    = 0xC5ACCE55U;          /* Cortex-M7: unlock DWT for software access */
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t plat_tick_ms(void)            { return HAL_GetTick(); }
uint32_t plat_time_us(void)
{
    /* TIM6 runs at 1 MHz and drives HAL's millisecond tick. Account for an
     * overflow whose ISR is pending, including while RTOS masks interrupts. */
    uint32_t irq=__get_PRIMASK();
    __disable_irq();
    uint32_t ms=HAL_GetTick(), before=TIM6->CNT;
    uint32_t pending=TIM6->SR & TIM_SR_UIF, count=TIM6->CNT;
    if(pending || count<before) ms++;
    __set_PRIMASK(irq);
    return ms*1000U+count;
}
void     plat_delay_ms(uint32_t ms)    { HAL_Delay(ms); }
uint32_t plat_cycles(void)             { return DWT->CYCCNT; }
uint32_t plat_cycles_per_us(void)      { return SystemCoreClock / 1000000U; }

void plat_delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000U);   /* DWT counts CPU cycles */
    while ((DWT->CYCCNT - start) < ticks) {}
}
