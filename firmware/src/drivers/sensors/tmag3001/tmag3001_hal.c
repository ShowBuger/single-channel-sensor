/* TMAG3001 adapter for the uniform sensor driver interface. */
#include "../sensor_hal.h"
#include "tmag3001.h"
#include "../../bsp/bsp_delay.h"
#include "../../bsp/bsp_i2c_soft.h"
#include "../../bsp/log_manager.h"

static uint8_t tmag_probe(void *ctx, uint8_t bus, SensorRole_t role)
{
    TMAG3001_Handle_t *dev = (TMAG3001_Handle_t *)ctx;
    /* ADDR pin: actual sensor tied to SDA, reference sensor tied to VCC. */
    uint8_t addr = (role == SENSOR_ROLE_ACTUAL) ? TMAG3001_I2C_ADDR_SDA : TMAG3001_I2C_ADDR_VCC;

    dev->i2c_addr = addr;
    if (I2C_DevicePresent(bus, addr) != 0) {
        LOG_E("TMAG3001", "I2C address mismatch: expected 0x%02X", addr);
        return 1;
    }
    return TMAG3001_Init(dev, bus, addr, TMAG3001_VERSION_A1);
}

static uint8_t tmag_configure(void *ctx, uint8_t bus, const Config_TypeDef *cfg)
{
    TMAG3001_Handle_t *dev = (TMAG3001_Handle_t *)ctx;
    uint8_t result = 0;
    (void)bus;

    LOG_D("CFG", "Applying TMAG3001 config: gain=%u osr=%u range=%u mode=%u ch=%u",
          cfg->gain_sel, cfg->osr, cfg->range, cfg->op_mode, cfg->mag_ch);

    if (cfg->gain_sel <= 15) {
        result |= TMAG3001_SetMagGain(dev, (TMAG3001_MagGain_t)cfg->gain_sel);
    } else {
        LOG_W("CFG", "Invalid gain %u, using default", cfg->gain_sel);
        result |= TMAG3001_SetMagGain(dev, TMAG3001_GAIN_1_0);
    }
    if (cfg->osr <= 5) {
        result |= TMAG3001_SetConvAvg(dev, (TMAG3001_ConvAvg_t)cfg->osr);
    } else {
        LOG_W("CFG", "Invalid osr %u, using default", cfg->osr);
        result |= TMAG3001_SetConvAvg(dev, TMAG3001_AVG_4X);
    }
    if (cfg->range <= 1) {
        result |= TMAG3001_SetRange(dev, (TMAG3001_Range_t)cfg->range);
    } else {
        LOG_W("CFG", "Invalid range %u, using default", cfg->range);
        result |= TMAG3001_SetRange(dev, TMAG3001_RANGE_40mT);
    }
    if (cfg->mag_ch <= 7) {
        result |= TMAG3001_SetMagChannels(dev, (TMAG3001_MagChannel_t)cfg->mag_ch);
    } else {
        LOG_W("CFG", "Invalid mag_ch %u, using default", cfg->mag_ch);
        result |= TMAG3001_SetMagChannels(dev, TMAG3001_CH_XYZ);
    }
    /* Clear POR and diagnostic flags. */
    result |= TMAG3001_ClearStatusFlags(dev);
    /* Operating mode last, so an early sleep cannot block the writes above. */
    result |= TMAG3001_SetOperatingMode(dev, (TMAG3001_OperatingMode_t)cfg->op_mode);
    return result;
}

static uint8_t tmag_sampling_supported(const Config_TypeDef *cfg)
{
    return cfg->op_mode == TMAG3001_MODE_STANDBY || cfg->op_mode == TMAG3001_MODE_CONTINUOUS;
}

static uint8_t tmag_trigger(void *ctx, uint8_t bus)
{
    TMAG3001_Handle_t *dev = (TMAG3001_Handle_t *)ctx;
    uint8_t status;
    (void)bus;
    if (dev->op_mode == TMAG3001_MODE_CONTINUOUS) return 0;
    if (dev->op_mode != TMAG3001_MODE_STANDBY) return 1;
    if (TMAG3001_ReadConvStatus(dev, &status)) return 1;
    dev->last_set_count = status & TMAG3001_STATUS_SET_COUNT;
    dev->sample_seen = 1;
    return TMAG3001_TriggerConversion(dev);
}

static int8_t tmag_ready(void *ctx)
{
    TMAG3001_Handle_t *dev = (TMAG3001_Handle_t *)ctx;
    uint8_t st, fresh;
    if (TMAG3001_ReadConvStatus(dev, &st)) return SENSOR_READ_ERROR;
    if (st & (TMAG3001_STATUS_DIAG | TMAG3001_STATUS_POR)) return SENSOR_FAULT;
    /* A new result set bumps SET_COUNT; until a first sample was seen any READY counts. */
    fresh = !dev->sample_seen || (st & TMAG3001_STATUS_SET_COUNT) != dev->last_set_count;
    return ((st & TMAG3001_STATUS_READY) && fresh) ? SENSOR_READY : SENSOR_NOT_READY;
}

static uint8_t tmag_read(void *ctx, uint8_t bus, SensorData_t *out)
{
    TMAG3001_Result_t r;
    (void)bus;
    uint8_t status = TMAG3001_ReadMagneticData((TMAG3001_Handle_t *)ctx, &r);
    if (status) return status;
    out->x = r.x_mT * 1000.0f;
    out->y = r.y_mT * 1000.0f;
    out->z = r.z_mT * 1000.0f;
    out->t = 25.0f;
    return 0;
}

const SensorDriver_t g_drv_tmag3001 = {
    .name               = "TMAG3001",
    .type               = SENSOR_TYPE_TMAG3001,
    .ctx_size           = sizeof(TMAG3001_Handle_t),
    .settle_ms          = 2,      /* typical conversion ~2 ms */
    .probe              = tmag_probe,
    .configure          = tmag_configure,
    .sampling_supported = tmag_sampling_supported,
    .trigger            = tmag_trigger,
    .ready              = tmag_ready,
    .read               = tmag_read,
};
