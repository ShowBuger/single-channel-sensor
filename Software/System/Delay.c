#include "stm32f10x.h"

/**
  * @brief  微秒级延时 (基于循环，不使用SysTick，避免与FreeRTOS冲突)
  * @param  xus 延时时长
  * @retval 无
  * @note   72MHz时钟下，每个循环约8个时钟周期
  */
void Delay_us(uint32_t xus)
{
	uint32_t i;
	for(; xus > 0; xus--)
	{
		/* 72MHz / 8 cycles = 9 loops per us */
		for(i = 0; i < 9; i++)
		{
			__NOP();
		}
	}
}

/**
  * @brief  毫秒级延时
  * @param  xms 延时时长，范围：0~4294967295
  * @retval 无
  */
void Delay_ms(uint32_t xms)
{
	uint32_t i;
	for(; xms > 0; xms--)
	{
		/* 1ms = 1000us, 72000 loops at 72MHz */
		for(i = 0; i < 9000; i++)
		{
			__NOP();
			__NOP();
			__NOP();
			__NOP();
			__NOP();
			__NOP();
			__NOP();
			__NOP();
		}
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
