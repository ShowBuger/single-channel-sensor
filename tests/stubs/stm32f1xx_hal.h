#ifndef __STM32F1XX_HAL_H
#define __STM32F1XX_HAL_H

/*
 * Minimal STM32 HAL stub for host-side unit tests.
 * Only the symbols used by config_manager.c are provided.
 */

#include <stdint.h>
#include <string.h>

typedef enum {
    HAL_OK    = 0x00U,
    HAL_ERROR = 0x01U,
    HAL_BUSY  = 0x02U,
    HAL_TIMEOUT = 0x03U
} HAL_StatusTypeDef;

#define FLASH_TYPEERASE_PAGES       0x01U
#define FLASH_TYPEPROGRAM_WORD      0x01U

typedef struct {
    uint32_t TypeErase;
    uint32_t PageAddress;
    uint32_t NbPages;
} FLASH_EraseInitTypeDef;

/* Function declarations — implemented in hal_stub.c */
HAL_StatusTypeDef HAL_FLASH_Unlock(void);
HAL_StatusTypeDef HAL_FLASH_Lock(void);
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *pEraseInit, uint32_t *PageError);
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t TypeProgram, uint32_t Address, uint64_t Data);

#endif /* __STM32F1XX_HAL_H */
