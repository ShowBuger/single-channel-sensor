/* AK09974C adapter for the uniform sensor driver interface. */
#include "../sensor_hal.h"
#include "ak09974c.h"
#include "board.h"
#include "../../bsp/bsp_i2c_soft.h"
#include "../../bsp/log_manager.h"

/* Boards with an AK09974C define VIO and the two addresses in board.h. On other
 * boards this driver is compiled but never selected (and dropped by
 * --gc-sections), so the defaults below only keep it building. */
#ifndef BOARD_AK09974C_VIO
#define BOARD_AK09974C_VIO         AK09974C_VIO_1V8
#endif
#ifndef BOARD_AK09974C_ACTUAL_ADDR
#define BOARD_AK09974C_ACTUAL_ADDR AK09974C_I2C_ADDR_CASE1
#endif
#ifndef BOARD_AK09974C_REF_ADDR
#define BOARD_AK09974C_REF_ADDR    AK09974C_I2C_ADDR_CASE2
#endif

static uint8_t ak_probe(void *ctx, uint8_t bus, SensorRole_t role)
{
    AK09974C_Handle_t *dev = (AK09974C_Handle_t *)ctx;
    uint8_t addr = (role == SENSOR_ROLE_ACTUAL) ? BOARD_AK09974C_ACTUAL_ADDR : BOARD_AK09974C_REF_ADDR;

    dev->i2c_addr = addr;
    /* Verify the wiring-selected address before init. */
    if (I2C_DevicePresent(bus, addr) != 0) {
        LOG_E("AK09974C", "I2C address mismatch: expected 0x%02X", addr);
        return 1;
    }
    return AK09974C_Init(dev, bus, addr, BOARD_AK09974C_VIO);
}

static uint8_t ak_configure(void *ctx, uint8_t bus, const Config_TypeDef *cfg)
{
    AK09974C_Handle_t *dev = (AK09974C_Handle_t *)ctx;
    uint8_t result = 0;
    (void)bus;

    LOG_D("CFG", "Applying AK09974C config: drive=%u range=%u mode=%u mcyc=%u",
          cfg->osr, cfg->range, cfg->op_mode, cfg->mcyc);

    /* Settings are writable only in power-down; clear the cycle first so a drive
     * change does not conflict with the previous cycle. */
    result |= AK09974C_Stop(dev);
    result |= AK09974C_SetMeasureCycle(dev, AK09974C_MCYC_SINGLE);
    result |= AK09974C_SetRange(dev, (AK09974C_Range_t)cfg->range);
    result |= AK09974C_SetDrive(dev, (AK09974C_Drive_t)cfg->osr);

    /* Continuous mode: write the cycle last and start. Single mode: each sample writes RUN. */
    if (cfg->op_mode == CONFIG_AK_OPMODE_CONTINUOUS) {
        result |= AK09974C_SetMeasureCycle(dev, cfg->mcyc);
        if (result == 0) result = AK09974C_Start(dev);
    }
    return result;
}

static uint8_t ak_trigger(void *ctx, uint8_t bus)
{
    AK09974C_Handle_t *dev = (AK09974C_Handle_t *)ctx;
    (void)bus;
    /* Continuous mode is already running; single mode writes RUN=1 per sample. */
    if (dev->running) return 0;
    return AK09974C_Start(dev);
}

static int8_t ak_ready(void *ctx)
{
    uint8_t st;
    /* Reading ST(10h) alone does not clear DRDY; DRDY=1 means an unread sample set. */
    if (AK09974C_ReadStatus((AK09974C_Handle_t *)ctx, &st)) return SENSOR_READ_ERROR;
    if (!(st & AK09974C_ST_DRDY)) return SENSOR_NOT_READY;
    if (st & AK09974C_ST_ERR) LOG_D("SCAN", "Magnetic overflow: status=0x%02X", st);
    return SENSOR_READY;
}

static uint8_t ak_read(void *ctx, uint8_t bus, SensorData_t *out)
{
    AK09974C_Result_t r;
    (void)bus;
    uint8_t status = AK09974C_ReadMagneticData((AK09974C_Handle_t *)ctx, &r);
    if (status) return status;
    out->x = r.x_uT;
    out->y = r.y_uT;
    out->z = r.z_uT;
    out->t = 25.0f;  /* no temperature output */
    return 0;
}

const SensorDriver_t g_drv_ak09974c = {
    .name               = "AK09974C",
    .type               = SENSOR_TYPE_AK09974C,
    .ctx_size           = sizeof(AK09974C_Handle_t),
    .settle_ms          = 3,      /* low-noise drive single measurement ~2.9 ms */
    .probe              = ak_probe,
    .configure          = ak_configure,
    .sampling_supported = NULL,
    .trigger            = ak_trigger,
    .ready              = ak_ready,
    .read               = ak_read,
};
