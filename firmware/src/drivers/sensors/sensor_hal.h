#ifndef __SENSOR_HAL_H
#define __SENSOR_HAL_H

/**
 * @file  sensor_hal.h
 * @brief Uniform magnetic-sensor driver interface (refactor P2).
 *
 * The acquisition layer talks to every sensor through a SensorDriver_t looked up
 * at runtime with Sensor_FindDriver(); it never includes a concrete driver header
 * and contains no per-sensor #if. Adding a sensor = one adapter (xxx_hal.c) plus
 * one entry in sensor_registry.c.
 */

#include <stdint.h>
#include "config_manager.h"   /* Config_TypeDef, SENSOR_TYPE_* */

/* Generic 3-axis float data (µT) plus temperature (°C) — sensor-agnostic output */
typedef struct { float x, y, z, t; } SensorData_t;

typedef enum {
    SENSOR_ROLE_ACTUAL    = 0,   /* measuring sensor  */
    SENSOR_ROLE_REFERENCE = 1,   /* background/reference sensor */
} SensorRole_t;

/* ready() results */
#define SENSOR_READY        1
#define SENSOR_NOT_READY    0
#define SENSOR_READ_ERROR  (-1)  /* status could not be read: abort the wait at once */
#define SENSOR_FAULT       (-2)  /* status read fine but reports a device fault */

typedef struct SensorDriver {
    const char *name;
    uint8_t     type;            /* SENSOR_TYPE_* */
    uint16_t    ctx_size;        /* bytes of driver context (handle) */
    /* Fixed wait after trigger() before ready() is polled. The legacy predictive
     * compensation also uses it as its time step; keep it unchanged until the
     * algorithm is converted to real timestamps (P5/P6). */
    uint16_t    settle_ms;

    /* Find the device for this role on the bus, verify its ID, reset it. */
    uint8_t (*probe)(void *ctx, uint8_t bus, SensorRole_t role);
    /* Write the hardware part of cfg. Called with the sensor idle. */
    uint8_t (*configure)(void *ctx, uint8_t bus, const Config_TypeDef *cfg);
    /* Whether cfg allows host-triggered sampling; NULL = always. */
    uint8_t (*sampling_supported)(const Config_TypeDef *cfg);
    /* Start one conversion (no-op for free-running modes). Non-blocking. */
    uint8_t (*trigger)(void *ctx, uint8_t bus);
    /* Poll conversion status; NULL = data is ready once settle_ms has elapsed. */
    int8_t  (*ready)(void *ctx);
    /* Read the converted sample. */
    uint8_t (*read)(void *ctx, uint8_t bus, SensorData_t *out);
} SensorDriver_t;

/* Registry (sensor_registry.c) */
const SensorDriver_t *Sensor_FindDriver(uint8_t type);

extern const SensorDriver_t g_drv_mlx90393;
extern const SensorDriver_t g_drv_tmag3001;
extern const SensorDriver_t g_drv_ak09974c;

#endif /* __SENSOR_HAL_H */
