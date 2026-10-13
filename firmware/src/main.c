#include "stm32f1xx_hal.h"
#include "drivers/bsp/bsp_delay.h"
#include "drivers/bsp/bsp_i2c_soft.h"
#include "drivers/bsp/bsp_uart.h"
#include "drivers/sensors/mlx90393.h"
#include "drivers/sensors/tca9548a.h"
#include "app/sensor_manager.h"
#include "app/algo_params.h"
#include "drivers/sensors/sensor_hal.h"
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
    HAL_IWDG_Refresh(&hiwdg);
}

/* -----------------------------------------------------------------------
 * Serial command processing
 * ----------------------------------------------------------------------- */
static void serial_command_process(void)
{
    if (!Serial_GetCommandFlag()) return;

    char *cmd = Serial_GetCommandBuffer();

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
        float fval;
        int   ival;
        if (sscanf(cmd + 7, "%23s %f", key, &fval) == 2)
        {
            ival = (int)fval;
            uint8_t ok = 1;
            if      (!strcmp(key,"sensor.gain"))  { g_config.gain_sel=(uint8_t)ival; }
            else if (!strcmp(key,"sensor.osr"))   { g_config.osr=(uint8_t)ival; }
            else if (!strcmp(key,"sensor.filt"))  { g_config.dig_filt=(uint8_t)ival; }
            else if (!strcmp(key,"sensor.resx"))  { g_config.res_x=(uint8_t)ival; }
            else if (!strcmp(key,"sensor.resy"))  { g_config.res_y=(uint8_t)ival; }
            else if (!strcmp(key,"sensor.resz"))  { g_config.res_z=(uint8_t)ival; }
            else if (!strcmp(key,"sensor.tcmp"))  { g_config.tcmp_en=(uint8_t)ival; }
            else if (!strcmp(key,"algo.flag"))    { g_config.FLAG_RES=(uint8_t)ival; FLAG_RES=g_config.FLAG_RES; }
            else if (!strcmp(key,"algo.xmap"))    { g_config.X_MAPPING_VALUE=fval; X_MAPPING_VALUE=fval; }
            else if (!strcmp(key,"algo.ymap"))    { g_config.Y_MAPPING_VALUE=fval; Y_MAPPING_VALUE=fval; }
            else if (!strcmp(key,"algo.zmap"))    { g_config.Z_MAPPING_VALUE=fval; Z_MAPPING_VALUE=fval; }
            else if (!strcmp(key,"algo.dead"))    { g_config.dead_value=fval; g_algo.dead_value=fval; }
            else if (!strcmp(key,"algo.spike"))   { g_config.spike_multiplier=fval; g_algo.spike_multiplier=fval; }
            else if (!strcmp(key,"algo.stab"))    { g_config.stability_threshold=fval; g_algo.stability_threshold=fval; }
            else { Serial_Printf("Unknown key: %s\r\n", key); ok=0; }
            if (ok) Serial_Printf("SET %s=%.3f\r\n", key, fval);
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
        if (SENSOR_Run[0]) { sensor_stop(); sensor_start(); }
        Serial_Printf("Config applied.\r\n");
    }
    else if (strcmp(cmd, "ML GETCFG") == 0)
    {
        Serial_Printf("{\"sensor\":{\"gain\":%u,\"osr\":%u,\"filt\":%u,"
                      "\"resx\":%u,\"resy\":%u,\"resz\":%u,\"tcmp\":%u},"
                      "\"algo\":{\"flag\":\"0x%02X\",\"xmap\":%.3f,"
                      "\"ymap\":%.3f,\"zmap\":%.3f,\"dead\":%.3f,"
                      "\"spike\":%.3f,\"stab\":%.3f}}\r\n",
                      g_config.gain_sel, g_config.osr, g_config.dig_filt,
                      g_config.res_x, g_config.res_y, g_config.res_z, g_config.tcmp_en,
                      g_config.FLAG_RES,
                      g_config.X_MAPPING_VALUE, g_config.Y_MAPPING_VALUE, g_config.Z_MAPPING_VALUE,
                      g_config.dead_value, g_config.spike_multiplier, g_config.stability_threshold);
    }
    else Serial_Printf("Unknown: %s\r\n", cmd);
}

/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    Delay_Init();

    SoftI2C_Init();
    Serial_Init();
    Config_Init_All();

    Serial_Printf("System_Reset\r\n");
    Scan_Ports();
    sensor_start();

    while (1)
    {
        serial_command_process();
        if (SENSOR_Run[0] == 1)
        {
            get_sensor_data();
            print_processed_sensor_data(0);
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
