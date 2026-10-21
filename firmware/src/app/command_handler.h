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
 * @brief Initialize the command handler module
 */
void CommandHandler_Init(void);

/**
 * @brief Process pending serial commands (call from main loop)
 */
void CommandHandler_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* COMMAND_HANDLER_H */
