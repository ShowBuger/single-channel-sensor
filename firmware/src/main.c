#include "stm32h7xx_hal.h"
#include "board_config.h"
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
    hiwdg.Instance       = BSP_IWDG_INSTANCE;
    hiwdg.Init.Prescaler = BSP_IWDG_PRESCALER;   /* LSI 32kHz/32 = 1kHz */
    hiwdg.Init.Reload    = BSP_IWDG_RELOAD;      /* ~1 second timeout */
    hiwdg.Init.Window    = IWDG_WINDOW_DISABLE;  /* 未关闭时 Window=0，喂狗即复位 */
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
    uint32_t reset_flags = RCC->RSR;
    __HAL_RCC_CLEAR_RESET_FLAGS();
    SCB_EnableICache();
    SCB_EnableDCache();
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
          (reset_flags & RCC_RSR_IWDG1RSTF) ? 1U : 0U);
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
 * System clock: HSI 64 MHz / 4 * 50 / 2 = 400 MHz
 * ----------------------------------------------------------------------- */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* Configure power supply and regulator (STM32H7 specific)
     * VOS1 allows 400 MHz on both rev Y and rev V silicon. */
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    /* HSI64 -> PLL1: 64MHz / M:4 * N:50 / P:2 = 400MHz */
    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI;
    osc.HSIState            = RCC_HSI_DIV1;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.LSIState            = RCC_LSI_ON;  /* For IWDG */
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM            = 4;   /* 64MHz / 4 = 16MHz */
    osc.PLL.PLLN            = 50;  /* 16MHz * 50 = 800MHz (VCO) */
    osc.PLL.PLLP            = 2;   /* 800MHz / 2 = 400MHz (SYSCLK) */
    osc.PLL.PLLQ            = 4;
    osc.PLL.PLLR            = 2;
    osc.PLL.PLLRGE          = RCC_PLL1VCIRANGE_3;  /* 8-16MHz input */
    osc.PLL.PLLVCOSEL       = RCC_PLL1VCOWIDE;
    osc.PLL.PLLFRACN        = 0;

    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        /* Clock configuration error */
        while (1);
    }

    /* Configure system clocks */
    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 |
                         RCC_CLOCKTYPE_PCLK2   | RCC_CLOCKTYPE_D3PCLK1;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.SYSCLKDivider  = RCC_SYSCLK_DIV1;   /* CPU   = 400 MHz */
    clk.AHBCLKDivider  = RCC_HCLK_DIV2;     /* HCLK  = 200 MHz */
    clk.APB3CLKDivider = RCC_APB3_DIV2;     /* PCLK3 = 100 MHz */
    clk.APB1CLKDivider = RCC_APB1_DIV2;     /* PCLK1 = 100 MHz */
    clk.APB2CLKDivider = RCC_APB2_DIV2;     /* PCLK2 = 100 MHz (USART1) */
    clk.APB4CLKDivider = RCC_APB4_DIV2;     /* PCLK4 = 100 MHz */

    /* VOS1 + HCLK 200MHz -> 2 wait states */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK)
    {
        /* Clock configuration error */
        while (1);
    }
}
