/**
 * @file    command_handler.c
 * @brief   Serial command handler implementation
 * @details Table-driven command dispatcher for extensibility
 */

#include "command_handler.h"
#include "drivers/bsp/bsp_uart.h"
#include "app/sensor_manager.h"
#include "app/algo_params.h"
#include "app/config_import.h"
#include "app/config_parser.h"
#include "app/config_manager.h"
#include "drivers/bsp/log_manager.h"
#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* -----------------------------------------------------------------------
 * Command handler function type
 * ----------------------------------------------------------------------- */
typedef void (*CommandHandler_Func)(const char *cmd);

typedef struct {
    const char *cmd_name;           /* Command string (e.g., "ML START") */
    CommandHandler_Func handler;    /* Handler function */
} CommandEntry_t;

/* -----------------------------------------------------------------------
 * Forward declarations of command handlers
 * ----------------------------------------------------------------------- */
static void Handle_Start(const char *cmd);
static void Handle_Stop(const char *cmd);
static void Handle_Measure(const char *cmd);
static void Handle_Calib(const char *cmd);
static void Handle_ChFlag(const char *cmd);
static void Handle_Flag(const char *cmd);
static void Handle_Mapping(const char *cmd);
static void Handle_SetMap(const char *cmd);
static void Handle_Set(const char *cmd);
static void Handle_SaveCfg(const char *cmd);
static void Handle_ApplyCfg(const char *cmd);
static void Handle_GetCfg(const char *cmd);
static void Handle_LogLevel(const char *cmd);
static void Handle_LogLevelQuery(const char *cmd);
static void Handle_Reset(const char *cmd);
static void Handle_Restore(const char *cmd);

/* -----------------------------------------------------------------------
 * Command table - ADD NEW COMMANDS HERE
 * ----------------------------------------------------------------------- */
static const CommandEntry_t g_command_table[] = {
    /* Basic sensor operations */
    {"ML START",        Handle_Start},
    {"ML STOP",         Handle_Stop},
    {"ML MERSURE",      Handle_Measure},
    {"ML CALIB",        Handle_Calib},

    /* Flag and mapping */
    {"ML CHFLAG ",      Handle_ChFlag},
    {"ML FLAG",         Handle_Flag},
    {"ML MAPPING",      Handle_Mapping},
    {"ML SETMAP ",      Handle_SetMap},

    /* Configuration */
    {"ML SET ",         Handle_Set},
    {"ML SAVECFG",      Handle_SaveCfg},
    {"ML APPLYCFG",     Handle_ApplyCfg},
    {"ML GETCFG",       Handle_GetCfg},

    /* Logging */
    {"ML LOGLEVEL ",    Handle_LogLevel},
    {"ML LOGLEVEL",     Handle_LogLevelQuery},

    /* System */
    {"ML RESET",        Handle_Reset},
    {"ML RESTORE",      Handle_Restore},
};

#define COMMAND_TABLE_SIZE (sizeof(g_command_table) / sizeof(g_command_table[0]))

/* -----------------------------------------------------------------------
 * Public API Implementation
 * ----------------------------------------------------------------------- */

void CommandHandler_Init(void)
{
    /* Reserved for future initialization if needed */
}

void CommandHandler_Process(void)
{
    if (!Serial_GetCommandFlag()) return;

    char *cmd = Serial_GetCommandBuffer();

    /* Priority 1: Handle import state commands */
    if (ConfigImport_GetState() != IMPORT_STATE_IDLE)
    {
        ConfigImport_ProcessCommand(cmd);
        return;
    }

    /* Priority 2: Handle import start commands */
    if (strncmp(cmd, "ML IMPORT_", 10) == 0)
    {
        ConfigImport_ProcessCommand(cmd);
        return;
    }

    /* Priority 3: Dispatch to command table */
    for (uint32_t i = 0; i < COMMAND_TABLE_SIZE; i++)
    {
        size_t cmd_len = strlen(g_command_table[i].cmd_name);
        if (strncmp(cmd, g_command_table[i].cmd_name, cmd_len) == 0)
        {
            g_command_table[i].handler(cmd);
            return;
        }
    }

    /* No match found */
    Serial_Printf("Unknown: %s\r\n", cmd);
}

/* -----------------------------------------------------------------------
 * Command handler implementations
 * ----------------------------------------------------------------------- */

static void Handle_Start(const char *cmd)
{
    (void)cmd;
    if (sensor_start() == 0)
    {
        Serial_Printf("Sensor started\r\n");
        Serial_Printf("Running: %s\r\n", SENSOR_Run[0] ? "sensor0" : "none");
    }
    else
    {
        Serial_Printf("Failed to start\r\n");
    }
}

static void Handle_Stop(const char *cmd)
{
    (void)cmd;
    Serial_Printf(sensor_stop() == 0 ? "Stopped\r\n" : "Stop failed\r\n");
}

static void Handle_Measure(const char *cmd)
{
    (void)cmd;
    if (sensor_start() == 0)
    {
        get_sensor_data();
        if (SENSOR_Run[0]) print_processed_sensor_data(0);
        sensor_stop();
    }
    else
    {
        Serial_Printf("Start failed\r\n");
    }
}

