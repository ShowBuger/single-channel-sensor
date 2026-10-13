#include "config_manager.h"
#include "stm32f1xx_hal.h"
#include "board_config.h"
#include <string.h>

#define CONFIG_MAGIC_NUMBER    0x52454C55U   /* "RELU" -- bumped from old "RELT" */

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
    cfg->FLAG_RES            = CONFIG_DEFAULT_FLAG_RES;
    cfg->gain_sel            = CONFIG_DEFAULT_GAIN_SEL;
    cfg->osr                 = CONFIG_DEFAULT_OSR;
    cfg->dig_filt            = CONFIG_DEFAULT_DIG_FILT;
    cfg->res_x               = CONFIG_DEFAULT_RES;
    cfg->res_y               = CONFIG_DEFAULT_RES;
    cfg->res_z               = CONFIG_DEFAULT_RES;
    cfg->tcmp_en             = CONFIG_DEFAULT_TCMP_EN;
    cfg->X_MAPPING_VALUE     = CONFIG_DEFAULT_X_MAP;
    cfg->Y_MAPPING_VALUE     = CONFIG_DEFAULT_Y_MAP;
    cfg->Z_MAPPING_VALUE     = CONFIG_DEFAULT_Z_MAP;
    cfg->dead_value          = CONFIG_DEFAULT_DEAD_VALUE;
    cfg->spike_multiplier    = CONFIG_DEFAULT_SPIKE_MULT;
    cfg->stability_threshold = CONFIG_DEFAULT_STABILITY_THR;
    cfg->checksum            = calculate_checksum(cfg);
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
        return 1; /* 1 = defaults applied */
    }

    memcpy(cfg, &sc->config, sizeof(Config_TypeDef));
    return 0;
}

void Config_RestoreDefault(void)
{
    Config_TypeDef def;
    Config_SetDefaults(&def);
    Config_Save(&def);
}
