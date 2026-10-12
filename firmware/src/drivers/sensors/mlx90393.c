#include "mlx90393.h"
#include "../bsp/bsp_i2c_soft.h"
#include "../bsp/bsp_delay.h"

/* All hardware I2C calls replaced with soft I2C.
 * i2c_id parameter maps to the iic[] index in bsp_i2c_soft.c (always 0 here). */

uint8_t error_process(uint8_t status)
{
    if (status & MLX90393_STATUS_ERROR)
        return MLX90393_ERROR_BIT_SET;
    return MLX90393_ERROR_NONE;
}

uint8_t MLX90393_getDataByteCount(uint8_t status)
{
    uint8_t d = status & (MLX90393_STATUS_D0 | MLX90393_STATUS_D1);
    return (1 << d) + 2;
}

uint8_t MLX90393_isBurstMode(uint8_t status)  { return (status & MLX90393_STATUS_BURST_MODE) ? 1 : 0; }
uint8_t MLX90393_isWOCMode(uint8_t status)    { return (status & MLX90393_STATUS_WOC_MODE)   ? 1 : 0; }
uint8_t MLX90393_isSingleMode(uint8_t status) { return (status & MLX90393_STATUS_SM_MODE)    ? 1 : 0; }

/* -----------------------------------------------------------------------
 * Low-level helpers using soft I2C
 * ----------------------------------------------------------------------- */

/**
 * Write `len` bytes from `buf` to device address `dev_addr` (7-bit).
 * Returns 0 on success.
 */
static uint8_t i2c_write(uint8_t i2c_id, uint8_t dev_addr, const uint8_t *buf, uint8_t len)
{
    uint8_t i;
    IIC_Start(i2c_id);
    I2C_WriteByte(i2c_id, dev_addr << 1);          /* write direction */
    if (I2C_WaitAck(i2c_id)) { IIC_Stop(i2c_id); return 1; }
    for (i = 0; i < len; i++)
    {
        I2C_WriteByte(i2c_id, buf[i]);
        if (I2C_WaitAck(i2c_id)) { IIC_Stop(i2c_id); return 1; }
    }
    IIC_Stop(i2c_id);
    return 0;
}

/**
 * Read `len` bytes into `buf` from device address `dev_addr` (7-bit).
 * Returns 0 on success.
 */
static uint8_t i2c_read(uint8_t i2c_id, uint8_t dev_addr, uint8_t *buf, uint8_t len)
{
    uint8_t i;
    IIC_Start(i2c_id);
    I2C_WriteByte(i2c_id, (dev_addr << 1) | 0x01); /* read direction */
    if (I2C_WaitAck(i2c_id)) { IIC_Stop(i2c_id); return 1; }
    for (i = 0; i < len; i++)
    {
        buf[i] = I2C_ReadByte(i2c_id);
        I2C_SendAck(i2c_id, (i == len - 1) ? 1 : 0); /* NACK on last byte */
    }
    IIC_Stop(i2c_id);
    return 0;
}

/* -----------------------------------------------------------------------
 * MLX90393 commands
 * ----------------------------------------------------------------------- */

uint8_t MLX90393_sendCommand(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t cmd)
{
    uint8_t resp = 0xFF;
    if (i2c_write(i2c_id, handle->I2C_Address, &cmd, 1) != 0) return 0xFF;
    if (i2c_read(i2c_id,  handle->I2C_Address, &resp, 1) != 0) return 0xFF;
    return resp;
}

uint8_t MLX90393_readRegister(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t reg, uint16_t *data)
{
    uint8_t tx[2] = { MLX90393_CMD_READ_REGISTER, (uint8_t)((reg & 0x3F) << 2) };
    uint8_t rx[3];
    if (i2c_write(i2c_id, handle->I2C_Address, tx, 2) != 0) return 0xFF;
    if (i2c_read(i2c_id,  handle->I2C_Address, rx, 3) != 0) return 0xFF;
    *data = (uint16_t)((rx[1] << 8) | rx[2]);
    return rx[0];
}

