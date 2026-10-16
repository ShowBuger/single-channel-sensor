#include "config_import.h"
#include "json_config_loader.h"
#include "config_manager.h"
#include "sensor_manager.h"
#include "algo_params.h"
#include "drivers/bsp/bsp_uart.h"
#include "drivers/bsp/log_manager.h"
#include <string.h>
#include <stdlib.h>

static char import_buffer[IMPORT_BUFFER_SIZE];
static uint16_t import_index = 0;
static uint16_t import_expected_size = 0;
static ImportState_t import_state = IMPORT_STATE_IDLE;

void ConfigImport_Init(void)
{
    import_state = IMPORT_STATE_IDLE;
    import_index = 0;
    import_expected_size = 0;
    memset(import_buffer, 0, sizeof(import_buffer));
}

ImportState_t ConfigImport_GetState(void)
{
    return import_state;
}

void ConfigImport_Reset(void)
{
    ConfigImport_Init();
}

void ConfigImport_ProcessCommand(const char *cmd)
{
    /* ML IMPORT_START <bytes> */
    if (strncmp(cmd, "ML IMPORT_START ", 16) == 0)
    {
        int size = atoi(cmd + 16);
        if (size <= 0 || size >= IMPORT_BUFFER_SIZE)
        {
            Serial_Printf("IMPORT_ERROR: size=%d exceeds buffer=%d\r\n", size, IMPORT_BUFFER_SIZE);
            import_state = IMPORT_STATE_ERROR;
            return;
        }

        import_expected_size = (uint16_t)size;
        import_index = 0;
        import_state = IMPORT_STATE_RECEIVING;
        memset(import_buffer, 0, sizeof(import_buffer));
        Serial_Printf("READY\r\n");
    }
    /* ML IMPORT_DATA <json_chunk> */
    else if (strncmp(cmd, "ML IMPORT_DATA ", 15) == 0)
    {
        if (import_state != IMPORT_STATE_RECEIVING)
        {
            Serial_Printf("IMPORT_ERROR: not in receiving state\r\n");
            return;
        }

        const char *data = cmd + 15;
        uint16_t len = (uint16_t)strlen(data);

        if (import_index + len >= IMPORT_BUFFER_SIZE || import_index + len > import_expected_size)
        {
            Serial_Printf("IMPORT_ERROR: buffer overflow\r\n");
            import_state = IMPORT_STATE_ERROR;
            return;
        }

        memcpy(import_buffer + import_index, data, len);
        import_index += len;
        Serial_Printf("ACK_%u\r\n", import_index);
    }
    /* ML IMPORT_END */
    else if (strcmp(cmd, "ML IMPORT_END") == 0)
    {
        if (import_state != IMPORT_STATE_RECEIVING)
        {
            Serial_Printf("IMPORT_ERROR: not in receiving state\r\n");
            return;
        }

        if (import_index != import_expected_size) {
            Serial_Printf("IMPORT_ERROR: incomplete payload\r\n");
            import_state = IMPORT_STATE_ERROR;
            return;
        }
        import_buffer[import_index] = '\0';

        /* Parse JSON */
        Config_TypeDef new_config;
        JsonParseResult_t result;

        if (JsonConfig_Parse(import_buffer, &new_config, &result) != 0)
        {
            Serial_Printf("IMPORT_ERROR: parse failed at line %u, key=%s, status=%u\r\n",
                          result.error_line, result.error_key, result.status);
            import_state = IMPORT_STATE_ERROR;
            return;
        }

        /* Validate */
        uint8_t validation_error = JsonConfig_Validate(&new_config);
        if (validation_error != 0)
        {
            Serial_Printf("IMPORT_ERROR: validation failed code=%u (product/type/range)\r\n", validation_error);
            import_state = IMPORT_STATE_ERROR;
            return;
        }

        /* Apply to global config */
        memcpy(&g_config, &new_config, sizeof(Config_TypeDef));

        /* Apply to runtime */
        FLAG_RES = g_config.FLAG_RES;
        X_MAPPING_VALUE = g_config.X_MAPPING_VALUE;
        Y_MAPPING_VALUE = g_config.Y_MAPPING_VALUE;
        Z_MAPPING_VALUE = g_config.Z_MAPPING_VALUE;
        AlgoParams_Load(&g_config);
        Log_SetLevel((LogLevel_t)g_config.log_level);

        /* Save to flash */
        if (Config_Save_All() != 0)
        {
            Serial_Printf("IMPORT_ERROR: save to flash failed\r\n");
            import_state = IMPORT_STATE_ERROR;
            return;
        }

        /* 按型号应用到正确的句柄；停止状态下也写入寄存器。 */
        uint8_t was_running = SENSOR_Run[0];
        sensor_stop();
        uint8_t failed = was_running ? sensor_start() : (Scan_Ports() == 0);
        if (failed) {
            Serial_Printf("IMPORT_ERROR: config saved but sensor apply failed\r\n");
            import_state = IMPORT_STATE_ERROR;
            return;
        }

        import_state = IMPORT_STATE_COMPLETE;
        Serial_Printf("IMPORT_OK: config applied and saved\r\n");
    }
    /* ML IMPORT_CANCEL */
    else if (strcmp(cmd, "ML IMPORT_CANCEL") == 0)
    {
        ConfigImport_Reset();
        Serial_Printf("IMPORT_CANCELLED\r\n");
    }
}
