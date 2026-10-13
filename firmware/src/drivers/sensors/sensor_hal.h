#ifndef __SENSOR_HAL_H
#define __SENSOR_HAL_H

#include <stdint.h>

/* Generic 3-axis float data — sensor-agnostic output */
typedef struct { float x, y, z, t; } SensorData_t;

/* Hardware config pushed down to the sensor driver */
typedef struct {
    uint8_t gain;       /* device-specific gain index     */
    uint8_t osr;        /* oversampling ratio index 0-3   */
    uint8_t dig_filt;   /* digital filter index 0-7       */
    uint8_t res_x, res_y, res_z; /* resolution per axis 0-3 */
    uint8_t tcmp_en;    /* temperature compensation 0/1   */
} SensorHwConfig_t;

/* HAL vtable — one instance per sensor type */
typedef struct {
    const char *name;   /* e.g. "MLX90393" or "TMAG3001"  */
    /* Initialize sensor at I2C address encoded in addr_bits (sensor-specific) */
    uint8_t (*init)(void *handle, uint8_t a1, uint8_t a0, int drdy_pin, uint8_t i2c_id);
    /* Apply hardware config (gain/osr/filter/resolution) */
    uint8_t (*configure)(void *handle, uint8_t i2c_id, const SensorHwConfig_t *cfg);
    /* Start a single measurement (non-blocking) */
    uint8_t (*start_meas)(void *handle, uint8_t i2c_id);
    /* Read completed measurement into *out; returns 0 on success */
    uint8_t (*read_meas)(void *handle, uint8_t i2c_id, SensorData_t *out);
    /* Convert raw counts to physical units (µT / °C) */
    SensorData_t (*convert_raw)(void *raw);  /* raw is sensor-specific struct pointer */
    /* Measurement interval hint (ms) after start_meas */
    uint16_t meas_interval_ms;
    /* Handle memory size — caller must allocate this many bytes for the handle */
    uint16_t handle_size;
} SensorHAL_t;

/* Implemented drivers */
extern const SensorHAL_t g_hal_mlx90393;
extern const SensorHAL_t g_hal_tmag3001;

#endif /* __SENSOR_HAL_H */
