#include "stm32f10x.h"
#include "mlx90393.h"
#include "sensor_manager.h"
#include "SoftI2C.h"
#include "Serial.h"
#include "Delay.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOSConfig.h"
#include "app.h"
/**
  * 函    数:系统时钟初始化
  * 参    数:无
  * 返 回 值:无
  * 说    明:根据STM32F103规范配置
  *         HSE: 8MHz外部晶振
  *         PLL: HSE/1 × 9 = 72MHz
  *         SYSCLK: 72MHz
  *         HCLK: 72MHz (AHB)
  *         PCLK1: 36MHz (APB1)
  *         PCLK2: 72MHz (APB2)
  */
void SYSTEM_Init()
{
	ErrorStatus HSEStartUpStatus;

	/*复位RCC时钟配置*/
	RCC_DeInit();

	RCC_HSEConfig(RCC_HSE_ON);

	HSEStartUpStatus = RCC_WaitForHSEStartUp();

	if(HSEStartUpStatus == SUCCESS)
	{
		/*使能FLASH预取指缓存*/
		FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);

		/*设置FLASH延迟周期: 72MHz需要2个等待周期*/
		FLASH_SetLatency(FLASH_Latency_2);

		/*配置AHB时钟 = SYSCLK = 72MHz*/
		RCC_HCLKConfig(RCC_SYSCLK_Div1);

		/*配置APB2时钟 = HCLK = 72MHz*/
		RCC_PCLK2Config(RCC_HCLK_Div1);

		/*配置APB1时钟 = HCLK/2 = 36MHz (符合STM32F103规范,最大36MHz)*/
		RCC_PCLK1Config(RCC_HCLK_Div2);

		/*配置PLL: HSE/1作为输入, 倍频系数×9*/
		/*PLL输出 = 8MHz ÷ 1 × 9 = 72MHz*/
		RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);

		/*使能PLL*/
		RCC_PLLCmd(ENABLE);

		/*等待PLL锁定*/
		while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);

		/*切换系统时钟源到PLL*/
		RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

		/*等待系统时钟切换完成 (0x08 = PLL作为系统时钟)*/
		while(RCC_GetSYSCLKSource() != 0x08);
	}
	else
	{
		/*HSE启动失败处理*/
		while(1);  // 建议添加错误指示
	}

	/*使能内部低速振荡器LSI (40kHz)*/
	RCC_LSICmd(ENABLE);

	/*等待LSI就绪*/
	while(RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET)
    {};
  
    /*初始化延时（TIM2）*/
    Delay_Init();
    /*初始化软件IIC*/
    SoftI2C_Init();
    /*初始化串口*/
    Serial_Init();
    /*初始化配置参数 - 从Flash读取*/
    Config_Init_All();
    printf("System_Reset\r\n");
}


int main(void)
{
    SYSTEM_Init();  //系统初始化
    
    //创建启动任务
    xTaskCreate(vBooTTask, "BOOT_task", 512, NULL, BOOT_Task_PRIORITIES, NULL);
    
    vTaskStartScheduler();  //开启任务调度

    while(1)
    {
        printf("FREERTOS_START_FAILED");
//        serial_command_process();
//        if(SENSOR_Run[0] == 1)
//        {
//            get_sensor_data();
//            print_processed_sensor_data(0);
//        }
    }
}
