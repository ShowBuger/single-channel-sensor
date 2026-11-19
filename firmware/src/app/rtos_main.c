/** Acquisition owns sensor I/O; processing owns algorithm state.
 * A mutex serializes configuration/calibration with both owners. Queue packets
 * contain immutable readings, never aliases of sensor_data. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "plat.h"
#include "board.h"
#include "sensor_manager.h"
#include "command_handler.h"
#include "rtos_types.h"
#include "log_manager.h"
#include "bsp_uart.h"
#include <string.h>

extern void IWDG_Feed(void);
static TaskHandle_t s_acq_task,s_proc_task,s_cmd_task,s_tx_task;
static QueueHandle_t s_sample_queue,s_tx_queue;
static QueueHandle_t s_trace_queue;
static SemaphoreHandle_t s_sensor_lock;
static uint32_t s_epoch=1;
static uint32_t s_sample_drops,s_output_drops;
static uint8_t s_calibration_pending,s_calibration_result;

/* Called by T_CMD while holding s_sensor_lock. Transfer calibration to T_ACQ
 * and release the bus/config lock while waiting; T_TX remains schedulable. */
static uint8_t calibration_executor(void)
{
    if(xTaskGetCurrentTaskHandle()==s_acq_task) return Sensor_CalibrateOwned();
    s_calibration_pending=1;
    xSemaphoreGive(s_sensor_lock);
    ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
    xSemaphoreTake(s_sensor_lock,portMAX_DELAY);
    return s_calibration_result;
}
static void task_acquisition(void *param)
{
    (void)param;
    TickType_t wake=xTaskGetTickCount();
    for(;;) {
        IWDG_Feed();
        xSemaphoreTake(s_sensor_lock,portMAX_DELAY);
        if(s_calibration_pending) {
            s_sample_drops+=uxQueueMessagesWaiting(s_sample_queue);
            xQueueReset(s_sample_queue); s_epoch++;
            s_calibration_result=Sensor_CalibrateOwned();
            s_calibration_pending=0;
            xTaskNotifyGive(s_cmd_task);
            wake=xTaskGetTickCount();
        } else if(SENSOR_Run[0]) {
            SamplePacket_t packet;
            get_sensor_data();
            packet.value=sensor_data[0]; packet.timing=sensor_timing; packet.epoch=s_epoch;
            if(xQueueSend(s_sample_queue,&packet,0)!=pdPASS) s_sample_drops++;
        }
        TickType_t period=pdMS_TO_TICKS(sensor_timing.rate_hz==20 ? 50 : 10);
        xSemaphoreGive(s_sensor_lock);
        vTaskDelayUntil(&wake,period);
    }
}
static void task_processing(void *param)
{
    (void)param;
    SamplePacket_t packet;
    TxMessage_t msg;
    for(;;) {
        if(xQueueReceive(s_sample_queue,&packet,portMAX_DELAY)!=pdPASS) continue;
        xSemaphoreTake(s_sensor_lock,portMAX_DELAY);
        if(packet.epoch==s_epoch && SENSOR_Run[0]) {
            Sensor_ProcessSample(&packet.value,&packet.timing);
            msg.type=TX_MSG_DATA;
            int len=format_processed_sensor_data(0,msg.text,sizeof(msg.text));
            /* No extra rate gate: each acquired sample is processed once. */
            if(len>0 && xQueueSend(s_tx_queue,&msg,0)!=pdPASS) s_output_drops++;
            AlgoTrace_t trace;
            if(Sensor_TakeTrace(&trace)) xQueueOverwrite(s_trace_queue,&trace);
        } else s_sample_drops++;
        xSemaphoreGive(s_sensor_lock);
    }
}
static void command_response(const char *response)
{
    TxMessage_t msg={.type=TX_MSG_RESPONSE};
    size_t n=strlen(response);
    if(n>=sizeof(msg.text)) n=sizeof(msg.text)-1;
    memcpy(msg.text,response,n); msg.text[n]=0;
    xQueueSend(s_tx_queue,&msg,portMAX_DELAY);
}
static void task_command(void *param)
{
    (void)param;
    CommandHandler_SetResponseCallback(command_response);
    for(;;) {
        xSemaphoreTake(s_sensor_lock,portMAX_DELAY);
        if(CommandHandler_Process()==2) {
            s_epoch++; s_sample_drops+=uxQueueMessagesWaiting(s_sample_queue); xQueueReset(s_sample_queue);
            xQueueReset(s_trace_queue);
        }
        xSemaphoreGive(s_sensor_lock);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
static void task_transmit(void *param)
{
    (void)param;
    TxMessage_t msg;
    AlgoTrace_t trace;
    for(;;) {
        if(xQueueReceive(s_tx_queue,&msg,pdMS_TO_TICKS(10))==pdPASS) Serial_SendString(msg.text);
        if(xQueueReceive(s_trace_queue,&trace,0)==pdPASS) {
            for(unsigned i=0;i<ALGO_STAGE_COUNT;i++) {
                if(Sensor_FormatTrace(&trace,i,msg.text,sizeof(msg.text))>0) Serial_SendString(msg.text);
            }
        }
    }
}
void rtos_main(void)
{
    s_sample_queue=xQueueCreate(4,sizeof(SamplePacket_t));
    s_tx_queue=xQueueCreate(8,sizeof(TxMessage_t));
    s_trace_queue=xQueueCreate(1,sizeof(AlgoTrace_t));
    s_sensor_lock=xSemaphoreCreateMutex();
    if(!s_sample_queue || !s_tx_queue || !s_trace_queue || !s_sensor_lock) plat_sys_reset();
    if(xTaskCreate(task_acquisition,"T_ACQ",768,NULL,5,&s_acq_task)!=pdPASS ||
       xTaskCreate(task_processing,"T_PROC",768,NULL,4,&s_proc_task)!=pdPASS ||
       xTaskCreate(task_command,"T_CMD",768,NULL,3,&s_cmd_task)!=pdPASS ||
       xTaskCreate(task_transmit,"T_TX",768,NULL,4,&s_tx_task)!=pdPASS) plat_sys_reset();
    Sensor_SetCalibrationExecutor(calibration_executor);
    vTaskStartScheduler();
    plat_sys_reset();
}
void vApplicationIdleHook(void) { IWDG_Feed(); }
void rtos_get_task_stack_watermarks(uint32_t *a,uint32_t *p,uint32_t *c,uint32_t *t)
{
    if(a) *a=uxTaskGetStackHighWaterMark(s_acq_task);
    if(p) *p=uxTaskGetStackHighWaterMark(s_proc_task);
    if(c) *c=uxTaskGetStackHighWaterMark(s_cmd_task);
    if(t) *t=uxTaskGetStackHighWaterMark(s_tx_task);
}
size_t rtos_get_heap_free_minimum(void) { return xPortGetMinimumEverFreeHeapSize(); }
void rtos_get_drop_counts(uint32_t *samples,uint32_t *output)
{
    *samples=s_sample_drops; *output=s_output_drops;
}