uint8_t MLX90393_writeRegister(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t reg, uint16_t data)
{
    uint8_t tx[4] = {
        MLX90393_CMD_WRITE_REGISTER,
        (uint8_t)(data >> 8),
        (uint8_t)(data & 0xFF),
        (uint8_t)((reg & 0x3F) << 2)
    };
    uint8_t resp = 0xFF;
    if (i2c_write(i2c_id, handle->I2C_Address, tx, 4) != 0) return 0xFF;
    if (i2c_read(i2c_id,  handle->I2C_Address, &resp, 1) != 0) return 0xFF;
    return resp;
}

uint8_t MLX90393_reset(MLX90393_Handle *handle, uint8_t i2c_id)
{
    return MLX90393_sendCommand(handle, i2c_id, MLX90393_CMD_RESET);
}

uint8_t MLX90393_exit_mode(MLX90393_Handle *handle, uint8_t i2c_id)
{
    return MLX90393_sendCommand(handle, i2c_id, MLX90393_CMD_EXIT);
}

uint8_t MLX90393_begin(MLX90393_Handle *handle, uint8_t A1, uint8_t A0, int DRDY_pin, uint8_t id)
{
    uint8_t status;
    handle->I2C_Address = MLX90393_I2C_BASE_ADDR | ((A1 ? 2 : 0) | (A0 ? 1 : 0));
    handle->DRDY_pin    = DRDY_pin;

    status = MLX90393_exit_mode(handle, id);
    if (error_process(status) == MLX90393_ERROR_BIT_SET) return 1;
    Delay_ms(1);

    status = MLX90393_reset(handle, id);
    if (error_process(status) == MLX90393_ERROR_BIT_SET) return 1;
    Delay_ms(2);

    status = MLX90393_setOverSampling(handle, id, 3);
    if (error_process(status) == MLX90393_ERROR_BIT_SET) return 1;
    Delay_ms(1);

    status = MLX90393_setDigitalFiltering(handle, id, 0);
    if (error_process(status) == MLX90393_ERROR_BIT_SET) return 1;
    Delay_ms(1);

    return 0;
}

int8_t MLX90393_selfTest(MLX90393_Handle *handle, uint8_t i2c_id)
{
    uint8_t status;
    uint16_t reg_value;
    MLX90393_RawData baseline, bist_data;
    int32_t z_diff;

    status = MLX90393_startMeasurement(handle, i2c_id, MLX90393_FLAG_Z);
    if (error_process(status) == MLX90393_ERROR_BIT_SET) return -1;
    Delay_ms(10);
    status = MLX90393_readMeasurement(handle, i2c_id, MLX90393_FLAG_Z, &baseline);
    if (error_process(status) == MLX90393_ERROR_BIT_SET) return -1;

    status = MLX90393_readRegister(handle, i2c_id, 0x00, &reg_value);
    if (error_process(status) == MLX90393_ERROR_BIT_SET) return -1;

    reg_value |= (1 << BIST_SHIFT);
    status = MLX90393_writeRegister(handle, i2c_id, 0x00, reg_value);
    if (error_process(status) == MLX90393_ERROR_BIT_SET) return -1;

    status = MLX90393_startMeasurement(handle, i2c_id, MLX90393_FLAG_Z);
    if (error_process(status) == MLX90393_ERROR_BIT_SET) return -1;
    Delay_ms(10);
    status = MLX90393_readMeasurement(handle, i2c_id, MLX90393_FLAG_Z, &bist_data);
    if (error_process(status) == MLX90393_ERROR_BIT_SET) return -1;

    reg_value &= ~(1 << BIST_SHIFT);
    MLX90393_writeRegister(handle, i2c_id, 0x00, reg_value);

    z_diff = (int32_t)((int16_t)bist_data.z) - (int32_t)((int16_t)baseline.z);
    if (z_diff < 0) z_diff = -z_diff;
    return (z_diff < BIST_MIN_DIFF) ? -1 : 0;
}

uint8_t MLX90393_startMeasurement(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t flags)
{
    uint8_t cmd = MLX90393_CMD_START_MEASUREMENT | (flags & 0x0F);
    return MLX90393_sendCommand(handle, i2c_id, cmd);
}

