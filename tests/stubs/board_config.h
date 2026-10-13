#ifndef __BOARD_CONFIG_H
#define __BOARD_CONFIG_H

#include <stdint.h>
#include <stddef.h>

/*
 * Test stub for board_config.h.
 * Replaces the real firmware board_config.h when building host tests.
 * Provides a static array as the flash backing store so config_manager.c
 * can read/write "flash" without any STM32 hardware.
 */

/* Flash backing store — defined in hal_stub.c */
extern uint8_t _test_flash_page[1024];

/* Use the array's run-time address cast to the uint32_t the firmware uses.
 * On a 64-bit host BSS is typically in the low 4 GB so this truncation is
 * safe in practice; the HAL stub computes the offset the same way. */
#define CONFIG_FLASH_PAGE_ADDR  ((uint32_t)(uintptr_t)_test_flash_page)
#define CONFIG_FLASH_PAGE_SIZE  1024U

/* No sensor HAL selection needed for unit tests */

#endif /* __BOARD_CONFIG_H */
