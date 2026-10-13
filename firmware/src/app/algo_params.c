#include "algo_params.h"

AlgoParams_t g_algo = {
    .dead_value          = CONFIG_DEFAULT_DEAD_VALUE,
    .spike_multiplier    = CONFIG_DEFAULT_SPIKE_MULT,
    .stability_threshold = CONFIG_DEFAULT_STABILITY_THR,
};

void AlgoParams_Load(const Config_TypeDef *cfg)
{
    g_algo.dead_value          = cfg->dead_value;
    g_algo.spike_multiplier    = cfg->spike_multiplier;
    g_algo.stability_threshold = cfg->stability_threshold;
}
