#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*-----------------------------------------------------------
 * FreeRTOS kernel configuration for STM32H743 (Cortex-M7)
 * - 480 MHz, dual-precision FPU with lazy stacking
 * - HAL timebase moved to TIM6, SysTick owned by FreeRTOS
 * - Preemptive scheduler, tick rate 1 kHz
 *-----------------------------------------------------------*/

/* Cortex-M7 specific: Enable FPU and lazy context switching */
#define configENABLE_FPU                         1
#define configENABLE_TRUSTZONE                   0
#define configENABLE_MVE                         0

/* Core scheduler configuration */
#define configUSE_PREEMPTION                     1
#define configUSE_TIME_SLICING                   1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  0
#define configUSE_TICKLESS_IDLE                  0
#define configCPU_CLOCK_HZ                       480000000UL
#define configTICK_RATE_HZ                       1000  /* 1 ms tick, aligned with HAL */
#define configMAX_PRIORITIES                     8
#define configMINIMAL_STACK_SIZE                 128   /* words (512 bytes) */
#define configMAX_TASK_NAME_LEN                  12
#define configUSE_16_BIT_TICKS                   0
#define configIDLE_SHOULD_YIELD                  1

/* Memory allocation */
#define configTOTAL_HEAP_SIZE                    (8 * 1024)  /* 8 KB, tune in P3 */
#define configSUPPORT_STATIC_ALLOCATION          0
#define configSUPPORT_DYNAMIC_ALLOCATION         1

/* Hook functions */
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configUSE_MALLOC_FAILED_HOOK             1
#define configUSE_DAEMON_TASK_STARTUP_HOOK       0
#define configCHECK_FOR_STACK_OVERFLOW           2  /* Method 2: pattern check */

/* Co-routine (not used) */
#define configUSE_CO_ROUTINES                    0
#define configMAX_CO_ROUTINE_PRIORITIES          1

/* Software timers */
#define configUSE_TIMERS                         1
#define configTIMER_TASK_PRIORITY                3
#define configTIMER_QUEUE_LENGTH                 10
#define configTIMER_TASK_STACK_DEPTH             (configMINIMAL_STACK_SIZE * 2)

/* Optional functions */
#define INCLUDE_vTaskPrioritySet                 1
#define INCLUDE_uxTaskPriorityGet                1
#define INCLUDE_vTaskDelete                      0  /* Tasks never deleted in this app */
#define INCLUDE_vTaskSuspend                     1
#define INCLUDE_xTaskDelayUntil                  1
#define INCLUDE_vTaskDelay                       1
#define INCLUDE_xTaskGetSchedulerState           1
#define INCLUDE_xTaskGetCurrentTaskHandle        1
#define INCLUDE_uxTaskGetStackHighWaterMark      1
#define INCLUDE_xTaskGetIdleTaskHandle           0
#define INCLUDE_eTaskGetState                    1
#define INCLUDE_xTimerPendFunctionCall           1
#define INCLUDE_xTaskAbortDelay                  0
#define INCLUDE_xTaskGetHandle                   0

/* Queue functions */
#define configUSE_QUEUE_SETS                     0
#define configUSE_MUTEXES                        1
#define configUSE_RECURSIVE_MUTEXES              0
#define configUSE_COUNTING_SEMAPHORES            1
#define configUSE_TASK_NOTIFICATIONS             1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES    1

/* Interrupt nesting and priorities
 * H743 uses 4-bit priority (16 levels): NVIC_PRIORITYGROUP_4
 * configMAX_SYSCALL_INTERRUPT_PRIORITY = 5 means priorities 0-4 cannot call FreeRTOS APIs
 * UART, I2C, timers use priority 5 or lower so they can call FromISR functions */
#define configPRIO_BITS                          4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY  15
#define configKERNEL_INTERRUPT_PRIORITY          (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY     (5 << (8 - configPRIO_BITS))

/* Cortex-M specific: SysTick and PendSV */
#define xPortPendSVHandler   PendSV_Handler
#define vPortSVCHandler      SVC_Handler
#define xPortSysTickHandler  SysTick_Handler

/* Debug and tracing (disabled in production) */
#define configUSE_TRACE_FACILITY                 1
#define configUSE_STATS_FORMATTING_FUNCTIONS     1
#define configGENERATE_RUN_TIME_STATS            0
#define configRECORD_STACK_HIGH_ADDRESS          1

/* Assertions: enabled in Debug, compiled out in Release */
#ifdef DEBUG
  extern void vAssertCalled(const char *file, uint32_t line);
  #define configASSERT(x) if((x) == 0) vAssertCalled(__FILE__, __LINE__)
#else
  #define configASSERT(x)
#endif

#endif /* FREERTOS_CONFIG_H */
