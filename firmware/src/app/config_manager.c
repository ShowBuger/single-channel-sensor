#include "config_manager.h"
#include "plat.h"
#include "../drivers/sensors/sensor_hal.h"
#include "../drivers/bsp/log_manager.h"
#include <string.h>
#include "config_parser.h"


#define CONFIG_MAGIC_NUMBER    0x52454C57U   /* "RELW" - version 6: CONTINUOUS mode + 16x OSR */

extern void IWDG_Feed(void);

typedef struct {
    uint32_t       magic;
    Config_TypeDef config;
} StoredConfig_TypeDef;

/* Compile-time check: the record must fit the smallest store/stack buffer (1 KB). */
typedef char _size_check[(sizeof(StoredConfig_TypeDef) <= 1024U) ? 1 : -1];

static uint32_t calculate_checksum(const Config_TypeDef *cfg)
{
    uint32_t sum = 0;
    const uint8_t *p = (const uint8_t *)cfg;
    size_t i;
    for (i = 0; i < sizeof(Config_TypeDef) - sizeof(uint32_t); i++)
        sum += p[i];
    return sum;
}

SensorType_t Config_SensorTypeForProduct(const char *product)
{
    if (product && strcmp(product, "MA01") == 0) return SENSOR_TYPE_MLX90393;
    if (product && strcmp(product, "MA02") == 0) return SENSOR_TYPE_TMAG3001;
    if (product && strcmp(product, "MA03") == 0) return SENSOR_TYPE_AK09974C;
    return SENSOR_TYPE_INVALID;
}

uint8_t Config_SetDefaultsForProduct(Config_TypeDef *cfg, const char *product)
{
    SensorType_t type = Config_SensorTypeForProduct(product);
    if (!cfg || type == SENSOR_TYPE_INVALID) return 1;
    memset(cfg, 0, sizeof(*cfg));
    cfg->schema_ver          = CONFIG_SCHEMA_VERSION;
    cfg->sensor_type         = type;
    cfg->FLAG_RES            = CONFIG_DEFAULT_FLAG_RES;
    cfg->log_level           = CONFIG_DEFAULT_LOG_LEVEL;
    cfg->X_MAPPING_VALUE     = CONFIG_DEFAULT_X_MAP;
    cfg->Y_MAPPING_VALUE     = CONFIG_DEFAULT_Y_MAP;
    cfg->Z_MAPPING_VALUE     = CONFIG_DEFAULT_Z_MAP;
    cfg->dead_value          = CONFIG_DEFAULT_DEAD_VALUE;
    cfg->spike_multiplier    = CONFIG_DEFAULT_SPIKE_MULT;
    cfg->stability_threshold = CONFIG_DEFAULT_STABILITY_THR;
    strncpy(cfg->product_model, product, sizeof(cfg->product_model) - 1);
    cfg->product_model[sizeof(cfg->product_model) - 1] = '\0';

    /* 根据传感器类型设置对应的默认值 */
    if (type == SENSOR_TYPE_TMAG3001) {
        /* TMAG3001 特有配置 */
        cfg->gain_sel       = CONFIG_DEFAULT_TMAG_GAIN_SEL;
        cfg->osr            = CONFIG_DEFAULT_TMAG_OSR;
        cfg->range          = CONFIG_DEFAULT_TMAG_RANGE;
        cfg->op_mode        = CONFIG_DEFAULT_TMAG_OP_MODE;
        cfg->mag_ch         = CONFIG_DEFAULT_TMAG_MAG_CH;
        cfg->sensor_version = CONFIG_DEFAULT_TMAG_VERSION;
        /* MLX90393 字段保留合法默认值（不使用） */
        cfg->dig_filt       = CONFIG_DEFAULT_MLX_DIG_FILT;
        cfg->res_x          = CONFIG_DEFAULT_MLX_RES;
        cfg->res_y          = CONFIG_DEFAULT_MLX_RES;
        cfg->res_z          = CONFIG_DEFAULT_MLX_RES;
        cfg->tcmp_en        = CONFIG_DEFAULT_MLX_TCMP_EN;
    } else if (type == SENSOR_TYPE_MLX90393) {
        /* MLX90393 特有配置 */
        cfg->gain_sel       = CONFIG_DEFAULT_MLX_GAIN_SEL;
        cfg->osr            = CONFIG_DEFAULT_MLX_OSR;
        cfg->dig_filt       = CONFIG_DEFAULT_MLX_DIG_FILT;
        cfg->res_x          = CONFIG_DEFAULT_MLX_RES;
        cfg->res_y          = CONFIG_DEFAULT_MLX_RES;
        cfg->res_z          = CONFIG_DEFAULT_MLX_RES;
        cfg->tcmp_en        = CONFIG_DEFAULT_MLX_TCMP_EN;
        /* TMAG3001 字段保留合法默认值（不使用） */
        cfg->range          = CONFIG_DEFAULT_TMAG_RANGE;
        cfg->op_mode        = CONFIG_DEFAULT_TMAG_OP_MODE;
        cfg->mag_ch         = CONFIG_DEFAULT_TMAG_MAG_CH;
        cfg->sensor_version = CONFIG_DEFAULT_TMAG_VERSION;
    } else if (type == SENSOR_TYPE_AK09974C) {
        /* AK09974C 特有配置：osr复用为驱动模式，range为WIDE位 */
        cfg->gain_sel       = 0;
        cfg->osr            = CONFIG_DEFAULT_AK_DRIVE;
        cfg->range          = CONFIG_DEFAULT_AK_RANGE;
        cfg->op_mode        = CONFIG_DEFAULT_AK_OP_MODE;
        cfg->mcyc           = CONFIG_DEFAULT_AK_MCYC;
        /* MLX90393/TMAG3001 字段保留合法默认值（不使用） */
        cfg->dig_filt       = CONFIG_DEFAULT_MLX_DIG_FILT;
        cfg->res_x          = CONFIG_DEFAULT_MLX_RES;
        cfg->res_y          = CONFIG_DEFAULT_MLX_RES;
        cfg->res_z          = CONFIG_DEFAULT_MLX_RES;
        cfg->tcmp_en        = CONFIG_DEFAULT_MLX_TCMP_EN;
        cfg->mag_ch         = CONFIG_DEFAULT_TMAG_MAG_CH;
        cfg->sensor_version = CONFIG_DEFAULT_TMAG_VERSION;
    }
    cfg->checksum = calculate_checksum(cfg);
    return 0;
}

