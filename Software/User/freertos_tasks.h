#ifndef __FREERTOS_TASKS_H
#define __FREERTOS_TASKS_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "sensor_manager.h"

/*===========================================================================*/
/*                              任务优先级                                    */
/*===========================================================================*/

#define SENSOR_TASK_PRIORITY     (configMAX_PRIORITIES - 1)  /* 4 - 最高 */
#define OUTPUT_TASK_PRIORITY     (configMAX_PRIORITIES - 3)  /* 2 - 中等 */
#define COMMAND_TASK_PRIORITY    (configMAX_PRIORITIES - 4)  /* 1 - 较低 */

/*===========================================================================*/
/*                              任务栈大小 (单位: words)                       */
/*===========================================================================*/

#define SENSOR_TASK_STACK_SIZE   256
#define OUTPUT_TASK_STACK_SIZE   192
#define COMMAND_TASK_STACK_SIZE  192

/*===========================================================================*/
/*                              队列配置                                      */
/*===========================================================================*/

#define DATA_QUEUE_LENGTH        4
#define CMD_QUEUE_LENGTH         2
#define CMD_MAX_LENGTH           64

/*===========================================================================*/
/*                              事件组位定义                                  */
/*===========================================================================*/

#define SENSOR_RUN_BIT           (1 << 0)   /* 传感器运行标志 */
#define SENSOR_STOP_REQUEST_BIT  (1 << 1)   /* 传感器停止请求 */
#define SENSOR_CALIBRATING_BIT   (1 << 2)   /* 传感器标定中 */

/*===========================================================================*/
/*                              传感器任务周期                                */
/*===========================================================================*/

#define SENSOR_TASK_PERIOD_MS    8

/*===========================================================================*/
/*                              类型定义                                      */
/*===========================================================================*/

/* 命令消息结构 */
typedef struct {
    char command[CMD_MAX_LENGTH];
} CommandMsg_t;

/* 传感器数据消息结构 */
typedef struct {
    SensorPair_Data data;
    uint8_t sensor_index;
    TickType_t timestamp;
} SensorDataMsg_t;

/*===========================================================================*/
/*                           全局句柄声明                                     */
/*===========================================================================*/

extern QueueHandle_t xDataQueue;
extern QueueHandle_t xCommandQueue;
extern SemaphoreHandle_t xSensorMutex;
extern EventGroupHandle_t xSensorEventGroup;

extern TaskHandle_t xSensorTaskHandle;
extern TaskHandle_t xOutputTaskHandle;
extern TaskHandle_t xCommandTaskHandle;

/*===========================================================================*/
/*                              函数声明                                      */
/*===========================================================================*/

/* 初始化函数 */
void FreeRTOS_Init(void);

/* 任务函数 */
void vSensorTask(void *pvParameters);
void vOutputTask(void *pvParameters);
void vCommandTask(void *pvParameters);

/* 传感器控制接口 */
void Sensor_RequestStart(void);
void Sensor_RequestStop(void);
uint8_t Sensor_IsRunning(void);

#endif /* __FREERTOS_TASKS_H */
