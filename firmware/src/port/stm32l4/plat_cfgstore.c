/* STM32L4 port: configuration store in the last 2 KB Flash page.
 * L4 programs 64-bit double words; the tail is padded with 0xFF (erased state).
 * A page erase takes ~25 ms, well inside the watchdog timeout. */
#include "plat.h"
#include "board_pins.h"
#include <string.h>

#define MAX_IMAGE 1024U

const void *plat_cfgstore_ptr(void)  { return (const void *)(uintptr_t)BOARD_CFG_ADDR; }
uint32_t    plat_cfgstore_size(void) { return BOARD_CFG_SIZE; }

uint8_t plat_cfgstore_write(const void *data, uint32_t len, void (*feed)(void))
{
    uint64_t image[MAX_IMAGE / 8U];
    uint32_t padded = (len + 7U) / 8U * 8U;
    uint32_t addr, page_err, i;
    FLASH_EraseInitTypeDef erase;

    if (len == 0U || padded > sizeof(image) || padded > BOARD_CFG_SIZE) return 1;
    memset(image, 0xFF, padded);
    memcpy(image, data, len);
    if (feed) feed();

    HAL_FLASH_Unlock();
    /* Clear stale error flags (e.g. PGSERR after reset), otherwise L4 rejects the erase. */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks     = FLASH_BANK_1;
    erase.Page      = (BOARD_CFG_ADDR - FLASH_BASE) / FLASH_PAGE_SIZE;
    erase.NbPages   = 1;
    if (HAL_FLASHEx_Erase(&erase, &page_err) != HAL_OK) { HAL_FLASH_Lock(); return 1; }

    addr = BOARD_CFG_ADDR;
    for (i = 0; i < padded / 8U; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr, image[i]) != HAL_OK) {
            HAL_FLASH_Lock();
            return 1;
        }
        addr += 8U;
    }
    HAL_FLASH_Lock();
    return 0;
}
