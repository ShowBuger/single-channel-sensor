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

/* -----------------------------------------------------------------------
 * Serial command processing
 * ----------------------------------------------------------------------- */
static void serial_command_process(void)
{
    if (!Serial_GetCommandFlag()) return;

    char *cmd = Serial_GetCommandBuffer();

    /* Check if in import state — delegate to import handler */
    if (ConfigImport_GetState() != IMPORT_STATE_IDLE)
    {
        ConfigImport_ProcessCommand(cmd);
        return;
    }

    /* Handle import commands */
    if (strncmp(cmd, "ML IMPORT_", 10) == 0)
    {
        ConfigImport_ProcessCommand(cmd);
        return;
    }

    if (strcmp(cmd, "ML START") == 0)
    {
        if (sensor_start() == 0)
        {
            Serial_Printf("Sensor started\r\n");
            Serial_Printf("Running: %s\r\n", SENSOR_Run[0] ? "sensor0" : "none");
        }
        else Serial_Printf("Failed to start\r\n");
    }
    else if (strcmp(cmd, "ML STOP") == 0)
    {
        Serial_Printf(sensor_stop() == 0 ? "Stopped\r\n" : "Stop failed\r\n");
    }
    else if (strcmp(cmd, "ML MERSURE") == 0)
    {
        if (sensor_start() == 0)
        {
            get_sensor_data();
            if (SENSOR_Run[0]) print_processed_sensor_data(0);
            sensor_stop();
        }
        else Serial_Printf("Start failed\r\n");
    }
    else if (strcmp(cmd, "ML CALIB") == 0)
    {
        Serial_Printf(perform_sensor_calibration() == 0
               ? "Calibration OK\r\n" : "Calibration FAIL\r\n");
    }
    else if (strncmp(cmd, "ML CHFLAG ", 10) == 0)
    {
        uint32_t val;
        if (sscanf(cmd + 10, "0x%lX", &val) == 1 ||
            sscanf(cmd + 10, "0X%lX", &val) == 1 ||
            sscanf(cmd + 10, "%lu",   &val) == 1)
        {
            if (val <= 0xFF)
            {
                FLAG_RES = (uint8_t)val;
                Config_Save_All();
                Serial_Printf("FLAG_RES=0x%02X\r\n", FLAG_RES);
            }
            else Serial_Printf("Out of range\r\n");
        }
        else Serial_Printf("Usage: ML CHFLAG <hex|dec>\r\n");
    }
    else if (strcmp(cmd, "ML FLAG") == 0)
        Serial_Printf("FLAG_RES: 0x%02X\r\n", FLAG_RES);
    else if (strcmp(cmd, "ML MAPPING") == 0)
    {
        Serial_Printf("X=%.3f Y=%.3f Z=%.3f\r\n",
                      X_MAPPING_VALUE, Y_MAPPING_VALUE, Z_MAPPING_VALUE);
    }
    else if (strncmp(cmd, "ML SETMAP ", 10) == 0)
    {
        char axis; float val;
        if (sscanf(cmd + 10, "%c %f", &axis, &val) == 2)
        {
            if      (axis=='X'||axis=='x') { X_MAPPING_VALUE=val; Mapping_Value_Save(); Serial_Printf("X=%.3f\r\n",val); }
            else if (axis=='Y'||axis=='y') { Y_MAPPING_VALUE=val; Mapping_Value_Save(); Serial_Printf("Y=%.3f\r\n",val); }
            else if (axis=='Z'||axis=='z') { Z_MAPPING_VALUE=val; Mapping_Value_Save(); Serial_Printf("Z=%.3f\r\n",val); }
            else Serial_Printf("Axis X/Y/Z\r\n");
        }
        else Serial_Printf("Usage: ML SETMAP <X|Y|Z> <val>\r\n");
    }
    /* ── New commands ─────────────────────────────────────────────────── */
    else if (strncmp(cmd, "ML SET ", 7) == 0)
    {
        char key[24];
        char value[32];
        if (sscanf(cmd + 7, "%23s %31s", key, value) == 2)
        {
            Config_TypeDef candidate = g_config;
            if (Config_ParseSingleKV(&candidate, key, value) || Config_Validate(&candidate)) {
                Serial_Printf("Error: invalid config %s\r\n", key);
                return;
            }
            g_config = candidate;
            FLAG_RES = g_config.FLAG_RES;
            X_MAPPING_VALUE = g_config.X_MAPPING_VALUE;
            Y_MAPPING_VALUE = g_config.Y_MAPPING_VALUE;
            Z_MAPPING_VALUE = g_config.Z_MAPPING_VALUE;
            AlgoParams_Load(&g_config);
            Log_SetLevel((LogLevel_t)g_config.log_level);
            Serial_Printf("SET %s=%s\r\n", key, value);
        }
        else Serial_Printf("Usage: ML SET <key> <value>\r\n");
    }
    else if (strcmp(cmd, "ML SAVECFG") == 0)
    {
        Serial_Printf(Config_Save_All() == 0 ? "Saved\r\n" : "Save FAIL\r\n");
    }
    else if (strcmp(cmd, "ML APPLYCFG") == 0)
    {
        FLAG_RES        = g_config.FLAG_RES;
        X_MAPPING_VALUE = g_config.X_MAPPING_VALUE;
        Y_MAPPING_VALUE = g_config.Y_MAPPING_VALUE;
        Z_MAPPING_VALUE = g_config.Z_MAPPING_VALUE;
        AlgoParams_Load(&g_config);
        uint8_t was_running = SENSOR_Run[0];
        sensor_stop();
        uint8_t failed = was_running ? sensor_start() : (Scan_Ports() == 0);
        Serial_Printf(failed ? "Config apply failed.\r\n" : "Config applied.\r\n");
    }
    else if (strcmp(cmd, "ML GETCFG") == 0)
    {
        /* Each printf stays below the UART's 100-byte formatting buffer.
         * This read-only snapshot reports configured sensor values and live
         * algorithm/log values; it is not an I2C register readback. */
        Serial_Printf("CFG_BEGIN\r\n");
        Serial_Printf("{\"product\":\"%s\",", g_config.product_model);
        Serial_Printf("\"sensor\":{\"type\":%u,\"gain\":%u,\"osr\":%u,\"filt\":%u,",
                      g_config.sensor_type, g_config.gain_sel, g_config.osr, g_config.dig_filt);
        Serial_Printf("\"resx\":%u,\"resy\":%u,\"resz\":%u,\"tcmp\":%u,\"range\":%u,",
                      g_config.res_x, g_config.res_y, g_config.res_z, g_config.tcmp_en, g_config.range);
        Serial_Printf("\"opmode\":%u,\"magch\":%u,\"version\":%u},",
                      g_config.op_mode, g_config.mag_ch, g_config.sensor_version);
        /* Nine significant digits round-trip an IEEE float32. */
        Serial_Printf("\"algo\":{\"flag\":\"0x%02X\",\"xmap\":%.9g,",
                      FLAG_RES, (double)X_MAPPING_VALUE);
        Serial_Printf("\"ymap\":%.9g,\"zmap\":%.9g,",
                      (double)Y_MAPPING_VALUE, (double)Z_MAPPING_VALUE);
        Serial_Printf("\"dead\":%.9g,\"spike\":%.9g,\"stab\":%.9g},",
                      (double)g_algo.dead_value, (double)g_algo.spike_multiplier,
                      (double)g_algo.stability_threshold);
        Serial_Printf("\"log\":{\"level\":%u}}\r\nCFG_END\r\n", (unsigned)Log_GetLevel());
    }
    else if (strncmp(cmd, "ML LOGLEVEL ", 12) == 0)
    {
        int lvl;
        if (sscanf(cmd + 12, "%d", &lvl) == 1 && lvl >= 0 && lvl <= 4)
        {
            Log_SetLevel((LogLevel_t)lvl);
            g_config.log_level = (uint8_t)lvl;
            Config_Save_All();
            Serial_Printf("LogLevel=%d\r\n", lvl);
        }
        else Serial_Printf("Usage: ML LOGLEVEL <0-4>\r\n");
    }
    else if (strcmp(cmd, "ML LOGLEVEL") == 0)
    {
        Serial_Printf("LogLevel=%d (0=NONE 1=E 2=W 3=I 4=D)\r\n", (int)Log_GetLevel());
    }
    else if (strcmp(cmd, "ML RESET") == 0)
    {
        Serial_Printf("System resetting...\r\n");
        HAL_Delay(100);  /* 等待串口发送完成 */
        NVIC_SystemReset();  /* 执行软件复位 */
    }
    else if (strcmp(cmd, "ML RESTORE") == 0)
    {
        Serial_Printf("Restoring factory defaults...\r\n");
        Config_RestoreDefault();
        Serial_Printf("Factory defaults restored. Resetting...\r\n");
        HAL_Delay(100);
        NVIC_SystemReset();
    }
    else Serial_Printf("Unknown: %s\r\n", cmd);
}

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

    LOG_I("SYS", "System Reset");
    LOG_I("SYS", "Reset flags=0x%08lX IWDG=%u", (unsigned long)reset_flags,
          (reset_flags & RCC_CSR_IWDGRSTF) ? 1U : 0U);
    IWDG_Feed();        /* 初始化耗时较长，喂一次防止在此处复位 */
    sensor_start();     /* 内含扫描、配置应用和校准 */

    while (1)
    {
        IWDG_Feed();        /* 主循环每次迭代喂狗 */
        serial_command_process();
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