uint8_t MLX90393_readMeasurement(MLX90393_Handle *handle, uint8_t i2c_id, uint8_t flags, MLX90393_RawData *raw)
{
    uint8_t cmd = MLX90393_CMD_READ_MEASUREMENT | (flags & 0x0F);
    uint8_t count = 1;
    uint8_t buf[9];
    uint8_t index;

    if (flags & MLX90393_FLAG_T) count += 2;
    if (flags & MLX90393_FLAG_X) count += 2;
    if (flags & MLX90393_FLAG_Y) count += 2;
    if (flags & MLX90393_FLAG_Z) count += 2;

    if (i2c_write(i2c_id, handle->I2C_Address, &cmd, 1) != 0) return 0xFF;
    if (i2c_read(i2c_id,  handle->I2C_Address, buf, count) != 0) return 0xFF;

    index = 1;
    raw->t = (flags & MLX90393_FLAG_T) ? ((uint16_t)(buf[index] << 8) | buf[index+1]) : 0;
    if (flags & MLX90393_FLAG_T) index += 2;
    raw->x = (flags & MLX90393_FLAG_X) ? ((uint16_t)(buf[index] << 8) | buf[index+1]) : 0;
    if (flags & MLX90393_FLAG_X) index += 2;
    raw->y = (flags & MLX90393_FLAG_Y) ? ((uint16_t)(buf[index] << 8) | buf[index+1]) : 0;
    if (flags & MLX90393_FLAG_Y) index += 2;
    raw->z = (flags & MLX90393_FLAG_Z) ? ((uint16_t)(buf[index] << 8) | buf[index+1]) : 0;

    return buf[0];
}

MLX90393_Data MLX90393_convertRaw(MLX90393_RawData raw)
{
    MLX90393_Data d;
    d.x = (float)((int16_t)raw.x) * 0.150f;
    d.y = (float)((int16_t)raw.y) * 0.150f;
    d.z = (float)((int16_t)raw.z) * 0.242f;
    d.t = 25.0f + (((float)raw.t - 46244.0f) / 45.2f);
    return d;
}

uint8_t MLX90393_setGainSel(MLX90393_Handle *h, uint8_t id, uint8_t gain)
    { return MLX90393_writeRegister(h, id, GAIN_SEL_REG, (uint16_t)(gain << GAIN_SEL_SHIFT)); }

uint8_t MLX90393_setResolution(MLX90393_Handle *h, uint8_t id, uint8_t rx, uint8_t ry, uint8_t rz)
{
    uint16_t v = (uint16_t)(((rz&3)<<4)|((ry&3)<<2)|(rx&3)) << RES_XYZ_SHIFT;
    return MLX90393_writeRegister(h, id, RES_XYZ_REG, v);
}

uint8_t MLX90393_setOverSampling(MLX90393_Handle *h, uint8_t id, uint8_t osr)
    { return MLX90393_writeRegister(h, id, OSR_REG, (uint16_t)(osr << OSR_SHIFT)); }

uint8_t MLX90393_setDigitalFiltering(MLX90393_Handle *h, uint8_t id, uint8_t flt)
    { return MLX90393_writeRegister(h, id, DIG_FLT_REG, (uint16_t)(flt << DIG_FLT_SHIFT)); }

uint8_t MLX90393_setTemperatureCompensation(MLX90393_Handle *h, uint8_t id, uint8_t en)
    { return MLX90393_writeRegister(h, id, TCMP_EN_REG, (uint16_t)((en?1:0) << TCMP_EN_SHIFT)); }

uint8_t MLX90393_setBurstDataRate(MLX90393_Handle *h, uint8_t id, uint8_t rate)
    { return MLX90393_writeRegister(h, id, BURST_DATA_RATE_REG, (uint16_t)((rate&0x7F) << BURST_DATA_RATE_SHIFT)); }

uint8_t MLX90393_startBurst(MLX90393_Handle *h, uint8_t id, uint8_t flags)
    { return MLX90393_sendCommand(h, id, MLX90393_CMD_START_BURST | (flags & 0x0F)); }
