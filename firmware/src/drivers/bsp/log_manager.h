#ifndef __LOG_MANAGER_H
#define __LOG_MANAGER_H

#include <stdint.h>

typedef enum {
    LOG_NONE  = 0,
    LOG_ERROR = 1,
    LOG_WARN  = 2,
    LOG_INFO  = 3,
    LOG_DEBUG = 4
} LogLevel_t;

/* Compile-time minimum — set to LOG_NONE to strip all logs at -O1+ */
#ifndef LOG_COMPILE_LEVEL
#define LOG_COMPILE_LEVEL LOG_DEBUG
#endif

void Log_Init(LogLevel_t level);
void Log_SetLevel(LogLevel_t level);
LogLevel_t Log_GetLevel(void);
void Log_Print(LogLevel_t level, const char *tag, const char *fmt, ...);

/* Convenience macros */
#define LOG_E(tag, fmt, ...) Log_Print(LOG_ERROR, tag, fmt, ##__VA_ARGS__)
#define LOG_W(tag, fmt, ...) Log_Print(LOG_WARN,  tag, fmt, ##__VA_ARGS__)
#define LOG_I(tag, fmt, ...) Log_Print(LOG_INFO,  tag, fmt, ##__VA_ARGS__)
#define LOG_D(tag, fmt, ...) Log_Print(LOG_DEBUG, tag, fmt, ##__VA_ARGS__)

#endif /* __LOG_MANAGER_H */
