/* MLX90393 adapter for the uniform sensor driver interface. */
#include "../sensor_hal.h"
#include "mlx90393.h"
#include "../../bsp/log_manager.h"

#define XYZT (MLX90393_FLAG_X | MLX90393_FLAG_Y | MLX90393_FLAG_Z)

static uint8_t mlx_probe(void *ctx, uint8_t bus, SensorRole_t role)
{
    /* Address pins: A1 = 0; A0 = 0 for the actual sensor, 1 for the reference. No DRDY. */
    return MLX90393_begin((MLX90393_Handle *)ctx, 0, (role == SENSOR_ROLE_ACTUAL) ? 0 : 1, -1, bus);
}

static uint8_t mlx_configure(void *ctx, uint8_t bus, const Config_TypeDef *cfg)
{
    MLX90393_Config mc;
    uint8_t result;

    LOG_D("CFG", "Applying MLX90393 config: gain=%u osr=%u filt=%u res=%u/%u/%u tcmp=%u",
          cfg->gain_sel, cfg->osr, cfg->dig_filt, cfg->res_x, cfg->res_y, cfg->res_z, cfg->tcmp_en);

    mc.gain_sel   = (cfg->gain_sel <= 7) ? cfg->gain_sel : CONFIG_DEFAULT_MLX_GAIN_SEL;
    mc.osr        = (cfg->osr <= 3)      ? cfg->osr      : CONFIG_DEFAULT_MLX_OSR;
    mc.dig_filt   = (cfg->dig_filt <= 7) ? cfg->dig_filt : CONFIG_DEFAULT_MLX_DIG_FILT;
    mc.res_x      = (cfg->res_x <= 3)    ? cfg->res_x    : CONFIG_DEFAULT_MLX_RES;
    mc.res_y      = (cfg->res_y <= 3)    ? cfg->res_y    : CONFIG_DEFAULT_MLX_RES;
    mc.res_z      = (cfg->res_z <= 3)    ? cfg->res_z    : CONFIG_DEFAULT_MLX_RES;
    mc.tcmp_en    = (cfg->tcmp_en <= 1)  ? cfg->tcmp_en  : CONFIG_DEFAULT_MLX_TCMP_EN;
    mc.burst_rate = 0;   /* no burst mode */

    result = MLX90393_configureAll((MLX90393_Handle *)ctx, bus, &mc);
    if (error_process(result) != MLX90393_ERROR_NONE) {
        LOG_E("CFG", "MLX90393 config apply failed");
        return 1;
    }
    return 0;   /* MLX returns a status byte; normalise to success/failure */
}

static uint8_t mlx_trigger(void *ctx, uint8_t bus)
{
    uint8_t status = MLX90393_startMeasurement((MLX90393_Handle *)ctx, bus, XYZT);
    /* Raw status: bit5 (SM_MODE) is normal in single-measurement mode, only bit4
     * (ERROR) means failure — normalise like mlx_read. */
    return error_process(status);
}

static uint8_t mlx_read(void *ctx, uint8_t bus, SensorData_t *out)
{
    MLX90393_RawData raw;
    uint8_t st = MLX90393_readMeasurement((MLX90393_Handle *)ctx, bus, XYZT, &raw);
    if (error_process(st) != MLX90393_ERROR_NONE) return 1;
    MLX90393_Data d = MLX90393_convertRaw(raw);
    out->x = d.x; out->y = d.y; out->z = d.z; out->t = d.t;
    return 0;
}

const SensorDriver_t g_drv_mlx90393 = {
    .name               = "MLX90393",
    .type               = SENSOR_TYPE_MLX90393,
    .ctx_size           = sizeof(MLX90393_Handle),
    .settle_ms          = 20,     /* OSR=2 (x4), 3 axes ~15 ms, with margin; no status polling */
    .probe              = mlx_probe,
    .configure          = mlx_configure,
    .sampling_supported = NULL,
    .trigger            = mlx_trigger,
    .ready              = NULL,
    .read               = mlx_read,
};
