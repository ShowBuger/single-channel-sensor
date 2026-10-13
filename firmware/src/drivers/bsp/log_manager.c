#include "log_manager.h"
#include "bsp_uart.h"
#include <stdarg.h>
#include <stdio.h>

static LogLevel_t g_log_level = LOG_INFO;

void Log_Init(LogLevel_t level)
{
    g_log_level = level;
}

void Log_SetLevel(LogLevel_t level)
{
    g_log_level = level;
}

LogLevel_t Log_GetLevel(void)
{
    return g_log_level;
}

void Log_Print(LogLevel_t level, const char *tag, const char *fmt, ...)
{
    static char buf[128];
    va_list args;
    const char *prefix;

    if (level > g_log_level) return;

    switch (level) {
        case LOG_ERROR: prefix = "E"; break;
        case LOG_WARN:  prefix = "W"; break;
        case LOG_INFO:  prefix = "I"; break;
        case LOG_DEBUG: prefix = "D"; break;
        default: return;
    }

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    Serial_Printf("[%s][%s] %s\r\n", (char *)prefix, (char *)tag, buf);
}
