/* STM32F1 port: configuration store in the last 1 KB Flash page.
 * F1 programs 32-bit words; the tail is padded with 0xFF (erased state).
 * A page erase takes ~20-40 ms, well inside the watchdog timeout. */
#include "plat.h"
#include "board_pins.h"
#include <string.h>

#define MAX_IMAGE 1024U

const void *plat_cfgstore_ptr(void)  { return (const void *)(uintptr_t)BOARD_CFG_ADDR; }
uint32_t    plat_cfgstore_size(void) { return BOARD_CFG_SIZE; }

uint8_t plat_cfgstore_write(const void *data, uint32_t len, void (*feed)(void))
{
    uint32_t image[MAX_IMAGE / 4U];
    uint32_t padded = (len + 3U) / 4U * 4U;
    uint32_t addr, page_err, i;
    FLASH_EraseInitTypeDef erase;

    if (len == 0U || padded > sizeof(image) || padded > BOARD_CFG_SIZE) return 1;
    memset(image, 0xFF, padded);
    memcpy(image, data, len);
    if (feed) feed();

    HAL_FLASH_Unlock();
    erase.TypeErase   = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = BOARD_CFG_ADDR;
    erase.NbPages     = 1;
    if (HAL_FLASHEx_Erase(&erase, &page_err) != HAL_OK) { HAL_FLASH_Lock(); return 1; }

    addr = BOARD_CFG_ADDR;
    for (i = 0; i < padded / 4U; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, image[i]) != HAL_OK) {
            HAL_FLASH_Lock();
            return 1;
        }
        addr += 4U;
    }
    HAL_FLASH_Lock();
    return 0;
}
