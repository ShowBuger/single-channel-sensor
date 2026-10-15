#include "config_manager.h"
#include "stm32f1xx_hal.h"
#include "board_config.h"
#include "../drivers/sensors/sensor_hal.h"
#include "../drivers/bsp/log_manager.h"
#include <string.h>

#if CURRENT_SENSOR_TYPE == SENSOR_TYPE_TMAG3001
#include "../drivers/sensors/tmag3001/tmag3001.h"
#elif CURRENT_SENSOR_TYPE == SENSOR_TYPE_MLX90393
#include "../drivers/sensors/mlx90393/mlx90393.h"
#endif

#define CONFIG_MAGIC_NUMBER    0x52454C56U   /* "RELV" - version 5 */

typedef struct {
    uint32_t       magic;
    Config_TypeDef config;
} StoredConfig_TypeDef;

/* Compile-time check: struct must fit in the Flash page */
typedef char _size_check[(sizeof(StoredConfig_TypeDef) <= CONFIG_FLASH_PAGE_SIZE) ? 1 : -1];

static uint32_t calculate_checksum(const Config_TypeDef *cfg)
{
    uint32_t sum = 0;
    const uint8_t *p = (const uint8_t *)cfg;
    size_t i;
    for (i = 0; i < sizeof(Config_TypeDef) - sizeof(uint32_t); i++)
        sum += p[i];
    return sum;
}

void Config_SetDefaults(Config_TypeDef *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->schema_ver          = CONFIG_SCHEMA_VERSION;
    cfg->sensor_type         = CONFIG_DEFAULT_SENSOR_TYPE;
    cfg->FLAG_RES            = CONFIG_DEFAULT_FLAG_RES;
    cfg->log_level           = CONFIG_DEFAULT_LOG_LEVEL;
    cfg->X_MAPPING_VALUE     = CONFIG_DEFAULT_X_MAP;
    cfg->Y_MAPPING_VALUE     = CONFIG_DEFAULT_Y_MAP;
    cfg->Z_MAPPING_VALUE     = CONFIG_DEFAULT_Z_MAP;
    cfg->dead_value          = CONFIG_DEFAULT_DEAD_VALUE;
    cfg->spike_multiplier    = CONFIG_DEFAULT_SPIKE_MULT;
    cfg->stability_threshold = CONFIG_DEFAULT_STABILITY_THR;
    strncpy(cfg->product_model, CONFIG_DEFAULT_PRODUCT_MODEL, sizeof(cfg->product_model) - 1);
    cfg->product_model[sizeof(cfg->product_model) - 1] = '\0';

    /* 根据传感器类型设置默认值 */
#if CURRENT_SENSOR_TYPE == SENSOR_TYPE_TMAG3001
    cfg->gain_sel        = CONFIG_DEFAULT_TMAG_GAIN_SEL;
    cfg->osr             = CONFIG_DEFAULT_TMAG_OSR;
    cfg->range           = CONFIG_DEFAULT_TMAG_RANGE;
    cfg->op_mode         = CONFIG_DEFAULT_TMAG_OP_MODE;
    cfg->mag_ch          = CONFIG_DEFAULT_TMAG_MAG_CH;
    cfg->sensor_version  = CONFIG_DEFAULT_TMAG_VERSION;
    /* TMAG3001不使用的字段设为0 */
    cfg->dig_filt = 0;
    cfg->res_x = 0;
    cfg->res_y = 0;
    cfg->res_z = 0;
    cfg->tcmp_en = 0;
#elif CURRENT_SENSOR_TYPE == SENSOR_TYPE_MLX90393
    cfg->gain_sel        = CONFIG_DEFAULT_MLX_GAIN_SEL;
    cfg->osr             = CONFIG_DEFAULT_MLX_OSR;
    cfg->dig_filt        = CONFIG_DEFAULT_MLX_DIG_FILT;
    cfg->res_x           = CONFIG_DEFAULT_MLX_RES;
    cfg->res_y           = CONFIG_DEFAULT_MLX_RES;
    cfg->res_z           = CONFIG_DEFAULT_MLX_RES;
    cfg->tcmp_en         = CONFIG_DEFAULT_MLX_TCMP_EN;
    /* MLX90393不使用的字段设为0 */
    cfg->range = 0;
    cfg->op_mode = 0;
    cfg->mag_ch = 0;
    cfg->sensor_version = 0;
#endif

    cfg->checksum = calculate_checksum(cfg);
}

