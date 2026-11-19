/**
 * @file    command_handler.h
 * @brief   Serial command handler module - extensible command processing
 * @details Provides a table-driven command dispatcher with easy registration
 */

#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

/**
 * @brief Response callback function type
 * @param response The response string to send
 *
 * P5: Command handlers call this callback instead of Serial_Printf directly.
 * In RTOS mode, this enqueues to TxQueue; in bare-metal mode, it sends directly.
 */
typedef void (*CommandHandler_ResponseCallback)(const char *response);

/**
 * @brief Initialize the command handler module
 */
void CommandHandler_Init(void);

/**
 * @brief Set response callback (P5: called by T_CMD to inject TxQueue sender)
 * @param callback Function to call for sending responses, or NULL for direct Serial_Printf
 */
void CommandHandler_SetResponseCallback(CommandHandler_ResponseCallback callback);

/**
 * @brief Process pending commands: 0=none, 1=read-only, 2=configuration/control.
 */
uint8_t CommandHandler_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* COMMAND_HANDLER_H */
