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

/**
 * Raw sample from one sensor (actual or reference).
 * Physical units are already converted by the driver.
 */
typedef struct {
    float x_uT;        /**< X-axis magnetic field (µT) */
    float y_uT;        /**< Y-axis magnetic field (µT) */
    float z_uT;        /**< Z-axis magnetic field (µT) */
    float temp_C;      /**< Temperature (°C), if supported */
    bool  valid;       /**< false if timeout/overflow/error */
} SensorReading_t;

/**
 * A sample packet from one acquisition cycle.
 * Contains readings from both actual and reference sensors, plus metadata.
 */
typedef struct {
    uint32_t seq;              /**< Sequence number (rolls over at UINT32_MAX) */
    uint32_t timestamp_us;     /**< Microsecond timestamp at trigger (from plat_get_us()) */
    SensorReading_t actual;    /**< Actual sensor reading */
    SensorReading_t reference; /**< Reference sensor reading */
} SamplePacket_t;

/* ── TX message: T_CMD/T_PROC → T_TX ──────────────────────────────────── */

#define TX_MSG_MAX_LEN  256   /**< Maximum message length (includes null terminator) */

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
