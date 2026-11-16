#ifndef SENSOR_REGISTRY_H
#define SENSOR_REGISTRY_H

/**
 * @file  sensor_registry.h
 * @brief Storage able to hold the context of any registered sensor driver.
 *
 * The acquisition layer allocates SensorCtx_t without knowing which driver will
 * use it. Only this header lists the concrete driver handle types.
 */

#include "sensor_hal.h"
#include "mlx90393/mlx90393.h"
#include "tmag3001/tmag3001.h"
#include "ak09974c/ak09974c.h"

typedef union {
    MLX90393_Handle   mlx90393;
    TMAG3001_Handle_t tmag3001;
    AK09974C_Handle_t ak09974c;
} SensorCtx_t;

#endif /* SENSOR_REGISTRY_H */
