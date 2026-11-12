#include "bsp_delay.h"
#include "plat.h"

/* Thin wrappers kept for existing callers (and the emulator tests, which hook
 * these symbols). New code should call plat_* directly. */
void Delay_Init(void)        { plat_time_init(); }
void Delay_us(uint32_t us)   { plat_delay_us(us); }
void Delay_ms(uint32_t ms)   { plat_delay_ms(ms); }
void Delay_s(uint32_t s)     { while (s--) plat_delay_ms(1000); }
