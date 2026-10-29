#include "stm32l4xx_hal.h"
#include "drivers/bsp/bsp_delay.h"
#include "drivers/bsp/bsp_i2c_soft.h"
#include "drivers/bsp/bsp_uart.h"
#include "drivers/sensors/sensor_hal.h"
#include "drivers/sensors/tca9548a/tca9548a.h"
#include "app/sensor_manager.h"
#include "app/algo_params.h"
#include "app/config_import.h"
#include "app/config_parser.h"
#include "app/config_manager.h"
#include "app/command_handler.h"
#include "drivers/sensors/sensor_hal.h"
#include "drivers/bsp/log_manager.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

/* Forward declaration */
static void SystemClock_Config(void);

/* -----------------------------------------------------------------------
 * IWDG
 * ----------------------------------------------------------------------- */
static IWDG_HandleTypeDef hiwdg;

static void IWDG_Init(void)
{
    hiwdg.Instance       = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_32;   /* 32kHz/32 = 1kHz     */
    hiwdg.Init.Reload    = 1250;                 /* ~1.25 second timeout */
    hiwdg.Init.Window    = IWDG_WINDOW_DISABLE;  /* L4 有窗口功能，未关闭时 Window=0，喂狗即复位 */
    HAL_IWDG_Init(&hiwdg);
}

void IWDG_Feed(void)
{
    if (hiwdg.Instance != NULL) HAL_IWDG_Refresh(&hiwdg);
}

/* Serial command processing is now handled by command_handler module */

/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */
int main(void)
{
    uint32_t reset_flags = RCC->CSR;
    __HAL_RCC_CLEAR_RESET_FLAGS();
    HAL_Init();
    SystemClock_Config();
    Delay_Init();

    SoftI2C_Init();
    Serial_Init();
    IWDG_Init();        /* 启动看门狗，统一在代码里管理，避免依赖 option bytes */
    Config_Init_All();
    ConfigImport_Init(); /* 初始化配置导入模块 */
    CommandHandler_Init(); /* 初始化命令处理模块 */

    LOG_I("SYS", "System Reset");
    LOG_I("SYS", "Reset flags=0x%08lX IWDG=%u", (unsigned long)reset_flags,
          (reset_flags & RCC_CSR_IWDGRSTF) ? 1U : 0U);
    IWDG_Feed();        /* 初始化耗时较长，喂一次防止在此处复位 */
    sensor_start();     /* 内含扫描、配置应用和校准 */

    while (1)
    {
        IWDG_Feed();        /* 主循环每次迭代喂狗 */
        CommandHandler_Process();
        if (SENSOR_Run[0] == 1)
        {
            get_sensor_data();
            print_processed_sensor_data(0);
            HAL_Delay(100);  /* 100ms 间隔 = 10 Hz 采集频率 */
        }
    }
}

/* -----------------------------------------------------------------------
 * System clock: HSI16 MHz -> PLL x10 / 2 = 80 MHz
 * ----------------------------------------------------------------------- */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* Configure power regulator (STM32L4 specific) */
    __HAL_RCC_PWR_CLK_ENABLE();
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
    {
        /* Voltage scaling error */
        while (1);
    }

    /* HSI16 -> PLL: 16MHz / M:1 * N:10 / R:2 = 80MHz */
    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI;
    osc.HSIState            = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.LSIState            = RCC_LSI_ON;  /* For IWDG */
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM            = 1;   /* 16MHz / 1 = 16MHz */
    osc.PLL.PLLN            = 10;  /* 16MHz * 10 = 160MHz */
    osc.PLL.PLLP            = RCC_PLLP_DIV7;
    osc.PLL.PLLQ            = RCC_PLLQ_DIV2;
    osc.PLL.PLLR            = RCC_PLLR_DIV2;  /* 160MHz / 2 = 80MHz (SYSCLK) */

    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        /* Clock configuration error */
        while (1);
    }

    /* Configure system clocks */
    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* HCLK  = 80 MHz */
    clk.APB1CLKDivider = RCC_HCLK_DIV1;     /* PCLK1 = 80 MHz */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;     /* PCLK2 = 80 MHz */

    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK)
    {
        /* Clock configuration error */
        while (1);
    }
}
