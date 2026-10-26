#include "../sensor_hal.h"
#include "ak09974c.h"
#include "board_config.h"
#include "../../bsp/bsp_i2c_soft.h"
#include "../../bsp/log_manager.h"

static uint8_t ak_init(void *h, uint8_t a1, uint8_t a0, int drdy, uint8_t id)
{
    AK09974C_Handle_t *dev = (AK09974C_Handle_t *)h;
    uint8_t expected_addr = dev->i2c_addr;

    /* The caller supplies the hardware address; verify it on the bus before init. */
    if (I2C_DevicePresent(id, expected_addr) != 0) {
        LOG_E("AK09974C", "I2C address mismatch: expected 0x%02X", expected_addr);
        return 1;
    }

    (void)a1; (void)a0; (void)drdy;
    return AK09974C_Init(dev, id, expected_addr, BOARD_AK09974C_VIO);
}

static uint8_t ak_configure(void *h, uint8_t id, const SensorHwConfig_t *cfg)
{
    AK09974C_Handle_t *dev = (AK09974C_Handle_t *)h;
    (void)id;
    /* osr 字段复用为驱动模式(DRI)，gain 字段对AK09974C无意义。 */
    uint8_t result = AK09974C_Stop(dev);
    return result ? result : AK09974C_SetDrive(dev, (AK09974C_Drive_t)(cfg->osr & 0x03));
}

static uint8_t ak_start(void *h, uint8_t id)
{
    AK09974C_Handle_t *dev = (AK09974C_Handle_t *)h;
    (void)id;
    /* 连续模式已在运行，单次模式每次写RUN=1启动一次测量。 */
    if (dev->running) return 0;
    return AK09974C_Start(dev);
}

static uint8_t ak_read(void *h, uint8_t id, SensorData_t *out)
{
    AK09974C_Result_t r;
    (void)id;
    uint8_t status = AK09974C_ReadMagneticData((AK09974C_Handle_t *)h, &r);
    if (status) return status;
    out->x = r.x_uT;
    out->y = r.y_uT;
    out->z = r.z_uT;
    out->t = 25.0f;  /* AK09974C无温度输出 */
    return 0;
}

static SensorData_t ak_convert(void *raw)
{
    /* AK09974C raw is AK09974C_Result_t */
    AK09974C_Result_t *r = (AK09974C_Result_t *)raw;
    SensorData_t out;
    out.x = r->x_uT;
    out.y = r->y_uT;
    out.z = r->z_uT;
    out.t = 25.0f;
    return out;
}

const SensorHAL_t g_hal_ak09974c = {
    .name             = "AK09974C",
    .init             = ak_init,
    .configure        = ak_configure,
    .start_meas       = ak_start,
    .read_meas        = ak_read,
    .convert_raw      = ak_convert,
    .meas_interval_ms = 3,
    .handle_size      = sizeof(AK09974C_Handle_t),
};
