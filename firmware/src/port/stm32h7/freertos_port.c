/**
 * @file  freertos_port.c
 * @brief FreeRTOS port support for STM32H7 — hooks, assertions, and heap monitoring.
 */

#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

/* Called when configASSERT() fails */
void vAssertCalled(const char *file, uint32_t line)
{
    taskDISABLE_INTERRUPTS();
    printf("ASSERT: %s:%lu\r\n", file ? file : "?", line);
    for (;;) {
        __asm volatile ("nop");
    }
}

/* Called when heap allocation fails */
void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    printf("FATAL: Heap allocation failed\r\n");
    for (;;) {
        __asm volatile ("nop");
    }
}

/* Called on stack overflow (configCHECK_FOR_STACK_OVERFLOW = 2) */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    taskDISABLE_INTERRUPTS();
    printf("FATAL: Stack overflow in task '%s'\r\n", pcTaskName);
    for (;;) {
        __asm volatile ("nop");
    }
}

/* Heap watermark query: xPortGetMinimumEverFreeHeapSize() is provided by heap_4.c,
 * so we don't redefine it here. Just declare it for reference. */
extern size_t xPortGetMinimumEverFreeHeapSize(void);
