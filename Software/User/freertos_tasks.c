#include "freertos_tasks.h"
#include "sensor_manager.h"
#include "Serial.h"
#include "config_storage.h"
#include <string.h>
#include <stdio.h>

/*===========================================================================*/
/*                              全局变量                                      */
/*===========================================================================*/

/* FreeRTOS 对象句柄 */
QueueHandle_t xDataQueue = NULL;
QueueHandle_t xCommandQueue = NULL;
SemaphoreHandle_t xSensorMutex = NULL;
EventGroupHandle_t xSensorEventGroup = NULL;

TaskHandle_t xSensorTaskHandle = NULL;
TaskHandle_t xOutputTaskHandle = NULL;
TaskHandle_t xCommandTaskHandle = NULL;

/*===========================================================================*/
/*                           静态函数声明                                     */
/*===========================================================================*/

static void process_command(const char* cmd);

/*===========================================================================*/
/*                           FreeRTOS 初始化                                  */
/*===========================================================================*/

void FreeRTOS_Init(void)
{
    /* 创建数据队列 */
    xDataQueue = xQueueCreate(DATA_QUEUE_LENGTH, sizeof(SensorDataMsg_t));

    /* 创建命令队列 */
    xCommandQueue = xQueueCreate(CMD_QUEUE_LENGTH, sizeof(CommandMsg_t));

    /* 创建互斥量 */
    xSensorMutex = xSemaphoreCreateMutex();

    /* 创建事件组 */
    xSensorEventGroup = xEventGroupCreate();

    /* 检查创建是否成功 */
    configASSERT(xDataQueue != NULL);
    configASSERT(xCommandQueue != NULL);
    configASSERT(xSensorMutex != NULL);
    configASSERT(xSensorEventGroup != NULL);

    /* 创建传感器采集任务 */
    xTaskCreate(vSensorTask,
                "SensorTask",
                SENSOR_TASK_STACK_SIZE,
                NULL,
                SENSOR_TASK_PRIORITY,
                &xSensorTaskHandle);

    /* 创建数据输出任务 */
    xTaskCreate(vOutputTask,
                "OutputTask",
                OUTPUT_TASK_STACK_SIZE,
                NULL,
                OUTPUT_TASK_PRIORITY,
                &xOutputTaskHandle);

    /* 创建命令处理任务 */
    xTaskCreate(vCommandTask,
                "CommandTask",
                COMMAND_TASK_STACK_SIZE,
                NULL,
                COMMAND_TASK_PRIORITY,
                &xCommandTaskHandle);
}

/*===========================================================================*/
/*                            传感器采集任务                                   */
/*===========================================================================*/

