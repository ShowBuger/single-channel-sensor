#ifndef __ALGO_PARAMS_H
#define __ALGO_PARAMS_H

#include <stdint.h>
#include "config_manager.h"

/**
 * @brief Runtime algorithm parameters.
 *
 * Initialized from Config_TypeDef at startup; updated by ML SET commands.
 * SLIDING_WINDOW_SIZE stays compile-time (array dimension), but all other
 * tunable values move here from hardcoded macros.
 */

#define SLIDING_WINDOW_SIZE        5   /* compile-time array dimension, not tunable */
#define STABILITY_CHECK_SAMPLES   10   /* compile-time loop bound */

typedef struct {
    float dead_value;             /* dead-band multiplier (x-sigma)            */
    float spike_multiplier;       /* spike filter multiplier (x-sigma)         */
    float stability_threshold;    /* stability threshold uT               */
} AlgoParams_t;

/* Global instance -- initialised in sensor_manager via Config_Init_All */
extern AlgoParams_t g_algo;

/* Apply config values to g_algo */
void AlgoParams_Load(const Config_TypeDef *cfg);

#endif /* __ALGO_PARAMS_H */
