/**
 * @file  rtos_main.c
 * @brief P4: Multi-task architecture with 5 tasks (simplified design).
 *
 * P3 → P4 changes:
 *   - Split T_MAIN into T_ACQ, T_PROC, T_CMD, T_TX
 *   - Add SampleQueue (T_ACQ → T_PROC) and TxQueue (T_CMD/T_PROC → T_TX)
 *   - Move watchdog feeding to vApplicationIdleHook (replaces T_SUP)
 *
 * Task architecture (simplified, 5 tasks):
 *   T_ACQ  (prio 5) - Acquisition: trigger sensors, read, enqueue samples
 *   T_PROC (prio 4) - Processing: algorithm + formatting, output to TxQueue
 *   T_CMD  (prio 3) - Command handler: parse commands, respond via TxQueue
 *   T_TX   (prio 4) - Transmitter: single writer, pulls from TxQueue
 *   Idle Hook      - Watchdog feeding (replaces T_SUP)
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "plat.h"
#include "board.h"
#include "sensor_manager.h"
#include "command_handler.h"
#include "rtos_types.h"
#include "../drivers/bsp/log_manager.h"
#include "../drivers/bsp/bsp_uart.h"
#include <stdio.h>
#include <string.h>

/* ── External dependencies ────────────────────────────────────────────── */

extern void IWDG_Feed(void);
extern uint8_t SENSOR_Run[1];
extern uint8_t get_sensor_data(void);  /* P3 legacy: reads sensor_data[0] */
extern void print_processed_sensor_data(uint8_t idx);  /* P3 legacy: prints processed_sensor_data[idx] */

/* ── Task handles (for diagnostics) ───────────────────────────────────── */

static TaskHandle_t s_acq_task;
static TaskHandle_t s_proc_task;
static TaskHandle_t s_cmd_task;
static TaskHandle_t s_tx_task;

/* ── Queues ───────────────────────────────────────────────────────────── */

#define SAMPLE_QUEUE_DEPTH  4
#define TX_QUEUE_DEPTH      8

static QueueHandle_t s_sample_queue;  /* T_ACQ → T_PROC */
static QueueHandle_t s_tx_queue;      /* T_CMD/T_PROC → T_TX */

/* ── T_ACQ: Acquisition task ──────────────────────────────────────────── */

/**
 * T_ACQ: Acquisition task (priority 5, highest).
 *
 * Runs at 10 Hz (100 ms period) in P4. Will be upgraded to 100 Hz in later phases.
 *
 * Responsibilities:
 *   - Trigger both actual and reference sensors
 *   - Wait for conversion (blocking wait in P4, will use timer in P5)
 *   - Read sensor data
 *   - Package into SamplePacket_t with timestamp
 *   - Send to T_PROC via SampleQueue
 *
 * MUST NOT do any of:
 *   - Logging (use LOG only for errors)
 *   - UART transmission
 *   - Flash access
 *   - Algorithm computation
 */