void Config_Init(void)
{
    HAL_FLASH_Unlock();
}

uint8_t Config_Save(const Config_TypeDef *cfg_in)
{
    StoredConfig_TypeDef sc;
    uint32_t addr, page_err, i, words;
    FLASH_EraseInitTypeDef erase;
    HAL_StatusTypeDef st;
    const uint32_t *src;

    memcpy(&sc.config, cfg_in, sizeof(Config_TypeDef));
    sc.magic             = CONFIG_MAGIC_NUMBER;
    sc.config.schema_ver = CONFIG_SCHEMA_VERSION;
    sc.config.checksum   = calculate_checksum(&sc.config);

    HAL_FLASH_Unlock();

    erase.TypeErase   = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = CONFIG_FLASH_PAGE_ADDR;
    erase.NbPages     = 1;
    st = HAL_FLASHEx_Erase(&erase, &page_err);
    if (st != HAL_OK) { HAL_FLASH_Lock(); return 1; }

    addr  = CONFIG_FLASH_PAGE_ADDR;
    src   = (const uint32_t *)&sc;
    words = (sizeof(StoredConfig_TypeDef) + 3U) / 4U;

    for (i = 0; i < words; i++) {
        st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, src[i]);
        if (st != HAL_OK) { HAL_FLASH_Lock(); return 1; }
        addr += 4U;
    }

    HAL_FLASH_Lock();
    LOG_I("CFG", "Config saved to Flash");
    return 0;
}

uint8_t Config_Load(Config_TypeDef *cfg)
{
    const StoredConfig_TypeDef *sc =
        (const StoredConfig_TypeDef *)(uintptr_t)CONFIG_FLASH_PAGE_ADDR;

    if (sc->magic != CONFIG_MAGIC_NUMBER ||
        sc->config.schema_ver != CONFIG_SCHEMA_VERSION ||
        calculate_checksum(&sc->config) != sc->config.checksum)
    {
        Config_SetDefaults(cfg);
        LOG_W("CFG", "Config invalid, using defaults");
        return 1; /* 1 = defaults applied */
    }

    memcpy(cfg, &sc->config, sizeof(Config_TypeDef));
    LOG_I("CFG", "Config loaded from Flash");
    return 0;
}

void Config_RestoreDefault(void)
{
    Config_TypeDef def;
    Config_SetDefaults(&def);
    Config_Save(&def);
    LOG_I("CFG", "Config restored to defaults");
}

/**
 * @brief 应用配置到传感器
 * @param cfg 配置结构
 * @param sensor_handle 传感器句柄（MLX90393_Handle* 或 TMAG3001_Handle_t*）
 * @param i2c_id I2C总线ID
 * @return 0=成功, 非0=失败
 */
