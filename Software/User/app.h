#ifndef __APP_H_
#define __APP_H_

#include "FreeRTOS.h"
#include "queue.h"
#include "sensor_manager.h"

#define BOOT_Task_PRIORITIES        4   // 启动任务：最高优先级，确保系统初始化先完成
#define DATA_ACQUIRE_PRIORITIES     3   // 数据采集：次高优先级，保证传感器采样实时性
#define DATA_PROCESS_PRIORITIES     3   // 数据处理：中优先级，采集完成后处理
#define UART_COMMAND_PRIORITIES     4   // 串口命令：低优先级，人机交互无严格实时要求

#define SENSOR_QUEUE_LENGTH         4   // 数据队列长度
#define COMMAND_QUEUE_LENGTH        4   // 命令队列长度


extern QueueHandle_t xSensorDataQueue;  // 传感器数据队列句柄（采集任务写，处理任务读）
extern QueueHandle_t xUartCommandQueue; // 串口命令队列句柄（串口中断写，命令任务读）
// 启动任务
void vBooTTask(void *pvParameters);

// 串口命令任务
void vUartCommandTask(void *pvParameters);

// 数据采集任务
void vDataAcquireTask(void *pvParameters);

// 数据处理任务
void vDataProcessTask(void *pvParameters);

#endif
