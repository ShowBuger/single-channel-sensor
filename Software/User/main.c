#include "stm32f10x.h"
#include "mlx90393.h"
#include "sensor_manager.h"
#include "HardI2C.h"
#include "SoftI2C.h"
#include "Serial.h"
#include "Delay.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

/* FreeRTOS 头文件 */
#include "FreeRTOS.h"
#include "task.h"
#include "freertos_tasks.h"

void IWDG_Init(uint8_t prer, uint16_t rlr)
{
	IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);	//使能对IWDG_PR和IWDG_RLR寄存器的写操作
	IWDG_SetPrescaler(prer);						//设置预分频器值
	IWDG_SetReload(rlr);							//设置重装载值
	IWDG_ReloadCounter();							//重装载计数器(喂狗)
	IWDG_Enable();									//启动独立看门狗
}

/**
  * 函    数:喂狗函数
  * 参    数:无
  * 返 回 值:无
  * 说    明:在主循环中定期调用此函数,防止系统复位
  */
void IWDG_Feed(void)
{
	IWDG_ReloadCounter();							//重装载计数器(喂狗)
}

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
  *         启用LSI 40kHz用于IWDG
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

	/*使能内部低速振荡器LSI (40kHz),供IWDG使用*/
	RCC_LSICmd(ENABLE);

	/*等待LSI就绪*/
	while(RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET);
}

/**
  * 函    数:Flash读保护检查和设置
  * 参    数:无
  * 返 回 值:无
  * 说    明:检查Flash读保护状态,如果未设置则设置为Level 1读保护
  *         Level 1读保护: 防止通过调试接口(JTAG/SWD)和SRAM启动读取Flash内容
  *         解除读保护需要先擦除整个Flash
  */

int main(void)
{
    /* 系统时钟初始化 */
    SYSTEM_Init();

    /* 硬件初始化 */
    HardI2C_Init();
    Serial_Init();
    Config_Init_All();

    printf("System Reset - FreeRTOS Mode\r\n");

    /* 扫描传感器 */
    Scan_Ports();

    /* 初始化 FreeRTOS 对象和任务 */
    FreeRTOS_Init();

    /* 启动调度器 - 此函数不会返回 */
    vTaskStartScheduler();

    /* 如果调度器启动失败，进入死循环 */
    while(1)
    {
        /* 不应到达这里 */
    }
}
