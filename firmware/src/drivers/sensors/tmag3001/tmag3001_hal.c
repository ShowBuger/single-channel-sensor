#include "../sensor_hal.h"
#include "tmag3001.h"
#include "../../bsp/bsp_delay.h"
#include "../../bsp/bsp_i2c_soft.h"
#include "../../bsp/log_manager.h"

static uint8_t tmag_init(void *h, uint8_t a1, uint8_t a0, int drdy, uint8_t id)
{
    TMAG3001_Handle_t *dev = (TMAG3001_Handle_t *)h;
    uint8_t expected_addr = dev->i2c_addr;

    /* The caller supplies the hardware address; verify it on the bus before init. */
    if (I2C_DevicePresent(id, expected_addr) != 0) {
        LOG_E("TMAG3001", "I2C address mismatch: expected 0x%02X", expected_addr);
        return 1;
    }

    (void)a1; (void)a0; (void)drdy;
    return TMAG3001_Init(dev, id, expected_addr, TMAG3001_VERSION_A1);
}

static uint8_t tmag_configure(void *h, uint8_t id, const SensorHwConfig_t *cfg)
{
    TMAG3001_Handle_t *dev = (TMAG3001_Handle_t *)h;
    (void)id;
    uint8_t result = TMAG3001_SetConvAvg(dev, (TMAG3001_ConvAvg_t)cfg->osr);
    return result ? result : TMAG3001_SetMagChannels(dev, TMAG3001_CH_XYZ);
}

static uint8_t tmag_start(void *h, uint8_t id)
{
    TMAG3001_Handle_t *dev = (TMAG3001_Handle_t *)h;
    (void)id;
    if (dev->op_mode == TMAG3001_MODE_CONTINUOUS) return 0;
    if (dev->op_mode != TMAG3001_MODE_STANDBY) return 1;
    uint8_t status;
    if (TMAG3001_ReadConvStatus(dev, &status)) return 1;
    dev->last_set_count = status & TMAG3001_STATUS_SET_COUNT;
    dev->sample_seen = 1;
    return TMAG3001_TriggerConversion(dev);
}

static uint8_t tmag_read(void *h, uint8_t id, SensorData_t *out)
{
    TMAG3001_Result_t r;
    (void)id;
    uint8_t status = TMAG3001_ReadMagneticData((TMAG3001_Handle_t *)h, &r);
    if (status) return status;
    out->x = r.x_mT * 1000.0f;
    out->y = r.y_mT * 1000.0f;
    out->z = r.z_mT * 1000.0f;
    out->t = 25.0f;
    return 0;
}

static SensorData_t tmag_convert(void *raw)
{
    /* TMAG3001 raw is TMAG3001_Result_t */
    TMAG3001_Result_t *r = (TMAG3001_Result_t *)raw;
    SensorData_t out;
    out.x = r->x_mT * 1000.0f;
    out.y = r->y_mT * 1000.0f;
    out.z = r->z_mT * 1000.0f;
    out.t = 25.0f;
    return out;
}

const SensorHAL_t g_hal_tmag3001 = {
    .name             = "TMAG3001",
    .init             = tmag_init,
    .configure        = tmag_configure,
    .start_meas       = tmag_start,
    .read_meas        = tmag_read,
    .convert_raw      = tmag_convert,
    .meas_interval_ms = 2,
    .handle_size      = sizeof(TMAG3001_Handle_t),
};
