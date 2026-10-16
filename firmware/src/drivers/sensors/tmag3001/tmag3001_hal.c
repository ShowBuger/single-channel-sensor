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
    TMAG3001_SetConvAvg(dev, (TMAG3001_ConvAvg_t)cfg->osr);
    TMAG3001_SetMagChannels(dev, TMAG3001_CH_XYZ);
    return 0;
}

static uint8_t tmag_start(void *h, uint8_t id)
{
    (void)id;
    return TMAG3001_TriggerConversion((TMAG3001_Handle_t *)h);
}

static uint8_t tmag_read(void *h, uint8_t id, SensorData_t *out)
{
    TMAG3001_Handle_t *dev = (TMAG3001_Handle_t *)h;
    TMAG3001_Result_t r;
    uint8_t status;
    uint8_t retry = 0;
    (void)id;

    /* 等待转换完成，最多重试 20 次（约 10ms） */
    while (retry < 20) {
        if (TMAG3001_ReadConvStatus(dev, &status) == 0) {
            /* 检查 DATA_READY 标志（bit 0） */
            if (status & 0x01) {
                break;  /* 转换完成 */
            }
        }
        /* 延时 0.5ms */
        for (volatile int i = 0; i < 5000; i++);
        retry++;
    }

    if (retry >= 20) {
        return 1;  /* 超时，转换未完成 */
    }

    uint8_t st = TMAG3001_ReadMagneticData(dev, &r);
    if (st != 0) return 1;
    out->x = r.x_mT * 1000.0f; /* mT -> uT */
    out->y = r.y_mT * 1000.0f;
    out->z = r.z_mT * 1000.0f;
    out->t = 25.0f; /* TMAG3001 temperature read not wired here yet */
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
