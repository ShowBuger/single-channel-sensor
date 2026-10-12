#include "config_manager.h"
#include "stm32f1xx_hal.h"
#include <string.h>

/* STM32F103C8T6: 64 KB Flash, 1 KB pages.
 * Store config in the last page: 0x0800FC00. */
#define FLASH_PAGE_SIZE        1024U
#define CONFIG_FLASH_PAGE_ADDR 0x0800FC00U
#define CONFIG_MAGIC_NUMBER    0x52454C54U   /* "RELT" */

typedef struct {
    uint32_t        magic;
    Config_TypeDef  config;
} StoredConfig_TypeDef;

static uint32_t calculate_checksum(const Config_TypeDef *cfg)
{
    uint32_t sum = 0;
    const uint8_t *p = (const uint8_t *)cfg;
    size_t i;
    for (i = 0; i < sizeof(Config_TypeDef) - sizeof(uint32_t); i++)
        sum += p[i];
    return sum;
}

void Config_Init(void)
{
    HAL_FLASH_Unlock();
}

uint8_t Config_Save(uint8_t flag_res, float x_map, float y_map, float z_map)
{
    StoredConfig_TypeDef sc;
    uint32_t addr;
    const uint32_t *src;
    FLASH_EraseInitTypeDef erase;
    uint32_t page_err;
    HAL_StatusTypeDef st;
    uint32_t i, words;

    sc.magic                  = CONFIG_MAGIC_NUMBER;
    sc.config.FLAG_RES        = flag_res;
    sc.config.reserved[0]     = 0;
    sc.config.reserved[1]     = 0;
    sc.config.reserved[2]     = 0;
    sc.config.X_MAPPING_VALUE = x_map;
    sc.config.Y_MAPPING_VALUE = y_map;
    sc.config.Z_MAPPING_VALUE = z_map;
    sc.config.checksum        = calculate_checksum(&sc.config);

    HAL_FLASH_Unlock();

    erase.TypeErase   = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = CONFIG_FLASH_PAGE_ADDR;
    erase.NbPages     = 1;
    st = HAL_FLASHEx_Erase(&erase, &page_err);
    if (st != HAL_OK) { HAL_FLASH_Lock(); return 1; }

    addr  = CONFIG_FLASH_PAGE_ADDR;
    src   = (const uint32_t *)&sc;
    words = sizeof(StoredConfig_TypeDef) / 4U;

    for (i = 0; i < words; i++)
    {
        st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, src[i]);
        if (st != HAL_OK) { HAL_FLASH_Lock(); return 1; }
        addr += 4U;
    }

    HAL_FLASH_Lock();
    return 0;
}

uint8_t Config_Load(uint8_t *flag_res, float *x_map, float *y_map, float *z_map)
{
    const StoredConfig_TypeDef *sc =
        (const StoredConfig_TypeDef *)CONFIG_FLASH_PAGE_ADDR;

    if (sc->magic != CONFIG_MAGIC_NUMBER ||
        calculate_checksum(&sc->config) != sc->config.checksum)
    {
        *flag_res = 0x3B;
        *x_map    = 0.1f;
        *y_map    = 0.1f;
        *z_map    = -0.1f;
        return 1;
    }

    *flag_res = sc->config.FLAG_RES;
    *x_map    = sc->config.X_MAPPING_VALUE;
    *y_map    = sc->config.Y_MAPPING_VALUE;
    *z_map    = sc->config.Z_MAPPING_VALUE;
    return 0;
}

void Config_RestoreDefault(void)
{
    Config_Save(0x3B, 0.1f, 0.1f, -0.1f);
}
