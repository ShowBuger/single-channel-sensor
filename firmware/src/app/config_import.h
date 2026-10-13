#ifndef __CONFIG_IMPORT_H
#define __CONFIG_IMPORT_H

#include <stdint.h>

/**
 * @file config_import.h
 * @brief JSON config import via UART
 *
 * Protocol:
 * 1. Host sends: ML IMPORT_START <total_bytes>
 * 2. Device replies: READY
 * 3. Host sends JSON in chunks (up to 64 bytes per line)
 * 4. Device buffers and replies: ACK_<bytes_received>
 * 5. Host sends: ML IMPORT_END
 * 6. Device parses, validates, applies, saves, and replies: IMPORT_OK or IMPORT_ERROR <reason>
 */

#define IMPORT_BUFFER_SIZE  1024  /* Max JSON size */

typedef enum {
    IMPORT_STATE_IDLE = 0,
    IMPORT_STATE_RECEIVING,
    IMPORT_STATE_COMPLETE,
    IMPORT_STATE_ERROR
} ImportState_t;

void ConfigImport_Init(void);
void ConfigImport_ProcessCommand(const char *cmd);
ImportState_t ConfigImport_GetState(void);
void ConfigImport_Reset(void);

#endif /* __CONFIG_IMPORT_H */