uint8_t Config_ApplyToSensor(const Config_TypeDef *cfg, void *sensor_handle, uint8_t i2c_id)
{
    uint8_t result = 0;

#if CURRENT_SENSOR_TYPE == SENSOR_TYPE_TMAG3001
    TMAG3001_Handle_t *dev = (TMAG3001_Handle_t *)sensor_handle;

    LOG_D("CFG", "Applying TMAG3001 config: gain=%u osr=%u range=%u mode=%u ch=%u",
          cfg->gain_sel, cfg->osr, cfg->range, cfg->op_mode, cfg->mag_ch);

    /* 应用增益配置 */
    if (cfg->gain_sel <= 15) {
        result |= TMAG3001_SetMagGain(dev, (TMAG3001_MagGain_t)cfg->gain_sel);
    } else {
        LOG_W("CFG", "Invalid gain %u, using default", cfg->gain_sel);
        result |= TMAG3001_SetMagGain(dev, TMAG3001_GAIN_1_0);
    }

    /* 应用平均次数配置 */
    if (cfg->osr <= 5) {
        result |= TMAG3001_SetConvAvg(dev, (TMAG3001_ConvAvg_t)cfg->osr);
    } else {
        LOG_W("CFG", "Invalid osr %u, using default", cfg->osr);
        result |= TMAG3001_SetConvAvg(dev, TMAG3001_AVG_4X);
    }

    /* 应用量程配置 */
    if (cfg->range <= 1) {
        result |= TMAG3001_SetRange(dev, (TMAG3001_Range_t)cfg->range);
    } else {
        LOG_W("CFG", "Invalid range %u, using default", cfg->range);
        result |= TMAG3001_SetRange(dev, TMAG3001_RANGE_40mT);
    }

    /* 应用工作模式配置 */
    if (cfg->op_mode <= 3) {
        result |= TMAG3001_SetOperatingMode(dev, (TMAG3001_OperatingMode_t)cfg->op_mode);
    } else {
        LOG_W("CFG", "Invalid op_mode %u, using default", cfg->op_mode);
        result |= TMAG3001_SetOperatingMode(dev, TMAG3001_MODE_STANDBY);
    }

    /* 应用通道选择配置 */
    if (cfg->mag_ch <= 7) {
        result |= TMAG3001_SetMagChannels(dev, (TMAG3001_MagChannel_t)cfg->mag_ch);
    } else {
        LOG_W("CFG", "Invalid mag_ch %u, using default", cfg->mag_ch);
        result |= TMAG3001_SetMagChannels(dev, TMAG3001_CH_XYZ);
    }

#elif CURRENT_SENSOR_TYPE == SENSOR_TYPE_MLX90393
    MLX90393_Handle *dev = (MLX90393_Handle *)sensor_handle;

    LOG_D("CFG", "Applying MLX90393 config: gain=%u osr=%u filt=%u res=%u/%u/%u tcmp=%u",
          cfg->gain_sel, cfg->osr, cfg->dig_filt, cfg->res_x, cfg->res_y, cfg->res_z, cfg->tcmp_en);

    /* 使用MLX90393_Config结构批量配置 */
    MLX90393_Config mlx_cfg;
    mlx_cfg.gain_sel   = (cfg->gain_sel <= 7) ? cfg->gain_sel : CONFIG_DEFAULT_MLX_GAIN_SEL;
    mlx_cfg.osr        = (cfg->osr <= 3) ? cfg->osr : CONFIG_DEFAULT_MLX_OSR;
    mlx_cfg.dig_filt   = (cfg->dig_filt <= 7) ? cfg->dig_filt : CONFIG_DEFAULT_MLX_DIG_FILT;
    mlx_cfg.res_x      = (cfg->res_x <= 3) ? cfg->res_x : CONFIG_DEFAULT_MLX_RES;
    mlx_cfg.res_y      = (cfg->res_y <= 3) ? cfg->res_y : CONFIG_DEFAULT_MLX_RES;
    mlx_cfg.res_z      = (cfg->res_z <= 3) ? cfg->res_z : CONFIG_DEFAULT_MLX_RES;
    mlx_cfg.tcmp_en    = (cfg->tcmp_en <= 1) ? cfg->tcmp_en : CONFIG_DEFAULT_MLX_TCMP_EN;
    mlx_cfg.burst_rate = 0;  /* 不使用Burst模式 */

    result = MLX90393_configureAll(dev, i2c_id, &mlx_cfg);
    if (error_process(result) != MLX90393_ERROR_NONE) {
        LOG_E("CFG", "MLX90393 config apply failed");
        return 1;
    }
#else
    #error "Unsupported sensor type"
#endif

    if (result == 0) {
        LOG_I("CFG", "Sensor config applied successfully");
    } else {
        LOG_E("CFG", "Sensor config apply failed with error %u", result);
    }

    return result;
}
