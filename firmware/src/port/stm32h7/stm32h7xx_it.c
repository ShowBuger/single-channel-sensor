#include "stm32h7xx_hal.h"

/* P3: After FreeRTOS integration, SysTick/PendSV/SVC are owned by FreeRTOS.
 * These handlers are defined in the FreeRTOS port (port.c) and renamed by FreeRTOSConfig.h:
 *   #define xPortSysTickHandler  SysTick_Handler
 *   #define vPortSVCHandler      SVC_Handler
 *   #define xPortPendSVHandler   PendSV_Handler
 * We do NOT redefine them here to avoid multiple definition errors.
 * HAL timebase is now on TIM6 (see plat_timebase.c). */

void NMI_Handler(void)          {}
void HardFault_Handler(void)    { while (1); }
void MemManage_Handler(void)    { while (1); }
void BusFault_Handler(void)     { while (1); }
void UsageFault_Handler(void)   { while (1); }
void DebugMon_Handler(void)     {}

/* SVC_Handler, PendSV_Handler, SysTick_Handler are defined by FreeRTOS port.c
 * and automatically renamed to the correct symbols by FreeRTOSConfig.h macros.
 * No definitions needed here. */