void vSensorTask(void *pvParameters)
{
    TickType_t xLastWakeTime;
    const TickType_t xPeriod = pdMS_TO_TICKS(SENSOR_TASK_PERIOD_MS);
    SensorDataMsg_t dataMsg;
    EventBits_t uxBits;

    (void)pvParameters;

    /* 初始化时间基准 */
    xLastWakeTime = xTaskGetTickCount();

    for(;;)
    {
        /* 等待传感器运行事件 */
        uxBits = xEventGroupWaitBits(
            xSensorEventGroup,
            SENSOR_RUN_BIT,
            pdFALSE,              /* 不清除位 */
            pdFALSE,              /* 任意位满足 */
            portMAX_DELAY         /* 无限等待 */
        );

        if(uxBits & SENSOR_RUN_BIT)
        {
            /* 检查是否收到停止请求 */
            if(xEventGroupGetBits(xSensorEventGroup) & SENSOR_STOP_REQUEST_BIT)
            {
                /* 清除运行位和停止请求位 */
                xEventGroupClearBits(xSensorEventGroup,
                    SENSOR_RUN_BIT | SENSOR_STOP_REQUEST_BIT);
                continue;
            }

            /* 获取互斥量保护传感器访问 */
            if(xSemaphoreTake(xSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE)
            {
                /* 采集传感器数据 */
                if(get_sensor_data() == 0)
                {
                    /* 填充消息 */
                    dataMsg.data = processed_sensor_data[0];
                    dataMsg.sensor_index = 0;
                    dataMsg.timestamp = xTaskGetTickCount();

                    /* 发送到输出队列 (非阻塞) */
                    xQueueSend(xDataQueue, &dataMsg, 0);
                }

                xSemaphoreGive(xSensorMutex);
            }

            /* 精确周期延时 */
            vTaskDelayUntil(&xLastWakeTime, xPeriod);
        }
    }
}

/*===========================================================================*/
/*                             数据输出任务                                    */
/*===========================================================================*/

void vOutputTask(void *pvParameters)
{
    SensorDataMsg_t dataMsg;

    (void)pvParameters;

    for(;;)
    {
        /* 等待数据队列 */
        if(xQueueReceive(xDataQueue, &dataMsg, portMAX_DELAY) == pdTRUE)
        {
            /* 检查数据有效性 */
            if(dataMsg.data.valid)
            {
                /* 简洁格式输出 */
                printf("sensor%d:%.2f, %.2f, %.2f\r\n",
                       dataMsg.sensor_index,
                       dataMsg.data.actual.x,
                       dataMsg.data.actual.y,
                       dataMsg.data.actual.z);
            }
        }
    }
}

/*===========================================================================*/
/*                            命令处理任务                                     */
/*===========================================================================*/

void vCommandTask(void *pvParameters)
{
    CommandMsg_t cmdMsg;

    (void)pvParameters;

    for(;;)
    {
        /* 等待命令队列 */
        if(xQueueReceive(xCommandQueue, &cmdMsg, portMAX_DELAY) == pdTRUE)
        {
            /* 获取互斥量保护传感器状态 */
            if(xSemaphoreTake(xSensorMutex, pdMS_TO_TICKS(100)) == pdTRUE)
            {
                /* 命令解析与处理 */
                process_command(cmdMsg.command);

                xSemaphoreGive(xSensorMutex);
            }
        }
    }
}

/*===========================================================================*/
/*                           命令处理实现                                      */
/*===========================================================================*/

static void process_command(const char* cmd)
{
    if(strcmp(cmd, "ML START") == 0)
    {
        if(sensor_start() == 0)
        {
            printf("Sensor started successfully\r\n");
            if(SENSOR_Run[0] == 1)
            {
                printf("Running sensor: 0\r\n");
            }
            xEventGroupSetBits(xSensorEventGroup, SENSOR_RUN_BIT);
        }
        else
        {
            printf("Failed to start sensors\r\n");
        }
    }
    else if(strcmp(cmd, "ML STOP") == 0)
    {
        /* 设置停止请求位 */
        xEventGroupSetBits(xSensorEventGroup, SENSOR_STOP_REQUEST_BIT);
        xEventGroupClearBits(xSensorEventGroup, SENSOR_RUN_BIT);

        if(sensor_stop() == 0)
        {
            printf("Sensor stopped\r\n");
        }
        else
        {
            printf("Failed to stop sensor\r\n");
        }
    }
    else if(strcmp(cmd, "ML MERSURE") == 0)
    {
        /* 单次测量 */
        if(sensor_start() == 0)
        {
            printf("Sensor started for single measurement\r\n");

            /* 手动触发一次采集 */
            if(get_sensor_data() == 0)
            {
                print_processed_sensor_data(0);
            }

            sensor_stop();
            printf("Single measurement completed\r\n");
        }
        else
        {
            printf("Failed to start sensor for measurement\r\n");
        }
    }
    else if(strcmp(cmd, "ML CALIB") == 0)
    {
        /* 标定时暂停采集任务 */
        xEventGroupSetBits(xSensorEventGroup, SENSOR_CALIBRATING_BIT);
        xEventGroupClearBits(xSensorEventGroup, SENSOR_RUN_BIT);

        if(perform_sensor_calibration() == 0)
        {
            printf("Calibration completed\r\n");
        }
        else
        {
            printf("Calibration failed\r\n");
        }

        xEventGroupClearBits(xSensorEventGroup, SENSOR_CALIBRATING_BIT);
    }
    else if(strcmp(cmd, "ML FLAG") == 0)
    {
        printf("FLAG_RES: 0x%02X\r\n", FLAG_RES);
    }
    else if(strncmp(cmd, "ML CHFLAG ", 10) == 0)
    {
        uint32_t new_flag_value;
        if(sscanf(cmd + 10, "0x%X", &new_flag_value) == 1 ||
           sscanf(cmd + 10, "0X%X", &new_flag_value) == 1 ||
           sscanf(cmd + 10, "%u", &new_flag_value) == 1)
        {
            if(new_flag_value <= 0xFF)
            {
                uint8_t old_value = FLAG_RES;
                FLAG_RES = (uint8_t)new_flag_value;
                printf("FLAG_RES updated: 0x%02X -> 0x%02X\r\n", old_value, FLAG_RES);
                Config_Save_All();
            }
            else
            {
                printf("Error: Value out of range (0-255)\r\n");
            }
        }
        else
        {
            printf("Error: Invalid format. Usage: ML CHFLAG <value>\r\n");
        }
    }
    else if(strcmp(cmd, "ML MAPPING") == 0)
    {
        printf("Mapping values:\r\n");
        printf("  X: %.3f\r\n", X_MAPPING_VALUE);
        printf("  Y: %.3f\r\n", Y_MAPPING_VALUE);
        printf("  Z: %.3f\r\n", Z_MAPPING_VALUE);
    }
    else if(strncmp(cmd, "ML SETMAP ", 10) == 0)
    {
        char axis;
        float new_value;
        if(sscanf(cmd + 10, "%c %f", &axis, &new_value) == 2)
        {
            if(axis == 'X' || axis == 'x')
            {
                X_MAPPING_VALUE = new_value;
                printf("X_MAPPING_VALUE updated: %.3f\r\n", X_MAPPING_VALUE);
                Mapping_Value_Save();
            }
            else if(axis == 'Y' || axis == 'y')
            {
                Y_MAPPING_VALUE = new_value;
                printf("Y_MAPPING_VALUE updated: %.3f\r\n", Y_MAPPING_VALUE);
                Mapping_Value_Save();
            }
            else if(axis == 'Z' || axis == 'z')
            {
                Z_MAPPING_VALUE = new_value;
                printf("Z_MAPPING_VALUE updated: %.3f\r\n", Z_MAPPING_VALUE);
                Mapping_Value_Save();
            }
            else
            {
                printf("Error: Invalid axis '%c'. Use X, Y, or Z\r\n", axis);
            }
        }
        else
        {
            printf("Error: Invalid format. Usage: ML SETMAP <X|Y|Z> <value>\r\n");
        }
    }
    else
    {
        printf("Unknown command: %s\r\n", cmd);
    }
}

/*===========================================================================*/
/*                              工具函数                                      */
/*===========================================================================*/

void Sensor_RequestStart(void)
{
    xEventGroupSetBits(xSensorEventGroup, SENSOR_RUN_BIT);
}

void Sensor_RequestStop(void)
{
    xEventGroupSetBits(xSensorEventGroup, SENSOR_STOP_REQUEST_BIT);
}

uint8_t Sensor_IsRunning(void)
{
    return (xEventGroupGetBits(xSensorEventGroup) & SENSOR_RUN_BIT) ? 1 : 0;
}

/*===========================================================================*/
/*                           FreeRTOS Hook 函数                               */
/*===========================================================================*/

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    printf("Stack overflow in task: %s\r\n", pcTaskName);
    while(1);
}

void vApplicationMallocFailedHook(void)
{
    printf("Malloc failed!\r\n");
    while(1);
}
