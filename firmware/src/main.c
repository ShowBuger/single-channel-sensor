#include "stm32f1xx_hal.h"
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
    hiwdg.Init.Prescaler = IWDG_PRESCALER_32;   /* 40kHz/32 = 1.25kHz */
    hiwdg.Init.Reload    = 1250;                 /* ~1 second timeout  */
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
 * System clock: HSE 8 MHz -> PLL x9 = 72 MHz
 * ----------------------------------------------------------------------- */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* Try HSE first (8 MHz external crystal -> PLL x9 = 72 MHz) */
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_LSI;
    osc.HSEState       = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.LSIState       = RCC_LSI_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL     = RCC_PLL_MUL9;

    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        /* HSE failed -- fall back to HSI (8 MHz internal -> PLL x9 = 72 MHz) */
        osc.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI;
        osc.HSEState       = RCC_HSE_OFF;
        osc.HSIState       = RCC_HSI_ON;
        osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
        osc.PLL.PLLSource  = RCC_PLLSOURCE_HSI_DIV2;  /* HSI/2 = 4 MHz x18 = 72 MHz */
        osc.PLL.PLLMUL     = RCC_PLL_MUL16;           /* 4 MHz x16 = 64 MHz (HSI safe) */
        HAL_RCC_OscConfig(&osc);
    }

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2);
}
