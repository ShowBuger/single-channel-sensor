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
    /* 在 CONTINUOUS 模式下，传感器会自动连续转换，不需要手动触发 */
    (void)h;
    (void)id;
    return 0;
}

static uint8_t tmag_read(void *h, uint8_t id, SensorData_t *out)
{
    TMAG3001_Handle_t *dev = (TMAG3001_Handle_t *)h;
    TMAG3001_Result_t r;
    (void)id;

    /* 测试代码：读取寄存器进行诊断 */
    static uint8_t test_counter = 0;
    if (test_counter < 2) {  /* 前两次读取时输出诊断信息 */
        uint8_t device_id;
        uint16_t manufacturer_id;

        LOG_D("TMAG_TEST", "=== Register Diagnostic #%d (addr=0x%02X) ===", test_counter + 1, dev->i2c_addr);

        if (TMAG3001_ReadID(dev, &device_id, &manufacturer_id) == 0) {
            LOG_D("TMAG_TEST", "DEVICE_ID = 0x%02X (expect: 0x01)", device_id);
            LOG_D("TMAG_TEST", "MANUFACTURER_ID = 0x%04X (expect: 0x5449 'TI')", manufacturer_id);
            if (device_id == 0x08) {
                LOG_I("TMAG_TEST", "Device ID 0x08 detected - TMAG3001 variant confirmed");
            }
        } else {
            LOG_E("TMAG_TEST", "Failed to read IDs - I2C communication error!");
        }

        /* 读取CONV_STATUS寄存器分析详细状态 */
        uint8_t conv_status;
        if (TMAG3001_ReadConvStatus(dev, &conv_status) == 0) {
            LOG_D("TMAG_TEST", "CONV_STATUS = 0x%02X (binary: %d%d%d%d %d%d%d%d)",
                  conv_status,
                  (conv_status >> 7) & 1, (conv_status >> 6) & 1,
                  (conv_status >> 5) & 1, (conv_status >> 4) & 1,
                  (conv_status >> 3) & 1, (conv_status >> 2) & 1,
                  (conv_status >> 1) & 1, (conv_status >> 0) & 1);
            LOG_D("TMAG_TEST", "  Bit[0] DATA_READY = %d", conv_status & 0x01);
            LOG_D("TMAG_TEST", "  Bit[7] DIAG_STATUS = %d", (conv_status >> 7) & 0x01);
        }

        LOG_D("TMAG_TEST", "=== Diagnostic End ===");
        test_counter++;
    }

    /* 在 CONTINUOUS 模式下，传感器会自动连续转换，直接读取最新数据即可 */
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
