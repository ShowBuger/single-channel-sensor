#include "plat.h"
#include "board.h"
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

/* -----------------------------------------------------------------------
 * Watchdog — the application-facing name stays IWDG_Feed (used across modules
 * and hooked by the emulator tests); the hardware lives in the port layer.
 * ----------------------------------------------------------------------- */
void IWDG_Feed(void)
{
    plat_wdt_feed();
}

/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */
/* P3: FreeRTOS entry point (rtos_main.c) */
#ifdef USE_FREERTOS
extern void rtos_main(void);
#endif

int main(void)
{
    plat_sys_init();    /* caches, HAL, clock tree; captures reset flags */
    Delay_Init();

    SoftI2C_Init();
    Serial_Init();
    plat_wdt_start();   /* 启动看门狗，统一在代码里管理，避免依赖 option bytes */
    Config_Init_All();
    ConfigImport_Init(); /* 初始化配置导入模块 */
    CommandHandler_Init(); /* 初始化命令处理模块 */

    LOG_I("SYS", "System Reset");
    LOG_I("SYS", "Reset flags=0x%08lX IWDG=%u", (unsigned long)plat_reset_flags(),
          (unsigned)plat_reset_was_watchdog());
    IWDG_Feed();        /* 初始化耗时较长，喂一次防止在此处复位 */
    sensor_start();     /* 内含扫描、配置应用和校准 */

#ifdef USE_FREERTOS
    /* P3: Start FreeRTOS scheduler (does not return) */
    rtos_main();
#else
    /* Bare-metal super loop (P0-P2 behavior) */
    while (1)
    {
        IWDG_Feed();        /* 主循环每次迭代喂狗 */
        CommandHandler_Process();
        if (SENSOR_Run[0] == 1)
        {
            get_sensor_data();
            print_processed_sensor_data(0);
            plat_delay_ms(100);  /* 100ms 间隔 = 10 Hz 采集频率 */
        }
    }
#endif
}