static void task_acquisition(void *param)
{
    (void)param;
    SamplePacket_t pkt;
    uint32_t seq = 0;

    LOG_I("ACQ", "T_ACQ started, 10 Hz sampling");

    for (;;) {
        /* P4 workaround: Feed watchdog here since Idle may not run if T_TX blocks */
        IWDG_Feed();

        /* Only sample if sensor is running */
        if (SENSOR_Run[0] != 1) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        /* P4: Use existing get_sensor_data() which reads into sensor_data[0].
         * In P5, this will be refactored to async trigger + wait. */
        uint32_t ts_start = plat_tick_ms();  /* P4: millisecond timestamp; P5 will add microsecond timer */
        get_sensor_data();

        /* Package sample into packet */
        pkt.seq = seq++;
        pkt.timestamp_us = ts_start * 1000;  /* Convert ms to µs for protocol compatibility */

        /* Copy actual sensor reading (SensorData_t fields: x, y, z, t) */
        pkt.actual.x_uT = sensor_data[0].actual.x;
        pkt.actual.y_uT = sensor_data[0].actual.y;
        pkt.actual.z_uT = sensor_data[0].actual.z;
        pkt.actual.temp_C = sensor_data[0].actual.t;  /* 't' not 'temp' */
        pkt.actual.valid = (sensor_data[0].valid != 0);

        /* Copy reference sensor reading */
        pkt.reference.x_uT = sensor_data[0].reference.x;
        pkt.reference.y_uT = sensor_data[0].reference.y;
        pkt.reference.z_uT = sensor_data[0].reference.z;
        pkt.reference.temp_C = sensor_data[0].reference.t;  /* 't' not 'temp' */
        pkt.reference.valid = (sensor_data[0].valid != 0);

        /* Send to processing task (non-blocking, drop if queue full) */
        if (xQueueSend(s_sample_queue, &pkt, 0) != pdPASS) {
            /* Queue full: sample dropped. Log only in debug builds to avoid jitter. */
            #ifdef DEBUG
            LOG_W("ACQ", "Sample %lu dropped (queue full)", (unsigned long)seq - 1);
            #endif
        }

        /* P4: Fixed 10 Hz (100 ms period). Will upgrade to 100 Hz in P5. */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ── T_PROC: Processing task ──────────────────────────────────────────── */

/**
 * T_PROC: Processing task (priority 4).
 *
 * Merges T_ALGO + T_OUT from original design (simplified).
 *
 * Responsibilities:
 *   - Pull samples from SampleQueue (blocking wait)
 *   - Run algorithm pipeline (calibration, filtering, mapping, etc.)
 *   - Format output as protocol string
 *   - Send to T_TX via TxQueue
 *
 * This task is pure computation: no I/O except queue operations.
 */
static void task_processing(void *param)
{
    (void)param;
    SamplePacket_t pkt;

    LOG_I("PROC", "T_PROC started");

    for (;;) {
        /* Wait for sample from T_ACQ (blocks indefinitely) */
        if (xQueueReceive(s_sample_queue, &pkt, portMAX_DELAY) != pdPASS) {
            continue;  /* Should never happen with portMAX_DELAY */
        }

        /* P4: Use existing data_process() pipeline via print_processed_sensor_data().
         * The function reads sensor_data[0] and writes processed_sensor_data[0],
         * then formats output to UART. We'll intercept the formatted string.
         *
         * TODO P5: Refactor data_process() to return formatted string instead of
         * directly calling Serial_SendString(). */

        /* Re-populate sensor_data[0] from packet (data_process reads from here) */
        sensor_data[0].actual.x = pkt.actual.x_uT;
        sensor_data[0].actual.y = pkt.actual.y_uT;
        sensor_data[0].actual.z = pkt.actual.z_uT;
        sensor_data[0].actual.t = pkt.actual.temp_C;  /* SensorData_t field is 't', not 'temp' */

        sensor_data[0].reference.x = pkt.reference.x_uT;
        sensor_data[0].reference.y = pkt.reference.y_uT;
        sensor_data[0].reference.z = pkt.reference.z_uT;
        sensor_data[0].reference.t = pkt.reference.temp_C;  /* SensorData_t field is 't', not 'temp' */

        sensor_data[0].valid = pkt.actual.valid ? 1 : 0;

        /* Run algorithm and formatting.
         * P4 limitation: print_processed_sensor_data() still calls Serial_SendString
         * directly, so we can't intercept the output yet. This will be fixed in P5.
         * For now, just call it directly (maintains P3 behavior). */
        print_processed_sensor_data(0);

        /* P4 TODO: Once print_processed_sensor_data is refactored to return a string,
         * package it into TxMessage_t and send to T_TX:
         *
         * msg.type = TX_MSG_DATA;
         * snprintf(msg.text, TX_MSG_MAX_LEN, "... formatted output ...");
         * xQueueSend(s_tx_queue, &msg, portMAX_DELAY);
         */
    }
}

/* ── T_CMD: Command handler task ──────────────────────────────────────── */

/**
 * T_CMD: Command processing task (priority 3).
 *
 * Responsibilities:
 *   - Check for incoming serial commands via CommandHandler_Process()
 *   - Parse and dispatch commands
 *   - Send responses via TxQueue (not directly to UART)
 *
 * P4 limitation: CommandHandler_Process() still calls Serial_SendString directly.
 * Will be refactored in P5 to use a response callback that enqueues to TxQueue.
 */
static void task_command(void *param)
{
    (void)param;

    LOG_I("CMD", "T_CMD started");

    for (;;) {
        /* P4: Use existing CommandHandler_Process() which polls serial buffer.
         * It still sends responses directly via Serial_SendString.
         * TODO P5: Refactor to use response callback → TxQueue. */
        CommandHandler_Process();

        /* Yield briefly to avoid busy-wait */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ── T_TX: Transmitter task (single writer) ───────────────────────────── */

/**
 * T_TX: Single-writer transmitter task (priority 4).
 *
 * The ONLY task that writes to UART. Ensures atomic line transmission.
 *
 * Responsibilities:
 *   - Pull messages from TxQueue (blocking wait)
 *   - Send complete messages to UART atomically
 *
 * P4 limitation: Not used yet because T_PROC and T_CMD still call Serial_SendString
 * directly. This task is a placeholder for P5 refactoring.
 */
static void task_transmit(void *param)
{
    (void)param;
    TxMessage_t msg;

    LOG_I("TX", "T_TX started (single writer)");

    for (;;) {
        /* P4: Use timeout instead of portMAX_DELAY to allow other tasks to run.
         * T_TX has priority 4, higher than Idle (0), so if it blocks forever,
         * Idle cannot run and watchdog won't be fed. */
        if (xQueueReceive(s_tx_queue, &msg, pdMS_TO_TICKS(100)) == pdPASS) {
            /* Send message atomically (entire string in one call) */
            Serial_SendString(msg.text);
        }
        /* Timeout expired: loop continues, allows scheduler to run other tasks */
    }
}

/* ── RTOS initialization ──────────────────────────────────────────────── */

/**
 * rtos_main: Initialize FreeRTOS tasks and start scheduler.
 * Called from main() after platform init. Does not return.
 */
void rtos_main(void)
{
    BaseType_t ret;

    LOG_I("RTOS", "FreeRTOS V%s starting (P4: 5-task architecture)", tskKERNEL_VERSION_NUMBER);
    LOG_I("RTOS", "Heap: %u bytes", configTOTAL_HEAP_SIZE);

    /* Create queues */
    s_sample_queue = xQueueCreate(SAMPLE_QUEUE_DEPTH, sizeof(SamplePacket_t));
    s_tx_queue = xQueueCreate(TX_QUEUE_DEPTH, sizeof(TxMessage_t));

    if (s_sample_queue == NULL || s_tx_queue == NULL) {
        LOG_E("RTOS", "Queue creation failed");
        plat_sys_reset();
    }

    /* Create T_ACQ (highest priority: time-critical) */
    ret = xTaskCreate(task_acquisition, "T_ACQ", 384, NULL, tskIDLE_PRIORITY + 5, &s_acq_task);
    if (ret != pdPASS) {
        LOG_E("RTOS", "T_ACQ creation failed");
        plat_sys_reset();
    }

    /* Create T_PROC (algorithm + formatting) */
    ret = xTaskCreate(task_processing, "T_PROC", 512, NULL, tskIDLE_PRIORITY + 4, &s_proc_task);
    if (ret != pdPASS) {
        LOG_E("RTOS", "T_PROC creation failed");
        plat_sys_reset();
    }

    /* Create T_CMD (command handler) */
    ret = xTaskCreate(task_command, "T_CMD", 384, NULL, tskIDLE_PRIORITY + 3, &s_cmd_task);
    if (ret != pdPASS) {
        LOG_E("RTOS", "T_CMD creation failed");
        plat_sys_reset();
    }

    /* Create T_TX (single writer) */
    ret = xTaskCreate(task_transmit, "T_TX", 256, NULL, tskIDLE_PRIORITY + 4, &s_tx_task);
    if (ret != pdPASS) {
        LOG_E("RTOS", "T_TX creation failed");
        plat_sys_reset();
    }

    LOG_I("RTOS", "Tasks created: T_ACQ, T_PROC, T_CMD, T_TX");
    LOG_I("RTOS", "Starting scheduler...");

    /* Start scheduler (does not return) */
    vTaskStartScheduler();

    /* Should never reach here */
    LOG_E("RTOS", "Scheduler start failed");
    plat_sys_reset();
}

/* ── Idle hook (watchdog feeding) ─────────────────────────────────────── */

/**
 * vApplicationIdleHook: Called when no task is ready to run.
 *
 * P4: Feeds watchdog here (replaces T_SUP from original 7-task design).
 * This is safe because idle task runs at lowest priority and is always
 * schedulable unless a higher-priority task is stuck.
 *
 * P5+: May add sleep mode entry here for power optimization.
 */
void vApplicationIdleHook(void)
{
    /* Feed watchdog to prevent system reset */
    IWDG_Feed();
}

/* ── Diagnostics ──────────────────────────────────────────────────────── */

/**
 * Query task stack high-water marks for tuning.
 * Call this from ML STATUS command after system has run for a while.
 */
void rtos_get_task_stack_watermarks(uint32_t *acq, uint32_t *proc, uint32_t *cmd, uint32_t *tx)
{
    if (acq)  *acq  = s_acq_task  ? uxTaskGetStackHighWaterMark(s_acq_task)  : 0;
    if (proc) *proc = s_proc_task ? uxTaskGetStackHighWaterMark(s_proc_task) : 0;
    if (cmd)  *cmd  = s_cmd_task  ? uxTaskGetStackHighWaterMark(s_cmd_task)  : 0;
    if (tx)   *tx   = s_tx_task   ? uxTaskGetStackHighWaterMark(s_tx_task)   : 0;
}

/**
 * Query heap watermark (minimum free heap since boot).
 */
size_t rtos_get_heap_free_minimum(void)
{
    return xPortGetMinimumEverFreeHeapSize();
}
