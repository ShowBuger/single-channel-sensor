/**
 * @file  rtos_types.h
 * @brief P4: Data structures for inter-task communication.
 *
 * Defines the packets and messages that flow between tasks:
 *   - SamplePacket_t: T_ACQ → T_PROC
 *   - TxMessage_t:    T_CMD/T_PROC → T_TX
 */

#ifndef RTOS_TYPES_H
#define RTOS_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* ── Sample packet: T_ACQ → T_PROC ────────────────────────────────────── */

#include "algo_pipeline.h"
typedef struct {
    AlgoPair_t value;
    AlgoTiming_t timing;
    uint32_t epoch;
} SamplePacket_t;

/* ── TX message: T_CMD/T_PROC → T_TX ──────────────────────────────────── */

#define TX_MSG_MAX_LEN  384   /**< Maximum message length (includes null terminator) */

/**
 * Message type for the single-writer transmitter.
 */
typedef enum {
    TX_MSG_RESPONSE,   /**< Command response (T_CMD → T_TX) */
    TX_MSG_DATA,       /**< Processed sensor data (T_PROC → T_TX) */
    TX_MSG_LOG         /**< Log message (any task → T_TX), not used in P4 */
} TxMsgType_t;

/**
 * A complete message to be sent by T_TX.
 * The message is null-terminated and sent atomically.
 */
typedef struct {
    TxMsgType_t type;
    char text[TX_MSG_MAX_LEN];
} TxMessage_t;

#endif /* RTOS_TYPES_H */
