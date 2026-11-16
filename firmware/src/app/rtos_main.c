/**
 * @file  rtos_main.c
 * @brief P3: Single-task FreeRTOS wrapper around the existing main loop.
 *
 * Before P3: bare-metal super loop in main.c runs cmd → sample → print → delay(100).
 * P3 goal: prove RTOS integration without changing app behavior. The entire super loop
 * moves into T_MAIN, a single task. This validates:
 *   - FreeRTOS scheduler works
 *   - HAL timebase on TIM6 works (SysTick is now RTOS-owned)
 *   - RAM overhead is within budget
 *   - Sampling jitter does not worsen
 * P4 will split T_MAIN into separate acquisition, algorithm, command, and output tasks.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "plat.h"
#include "board.h"
#include "sensor_manager.h"
#include "command_handler.h"
#include "../drivers/bsp/log_manager.h"
#include <stdio.h>

extern void IWDG_Feed(void);
extern uint8_t SENSOR_Run[1];

/* Task handle for diagnostics */
static TaskHandle_t s_main_task_handle;

/**
 * T_MAIN: The entire application logic runs here in P3.
 * This is the bare-metal main loop transplanted into a task.
 */
static void main_task(void *param)
{
    (void)param;

    LOG_I("MAIN", "T_MAIN started, entering sensor loop");

    /* Super loop: cmd → sample → print → delay, identical to main.c while(1) */
    for (;;) {
        IWDG_Feed();
        CommandHandler_Process();

        if (SENSOR_Run[0] == 1) {
            get_sensor_data();
            print_processed_sensor_data(0);
            /* P3: vTaskDelay instead of plat_delay_ms, but behavior is identical (100 ms wait) */
            vTaskDelay(pdMS_TO_TICKS(100));
        } else {
            /* Sensor not running: yield CPU to avoid busy-wait */
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

/**
 * rtos_main: Called from main() after platform init, starts the scheduler.
 * Does not return (scheduler takes over).
 */
void rtos_main(void)
{
    BaseType_t ret;

    LOG_I("RTOS", "FreeRTOS V%s starting (heap: %u bytes)", tskKERNEL_VERSION_NUMBER, configTOTAL_HEAP_SIZE);

    /* Create the main task with generous stack for P3 (will tune in P4 after measuring watermark) */
    ret = xTaskCreate(main_task,
                      "T_MAIN",
                      512,  /* stack: 2 KB (512 words) — the entire app runs here, so it needs the old stack + margin */
                      NULL,
                      tskIDLE_PRIORITY + 2,
                      &s_main_task_handle);

    if (ret != pdPASS) {
        LOG_E("RTOS", "Task creation failed");
        plat_sys_reset();
    }

    /* Start the scheduler: does not return unless heap exhausted */
    vTaskStartScheduler();

    /* Scheduler start failed (e.g., heap too small for idle task) */
    LOG_E("RTOS", "Scheduler start failed");
    plat_sys_reset();
}

/**
 * Idle hook: called when no task is ready (not used in P3, may add sleep in P7)
 */
void vApplicationIdleHook(void)
{
    /* Feed the watchdog in idle so the system doesn't reset if T_MAIN blocks.
     * In P4, T_SUP will take over watchdog duty. */
    IWDG_Feed();
}

/**
 * Diagnostics: query task stack high-water mark (called from ML STATUS or startup).
 * Returns unused stack in words. Call this after the system has been running for a while.
 */
uint32_t rtos_get_main_task_stack_free(void)
{
    if (s_main_task_handle == NULL) return 0;
    return uxTaskGetStackHighWaterMark(s_main_task_handle);
}

/**
 * Diagnostics: heap watermark. Returns bytes of heap that were never allocated.
 */
size_t rtos_get_heap_free_minimum(void)
{
    return xPortGetMinimumEverFreeHeapSize();
}