void Config_SetDefaults(Config_TypeDef *cfg)
{
    Config_SetDefaultsForProduct(cfg, CONFIG_DEFAULT_PRODUCT_MODEL);
}

void Config_Init(void)
{
    /* Nothing to do: the platform unlocks the store only while writing. */
}

uint8_t Config_Save(const Config_TypeDef *cfg_in)
{
    StoredConfig_TypeDef sc;

    if (Config_Validate(cfg_in)) return 1;
    memcpy(&sc.config, cfg_in, sizeof(Config_TypeDef));
    sc.magic             = CONFIG_MAGIC_NUMBER;
    sc.config.schema_ver = CONFIG_SCHEMA_VERSION;
    sc.config.checksum   = calculate_checksum(&sc.config);

    if (plat_cfgstore_write(&sc, sizeof(sc), IWDG_Feed) != 0) return 1;
    LOG_I("CFG", "Config saved to Flash");
    return 0;
}

uint8_t Config_Load(Config_TypeDef *cfg)
{
    const StoredConfig_TypeDef *sc =
        (const StoredConfig_TypeDef *)plat_cfgstore_ptr();

    if (sc->magic != CONFIG_MAGIC_NUMBER ||
        sc->config.schema_ver != CONFIG_SCHEMA_VERSION ||
        calculate_checksum(&sc->config) != sc->config.checksum)
    {
        Config_SetDefaults(cfg);
        LOG_W("CFG", "Config invalid, using defaults");
        return 1; /* 1 = defaults applied */
    }

    memcpy(cfg, &sc->config, sizeof(Config_TypeDef));
    /* 旧版TMAG模板曾写成MA01/type=1。仅迁移已通过Flash校验的旧记录，
     * 保留用户的传感器和算法参数；新导入的冲突配置仍由校验拒绝。
     */
    if (cfg->sensor_type == SENSOR_TYPE_TMAG3001 &&
        memchr(cfg->product_model, '\0', sizeof(cfg->product_model)) &&
        strcmp(cfg->product_model, "MA01") == 0) {
        memcpy(cfg->product_model, "MA02", 5);
        cfg->checksum = calculate_checksum(cfg);
        LOG_W("CFG", "Migrated legacy TMAG product MA01 to MA02");
    }
    if (Config_Validate(cfg)) {
        Config_SetDefaults(cfg);
        LOG_W("CFG", "Config fields invalid, using defaults");
        return 1;
    }
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
 * @brief Apply the hardware part of cfg to one sensor through its driver.
 * Sensor-specific register handling lives in each driver adapter (xxx_hal.c).
 */
uint8_t Config_ApplyToSensor(const Config_TypeDef *cfg, const SensorDriver_t *drv,
                             void *ctx, uint8_t bus)
{
    uint8_t result;

    if (!drv || !ctx || Config_Validate(cfg)) return 1;
    result = drv->configure(ctx, bus, cfg);
    if (result == 0) {
        LOG_I("CFG", "Sensor config applied successfully");
    } else {
        LOG_E("CFG", "Sensor config apply failed with error %u", result);
    }
    return result;
}
