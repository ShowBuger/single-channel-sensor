/* STM32F1 port: millisecond tick (HAL/SysTick) and DWT cycle counter. */
#include "plat.h"
#include "board_pins.h"

void plat_time_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t plat_tick_ms(void)          { return HAL_GetTick(); }
void     plat_delay_ms(uint32_t ms)  { HAL_Delay(ms); }
uint32_t plat_cycles(void)           { return DWT->CYCCNT; }
uint32_t plat_cycles_per_us(void)    { return HAL_RCC_GetHCLKFreq() / 1000000U; }

void plat_delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (HAL_RCC_GetHCLKFreq() / 1000000U);
    while ((DWT->CYCCNT - start) < ticks) {}
}

uint32_t plat_time_us(void)
{
    uint32_t irq=__get_PRIMASK();
    __disable_irq();
    uint32_t ms=HAL_GetTick(), before=SysTick->VAL;
    uint32_t pending=SCB->ICSR & SCB_ICSR_PENDSTSET_Msk, count=SysTick->VAL;
    if(pending || count>before) ms++;
    uint32_t fraction=(SysTick->LOAD-count)/(SystemCoreClock/1000000U);
    __set_PRIMASK(irq);
    return ms*1000U+fraction;
}
