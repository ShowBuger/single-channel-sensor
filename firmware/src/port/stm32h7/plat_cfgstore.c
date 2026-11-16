/* STM32H7 port: configuration store in the last bank-2 sector.
 *
 * H7 programs whole 256-bit flash words (32 bytes); the tail is padded with
 * 0xFF (erased state). A 128 KB sector erase takes ~2 s, longer than the 1 s
 * IWDG timeout, so the erase is started without the blocking HAL call and
 * feed() is called while waiting. Bank 2 holds no code, so the CPU keeps
 * fetching from bank 1 during the erase. */
#include "plat.h"
#include "board_pins.h"
#include <string.h>

#define WORD_BYTES   (FLASH_NB_32BITWORD_IN_FLASHWORD * 4U)   /* 32 */
#define TIMEOUT_MS   5000U
#define MAX_IMAGE    1024U                                    /* bytes buffered on stack */

const void *plat_cfgstore_ptr(void)  { return (const void *)(uintptr_t)BOARD_CFG_ADDR; }
uint32_t    plat_cfgstore_size(void) { return BOARD_CFG_SIZE; }

uint8_t plat_cfgstore_write(const void *data, uint32_t len, void (*feed)(void))
{
    uint32_t image[MAX_IMAGE / 4U] __attribute__((aligned(32)));
    uint32_t padded = (len + WORD_BYTES - 1U) / WORD_BYTES * WORD_BYTES;
    uint32_t addr, i;
    uint8_t  ret = 1;

    if (len == 0U || padded > sizeof(image) || padded > BOARD_CFG_SIZE) return 1;
    memset(image, 0xFF, padded);
    memcpy(image, data, len);

    HAL_FLASH_Unlock();
    /* Clear stale error flags (e.g. PGSERR after reset), otherwise H7 rejects the erase. */
    __HAL_FLASH_CLEAR_FLAG_BANK2(FLASH_FLAG_ALL_ERRORS_BANK2);

    FLASH_Erase_Sector(BOARD_CFG_SECTOR, BOARD_CFG_BANK, FLASH_VOLTAGE_RANGE_3);
    while (__HAL_FLASH_GET_FLAG_BANK2(FLASH_FLAG_QW_BANK2)) {
        if (feed) feed();
    }
    if (FLASH_WaitForLastOperation(TIMEOUT_MS, BOARD_CFG_BANK) != HAL_OK) {
        FLASH->CR2 &= ~(FLASH_CR_SER | FLASH_CR_SNB);
        goto out;
    }
    FLASH->CR2 &= ~(FLASH_CR_SER | FLASH_CR_SNB);

    addr = BOARD_CFG_ADDR;
    for (i = 0; i < padded / 4U; i += WORD_BYTES / 4U) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, addr,
                              (uint32_t)(uintptr_t)&image[i]) != HAL_OK)
            goto out;
        addr += WORD_BYTES;
    }
    ret = 0;

out:
    HAL_FLASH_Lock();
    /* Flash content changed: drop stale D-cache lines before anyone reads it back. */
    SCB_InvalidateDCache_by_Addr((void *)(uintptr_t)BOARD_CFG_ADDR, (int32_t)padded);
    return ret;
}
