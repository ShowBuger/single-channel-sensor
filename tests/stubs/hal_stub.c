#include "stm32f1xx_hal.h"
#include "board_config.h"
#include <string.h>

/* Flash backing store — sized to one config page */
uint8_t _test_flash_page[1024];

/* Initialise the "flash" to the erased state (all 0xFF) */
void test_flash_reset(void)
{
    memset(_test_flash_page, 0xFF, sizeof(_test_flash_page));
}

HAL_StatusTypeDef HAL_FLASH_Unlock(void)
{
    return HAL_OK;
}

HAL_StatusTypeDef HAL_FLASH_Lock(void)
{
    return HAL_OK;
}

HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *pEraseInit, uint32_t *PageError)
{
    (void)pEraseInit;
    *PageError = 0xFFFFFFFFU; /* no faulty page */
    memset(_test_flash_page, 0xFF, sizeof(_test_flash_page));
    return HAL_OK;
}

HAL_StatusTypeDef HAL_FLASH_Program(uint32_t TypeProgram, uint32_t Address, uint64_t Data)
{
    uint32_t base   = (uint32_t)(uintptr_t)_test_flash_page;
    uint32_t offset = Address - base;
    uint32_t word   = (uint32_t)(Data & 0xFFFFFFFFU);
    (void)TypeProgram;

    if (offset + 4U > sizeof(_test_flash_page))
        return HAL_ERROR;

    memcpy(_test_flash_page + offset, &word, 4U);
    return HAL_OK;
}
