#include "stm32f10x.h"

/**
  * @brief  延时模块初始化（使用TIM2，不占用SysTick，兼容FreeRTOS）
  *         TIM2 时钟源：APB1 timer clock = 72MHz
  *         预分频：72-1 → 计数频率 1MHz，1计数 = 1us
  * @retval 无
  */
void Delay_Init(void)
{
	TIM_TimeBaseInitTypeDef TIM_InitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

	TIM_InitStructure.TIM_Prescaler     = 72 - 1;        /* 72MHz / 72 = 1MHz, 1tick = 1us */
	TIM_InitStructure.TIM_Period        = 0xFFFF;         /* 初始装载值，运行时动态修改 */
	TIM_InitStructure.TIM_CounterMode   = TIM_CounterMode_Up;
	TIM_InitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_InitStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM2, &TIM_InitStructure);

	TIM_Cmd(TIM2, DISABLE);
}

/**
  * @brief  微秒级延时（基于TIM2，兼容FreeRTOS）
  * @param  xus 延时时长，范围：1~65535 us
  * @retval 无
  */
void Delay_us(uint32_t xus)
{
	TIM_SetCounter(TIM2, 0);
	TIM_SetAutoreload(TIM2, xus - 1);
	TIM_ClearFlag(TIM2, TIM_FLAG_Update);
	TIM_Cmd(TIM2, ENABLE);
	while(!TIM_GetFlagStatus(TIM2, TIM_FLAG_Update));
	TIM_Cmd(TIM2, DISABLE);
}

/**
  * @brief  毫秒级延时
  * @param  xms 延时时长，范围：0~4294967295
  * @retval 无
  */
void Delay_ms(uint32_t xms)
{
	while(xms--)
	{
		Delay_us(1000);
	}
}

/**
  * @brief  秒级延时
  * @param  xs 延时时长，范围：0~4294967295
  * @retval 无
  */
void Delay_s(uint32_t xs)
{
	while(xs--)
	{
		Delay_ms(1000);
	}
}
