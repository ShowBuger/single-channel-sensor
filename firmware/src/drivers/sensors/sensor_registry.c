#include "sensor_registry.h"
#include <stddef.h>

/* Drivers linked into this image. The build enables SENSOR_DRV_<NAME> for each
 * driver the board lists (CMake BOARD_SENSOR_DRIVERS), so unused drivers are
 * neither compiled nor linked. Add new sensors here and to the board list. */
static const SensorDriver_t *const s_drivers[] = {
#ifdef SENSOR_DRV_MLX90393
    &g_drv_mlx90393,
#endif
#ifdef SENSOR_DRV_TMAG3001
    &g_drv_tmag3001,
#endif
#ifdef SENSOR_DRV_AK09974C
    &g_drv_ak09974c,
#endif
    NULL
};

const SensorDriver_t *Sensor_FindDriver(uint8_t type)
{
    size_t i;
    for (i = 0; s_drivers[i] != NULL; i++) {
        if (s_drivers[i]->type == type && s_drivers[i]->ctx_size <= sizeof(SensorCtx_t))
            return s_drivers[i];
    }
    return NULL;
}
