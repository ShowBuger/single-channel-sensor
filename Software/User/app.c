#include "app.h"
#include "stm32f10x.h"
#include "mlx90393.h"
#include "sensor_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "FreeRTOSConfig.h"
#include "Serial.h"
#include "queue.h"
QueueHandle_t xSensorDataQueue;  //传感器数据队列
QueueHandle_t xUartCommandQueue; //串口命令队列

void vBooTTask(void *pvParameters)
{
    Scan_Ports();   //扫描传感器
    sensor_start();

    // 创建队列
    xSensorDataQueue  = xQueueCreate(SENSOR_QUEUE_LENGTH, sizeof(SensorPair_Data));
    xUartCommandQueue = xQueueCreate(COMMAND_QUEUE_LENGTH, 64);
    
    
    xTaskCreate(vDataAcquireTask, "DataAcquire_task", 256, NULL, DATA_ACQUIRE_PRIORITIES, NULL);
    xTaskCreate(vDataProcessTask, "DataProcess_task", 512, NULL, DATA_PROCESS_PRIORITIES, NULL);
    xTaskCreate(vUartCommandTask, "UartCommand_task", 128, NULL, UART_COMMAND_PRIORITIES, NULL);

    vTaskDelete(NULL); //任务自杀
}


// 数据采集任务
void vDataAcquireTask(void *pvParameters)
{
    while(1)
    {
        
       //获取互斥量
        
        
        // 采集传感器数据
        if(get_sensor_data() == 0)
        {
            // 写队列，不等待（队列满则丢弃本帧）
            xQueueSend(xSensorDataQueue, &sensor_data[0], 0);
        }

        //释放互斥量
    }
}

// 数据处理任务
void vDataProcessTask(void *pvParameters)
{
    SensorPair_Data received_data;
    while(1)
    {
        // 阻塞等待队列数据，超时 100ms
        if(xQueueReceive(xSensorDataQueue, &received_data, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            // 将接收到的数据写回全局变量，供 data_process() 使用
            sensor_data[0] = received_data;
            data_process();
            print_processed_sensor_data(0);
           // printf("2\n");
        }
    }
}

// 串口命令任务
void vUartCommandTask(void *pvParameters)
{
    char cmd[CMD_BUFFER_SIZE];
    while(1)
    {
        // 阻塞等待命令队列，收到后直接处理 cmd 字符串
        if(xQueueReceive(xUartCommandQueue, cmd, portMAX_DELAY) != pdTRUE)
            continue;

        // ML START
        if(strcmp(cmd, "ML START") == 0)
        {
            if(sensor_start() == 0)
            {
                printf("Sensor started successfully\r\n");
                printf("Running sensor: %s\r\n", SENSOR_Run[0] ? "0" : "None");
            }
            else
            {
                printf("Failed to start sensors\r\n");
            }
        }
        // ML STOP
        else if(strcmp(cmd, "ML STOP") == 0)
        {
            if(sensor_stop() == 0)
                printf("Sensor stopped\r\n");
            else
                printf("Failed to stop sensor\r\n");
        }
        // ML CALIB
        else if(strcmp(cmd, "ML CALIB") == 0)
        {
            if(perform_sensor_calibration() == 0)
                printf("Calibration completed\r\n");
            else
                printf("Calibration failed\r\n");
        }
        // ML FLAG
        else if(strcmp(cmd, "ML FLAG") == 0)
        {
            printf("FLAG_RES: 0x%02X\r\n", FLAG_RES);
        }
        // ML CHFLAG <value>
        else if(strncmp(cmd, "ML CHFLAG ", 10) == 0)
        {
            uint32_t new_val;
            if(sscanf(cmd + 10, "0x%X", &new_val) == 1 ||
               sscanf(cmd + 10, "0X%X", &new_val) == 1 ||
               sscanf(cmd + 10, "%u",   &new_val) == 1)
            {
                if(new_val <= 0xFF)
                {
                    uint8_t old = FLAG_RES;
                    FLAG_RES = (uint8_t)new_val;
                    printf("FLAG_RES updated: 0x%02X -> 0x%02X\r\n", old, FLAG_RES);
                    Config_Save_All();
                }
                else
                    printf("Error: Value out of range (0-255)\r\n");
            }
            else
            {
                printf("Error: Invalid format. Usage: ML CHFLAG <value>\r\n");
                printf("Example: ML CHFLAG 0x1B or ML CHFLAG 27\r\n");
            }
        }
        // ML MAPPING
        else if(strcmp(cmd, "ML MAPPING") == 0)
        {
            printf("Mapping values:\r\n");
            printf("  X: %.3f\r\n", X_MAPPING_VALUE);
            printf("  Y: %.3f\r\n", Y_MAPPING_VALUE);
            printf("  Z: %.3f\r\n", Z_MAPPING_VALUE);
        }
        // ML SETMAP <X|Y|Z> <value>
        else if(strncmp(cmd, "ML SETMAP ", 10) == 0)
        {
            char axis;
            float new_val;
            if(sscanf(cmd + 10, "%c %f", &axis, &new_val) == 2)
            {
                if(axis == 'X' || axis == 'x')
                {
                    X_MAPPING_VALUE = new_val;
                    printf("X_MAPPING_VALUE updated: %.3f\r\n", X_MAPPING_VALUE);
                    Mapping_Value_Save();
                }
                else if(axis == 'Y' || axis == 'y')
                {
                    Y_MAPPING_VALUE = new_val;
                    printf("Y_MAPPING_VALUE updated: %.3f\r\n", Y_MAPPING_VALUE);
                    Mapping_Value_Save();
                }
                else if(axis == 'Z' || axis == 'z')
                {
                    Z_MAPPING_VALUE = new_val;
                    printf("Z_MAPPING_VALUE updated: %.3f\r\n", Z_MAPPING_VALUE);
                    Mapping_Value_Save();
                }
                else
                    printf("Error: Invalid axis '%c'. Use X, Y, or Z\r\n", axis);
            }
            else
            {
                printf("Error: Invalid format. Usage: ML SETMAP <X|Y|Z> <value>\r\n");
                printf("Example: ML SETMAP X 0.5\r\n");
            }
        }
        else
        {
            printf("Unknown command: %s\r\n", cmd);
        }
    }
}

