#include "stm32f1xx_hal.h"
#include "drivers/bsp/bsp_delay.h"
#include "drivers/bsp/bsp_i2c_soft.h"
#include "drivers/bsp/bsp_uart.h"
#include "drivers/sensors/mlx90393.h"
#include "drivers/sensors/tca9548a.h"
#include "app/sensor_manager.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

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
 * Serial command processing (same logic as before)
 * ----------------------------------------------------------------------- */
static void serial_command_process(void)
{
    if (!Serial_GetCommandFlag()) return;

    char *cmd = Serial_GetCommandBuffer();

    if (strcmp(cmd, "ML START") == 0)
    {
        if (sensor_start() == 0)
        {
            Serial_Printf("Sensor started successfully\r\n");
            Serial_Printf("Running sensor: ");
            Serial_Printf(SENSOR_Run[0] == 1 ? "0\r\n" : "None\r\n");
        }
        else Serial_Printf("Failed to start sensors\r\n");
    }
    else if (strcmp(cmd, "ML STOP") == 0)
    {
        Serial_Printf(sensor_stop() == 0 ? "Sensor stopped\r\n" : "Failed to stop sensor\r\n");
    }
    else if (strcmp(cmd, "ML MERSURE") == 0)
    {
        if (sensor_start() == 0)
        {
            Serial_Printf("Sensor started for single measurement\r\n");
            get_sensor_data();
            if (SENSOR_Run[0] == 1) print_processed_sensor_data(0);
            Serial_Printf(sensor_stop() == 0
                   ? "Single measurement completed\r\n"
                   : "Warning: Failed to stop sensor\r\n");
        }
        else Serial_Printf("Failed to start sensor for measurement\r\n");
    }
    else if (strcmp(cmd, "ML CALIB") == 0)
    {
        Serial_Printf(perform_sensor_calibration() == 0
               ? "Calibration completed\r\n"
               : "Calibration failed\r\n");
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
                uint8_t old = FLAG_RES;
                FLAG_RES = (uint8_t)val;
                Serial_Printf("FLAG_RES updated: 0x%02X -> 0x%02X\r\n", old, FLAG_RES);
                Config_Save_All();
            }
            else Serial_Printf("Error: Value out of range (0-255)\r\n");
        }
        else
        {
            Serial_Printf("Error: Invalid format. Usage: ML CHFLAG <value>\r\n");
            Serial_Printf("Example: ML CHFLAG 0x1B or ML CHFLAG 27\r\n");
        }
    }
    else if (strcmp(cmd, "ML FLAG") == 0)
        Serial_Printf("FLAG_RES: 0x%02X\r\n", FLAG_RES);
    else if (strcmp(cmd, "ML MAPPING") == 0)
    {
        Serial_Printf("Mapping values:\r\n");
        Serial_Printf("  X: %.3f\r\n", X_MAPPING_VALUE);
        Serial_Printf("  Y: %.3f\r\n", Y_MAPPING_VALUE);
        Serial_Printf("  Z: %.3f\r\n", Z_MAPPING_VALUE);
    }
    else if (strncmp(cmd, "ML SETMAP ", 10) == 0)
    {
        char axis; float val;
        if (sscanf(cmd + 10, "%c %f", &axis, &val) == 2)
        {
            if      (axis == 'X' || axis == 'x') { X_MAPPING_VALUE = val; Mapping_Value_Save(); Serial_Printf("X_MAPPING_VALUE updated: %.3f\r\n", val); }
            else if (axis == 'Y' || axis == 'y') { Y_MAPPING_VALUE = val; Mapping_Value_Save(); Serial_Printf("Y_MAPPING_VALUE updated: %.3f\r\n", val); }
            else if (axis == 'Z' || axis == 'z') { Z_MAPPING_VALUE = val; Mapping_Value_Save(); Serial_Printf("Z_MAPPING_VALUE updated: %.3f\r\n", val); }
            else Serial_Printf("Error: Invalid axis '%c'. Use X, Y, or Z\r\n", axis);
        }
        else
        {
            Serial_Printf("Error: Invalid format. Usage: ML SETMAP <X|Y|Z> <value>\r\n");
            Serial_Printf("Example: ML SETMAP X 0.5\r\n");
        }
    }
    else Serial_Printf("Unknown command: %s\r\n", cmd);
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