static void Handle_Calib(const char *cmd)
{
    (void)cmd;
    Serial_Printf(perform_sensor_calibration() == 0
           ? "Calibration OK\r\n" : "Calibration FAIL\r\n");
}

static void Handle_ChFlag(const char *cmd)
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
        else
        {
            Serial_Printf("Out of range\r\n");
        }
    }
    else
    {
        Serial_Printf("Usage: ML CHFLAG <hex|dec>\r\n");
    }
}

static void Handle_Flag(const char *cmd)
{
    (void)cmd;
    Serial_Printf("FLAG_RES: 0x%02X\r\n", FLAG_RES);
}

static void Handle_Mapping(const char *cmd)
{
    (void)cmd;
    Serial_Printf("X=%.3f Y=%.3f Z=%.3f\r\n",
                  X_MAPPING_VALUE, Y_MAPPING_VALUE, Z_MAPPING_VALUE);
}

static void Handle_SetMap(const char *cmd)
{
    char axis;
    float val;
    if (sscanf(cmd + 10, "%c %f", &axis, &val) == 2)
    {
        if (axis == 'X' || axis == 'x')
        {
            X_MAPPING_VALUE = val;
            Mapping_Value_Save();
            Serial_Printf("X=%.3f\r\n", val);
        }
        else if (axis == 'Y' || axis == 'y')
        {
            Y_MAPPING_VALUE = val;
            Mapping_Value_Save();
            Serial_Printf("Y=%.3f\r\n", val);
        }
        else if (axis == 'Z' || axis == 'z')
        {
            Z_MAPPING_VALUE = val;
            Mapping_Value_Save();
            Serial_Printf("Z=%.3f\r\n", val);
        }
        else
        {
            Serial_Printf("Axis X/Y/Z\r\n");
        }
    }
    else
    {
        Serial_Printf("Usage: ML SETMAP <X|Y|Z> <val>\r\n");
    }
}

static void Handle_Set(const char *cmd)
{
    char key[24];
    char value[32];
    if (sscanf(cmd + 7, "%23s %31s", key, value) == 2)
    {
        Config_TypeDef candidate = g_config;
        if (Config_ParseSingleKV(&candidate, key, value) || Config_Validate(&candidate))
        {
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
    else
    {
        Serial_Printf("Usage: ML SET <key> <value>\r\n");
    }
}

static void Handle_SaveCfg(const char *cmd)
{
    (void)cmd;
    Serial_Printf(Config_Save_All() == 0 ? "Saved\r\n" : "Save FAIL\r\n");
}

static void Handle_ApplyCfg(const char *cmd)
{
    (void)cmd;
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

static void Handle_GetCfg(const char *cmd)
{
    (void)cmd;
    /* Each printf stays below the UART's 100-byte formatting buffer.
     * This read-only snapshot reports configured sensor values and live
     * algorithm/log values; it is not an I2C register readback. */
    Serial_Printf("CFG_BEGIN\r\n");
    Serial_Printf("{\"product\":\"%s\",", g_config.product_model);
    Serial_Printf("\"sensor\":{\"type\":%u,\"gain\":%u,\"osr\":%u,\"filt\":%u,",
                  g_config.sensor_type, g_config.gain_sel, g_config.osr, g_config.dig_filt);
    Serial_Printf("\"resx\":%u,\"resy\":%u,\"resz\":%u,\"tcmp\":%u,\"range\":%u,",
                  g_config.res_x, g_config.res_y, g_config.res_z, g_config.tcmp_en, g_config.range);
    Serial_Printf("\"opmode\":%u,\"magch\":%u,\"version\":%u",
                  g_config.op_mode, g_config.mag_ch, g_config.sensor_version);
    /* mcyc只属于AK09974C，其他型号保持原有字段集合。 */
    if (g_config.sensor_type == SENSOR_TYPE_AK09974C)
        Serial_Printf(",\"mcyc\":%u", g_config.mcyc);
    Serial_Printf("},");
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

static void Handle_LogLevel(const char *cmd)
{
    int lvl;
    if (sscanf(cmd + 12, "%d", &lvl) == 1 && lvl >= 0 && lvl <= 4)
    {
        Log_SetLevel((LogLevel_t)lvl);
        g_config.log_level = (uint8_t)lvl;
        Config_Save_All();
        Serial_Printf("LogLevel=%d\r\n", lvl);
    }
    else
    {
        Serial_Printf("Usage: ML LOGLEVEL <0-4>\r\n");
    }
}

static void Handle_LogLevelQuery(const char *cmd)
{
    (void)cmd;
    Serial_Printf("LogLevel=%d (0=NONE 1=E 2=W 3=I 4=D)\r\n", (int)Log_GetLevel());
}

static void Handle_Reset(const char *cmd)
{
    (void)cmd;
    Serial_Printf("System resetting...\r\n");
    HAL_Delay(100);  /* 等待串口发送完成 */
    NVIC_SystemReset();  /* 执行软件复位 */
}

static void Handle_Restore(const char *cmd)
{
    (void)cmd;
    Serial_Printf("Restoring factory defaults...\r\n");
    Config_RestoreDefault();
    Serial_Printf("Factory defaults restored. Resetting...\r\n");
    HAL_Delay(100);
    NVIC_SystemReset();
}
