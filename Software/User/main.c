#include "stm32f10x.h"
#include "mlx90393.h"
#include "sensor_manager.h"
#include "HardI2C.h"
#include "SoftI2C.h"
#include "Serial.h"
#include "Delay.h"
#include "TCA9548A.h"
#include "tmag3001.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

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

void serial_command_process()
{
 // 处理串口命令
			if(Serial_GetCommandFlag())
			{
					char* cmd = Serial_GetCommandBuffer();
					// 处理 ML START 命令
					if(strcmp(cmd, "ML START") == 0)
					{
							if(sensor_start() == 0)
							{
									printf("Sensor started successfully\r\n");
									// 打印运行中的传感器 - 仅1个传感器
									printf("Running sensor: ");
									if(SENSOR_Run[0] == 1)
									{
											printf("0\r\n");
									}
									else
									{
											printf("None\r\n");
									}
							}
							else
							{
									printf("Failed to start sensors\r\n");
							}
					}
					// 处理 ML STOP 命令
					else if(strcmp(cmd, "ML STOP") == 0)
					{
							if(sensor_stop() == 0)
							{
									printf("Sensor stopped\r\n");
							}
							else
							{
									printf("Failed to stop sensor\r\n");
							}
					}
					// 处理 ML MERSURE 命令 - 单次测量
					else if(strcmp(cmd, "ML MERSURE") == 0)
					{
							// 启动传感器
							if(sensor_start() == 0)
							{
									printf("Sensor started for single measurement\r\n");

									// 获取数据
									get_sensor_data();

									// 打印传感器数据 - 仅1个传感器
									if(SENSOR_Run[0] == 1)
									{
											print_processed_sensor_data(0);
									}

									// 停止传感器
									if(sensor_stop() == 0)
									{
											printf("Single measurement completed\r\n");
									}
									else
									{
											printf("Warning: Failed to stop sensor\r\n");
									}
							}
							else
							{
									printf("Failed to start sensor for measurement\r\n");
							}
					}
					else if(strcmp(cmd, "ML CALIB") == 0)
					{
							if(perform_sensor_calibration() == 0)
							{
									printf("Calibration completed\r\n");
							}
							else
							{
									printf("Calibration failed\r\n");
							}
					}
					// 处理 ML CHFLAG data 命令 - 修改 FLAG_RES 的值
					else if(strncmp(cmd, "ML CHFLAG ", 10) == 0)
					{
							uint32_t new_flag_value;
							// 尝试解析十六进制格式 (0x开头) 或十进制格式
							if(sscanf(cmd + 10, "0x%X", &new_flag_value) == 1 ||
							   sscanf(cmd + 10, "0X%X", &new_flag_value) == 1 ||
							   sscanf(cmd + 10, "%u", &new_flag_value) == 1)
							{
									if(new_flag_value <= 0xFF)  // 确保值在 uint8_t 范围内
									{
											uint8_t old_value = FLAG_RES;
											FLAG_RES = (uint8_t)new_flag_value;
											printf("FLAG_RES updated: 0x%02X -> 0x%02X\r\n", old_value, FLAG_RES);

											// 保存到Flash
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
									printf("Example: ML CHFLAG 0x1B or ML CHFLAG 27\r\n");
							}
					}
					// 处理 ML FLAG 命令 - 显示FLAG_RES
					else if(strcmp(cmd, "ML FLAG") == 0)
					{
							printf("FLAG_RES: 0x%02X\r\n", FLAG_RES);
					}
					// 处理 ML MAPPING 命令 - 显示映射系数
					else if(strcmp(cmd, "ML MAPPING") == 0)
					{
							printf("Mapping values:\r\n");
							printf("  X: %.3f\r\n", X_MAPPING_VALUE);
							printf("  Y: %.3f\r\n", Y_MAPPING_VALUE);
							printf("  Z: %.3f\r\n", Z_MAPPING_VALUE);
					}
					// 处理 ML SETMAP X/Y/Z value 命令 - 修改映射系数
					else if(strncmp(cmd, "ML SETMAP ", 10) == 0)
					{
							char axis;
							float new_value;
							// 解析命令: ML SETMAP X 0.5
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
									printf("Example: ML SETMAP X 0.5\r\n");
							}
					}
					else
					{
							printf("Unknown command: %s\r\n", cmd);
					}
			}
}
int main(void)
{
    SYSTEM_Init();
    /*初始化硬件IIC*/
    HardI2C_Init();
    /*初始化串口*/
    Serial_Init();
    /*初始化配置参数 - 从Flash读取*/
    Config_Init_All();
    printf("System_Reset\r\n");
    Scan_Ports();
    sensor_start();
    while(1)
    {
        serial_command_process();
        if(SENSOR_Run[0] == 1)
        {
            get_sensor_data();
            print_processed_sensor_data(0);
        }
    }
}
