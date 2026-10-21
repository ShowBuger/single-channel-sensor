#include "../sensor_hal.h"
#include "mlx90393.h"

static uint8_t mlx_init(void *h, uint8_t a1, uint8_t a0, int drdy, uint8_t id)
{
    return MLX90393_begin((MLX90393_Handle *)h, a1, a0, drdy, id);
}

static uint8_t mlx_configure(void *h, uint8_t id, const SensorHwConfig_t *cfg)
{
    MLX90393_Config mc;
    mc.gain_sel   = cfg->gain;
    mc.res_x      = cfg->res_x;
    mc.res_y      = cfg->res_y;
    mc.res_z      = cfg->res_z;
    mc.osr        = cfg->osr;
    mc.dig_filt   = cfg->dig_filt;
    mc.tcmp_en    = cfg->tcmp_en;
    mc.burst_rate = 0;
    return MLX90393_configureAll((MLX90393_Handle *)h, id, &mc);
}

#define XYZT (MLX90393_FLAG_X|MLX90393_FLAG_Y|MLX90393_FLAG_Z)

static uint8_t mlx_start(void *h, uint8_t id)
{
    uint8_t status = MLX90393_startMeasurement((MLX90393_Handle *)h, id, XYZT);
    /* startMeasurement 返回原始状态字节：bit5(SM_MODE)=单次测量模式属正常，
     * 只有 bit4(ERROR) 才代表失败。与 mlx_read 一致，用 error_process 归一化。 */
    return error_process(status);
}

static uint8_t mlx_read(void *h, uint8_t id, SensorData_t *out)
{
    MLX90393_RawData raw;
    uint8_t st = MLX90393_readMeasurement((MLX90393_Handle *)h, id, XYZT, &raw);
    if (error_process(st) != MLX90393_ERROR_NONE) return 1;
    MLX90393_Data d = MLX90393_convertRaw(raw);
    out->x = d.x; out->y = d.y; out->z = d.z; out->t = d.t;
    return 0;
}

static SensorData_t mlx_convert(void *raw)
{
    MLX90393_Data d = MLX90393_convertRaw(*(MLX90393_RawData *)raw);
    SensorData_t out = { d.x, d.y, d.z, d.t };
    return out;
}

const SensorHAL_t g_hal_mlx90393 = {
    .name            = "MLX90393",
    .init            = mlx_init,
    .configure       = mlx_configure,
    .start_meas      = mlx_start,
    .read_meas       = mlx_read,
    .convert_raw     = mlx_convert,
    .meas_interval_ms = 8,
    .handle_size     = sizeof(MLX90393_Handle),
};
