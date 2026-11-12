#ifndef PLAT_H
#define PLAT_H

/**
 * @file  plat.h
 * @brief MCU-neutral platform interface.
 *
 * Application and driver code talk to the hardware only through these
 * functions. Each MCU family implements them under src/port/<family>/,
 * using pin/peripheral assignments from src/board/<board>/board_pins.h.
 * No HAL or CMSIS header may be included above this layer.
 */

#include <stdint.h>

/* ---- system ------------------------------------------------------------ */
void        plat_sys_init(void);              /* caches, HAL, clock tree        */
uint32_t    plat_reset_flags(void);           /* raw reset-cause register at boot */
uint8_t     plat_reset_was_watchdog(void);
void        plat_sys_reset(void);             /* does not return                */
const char *plat_mcu_name(void);

/* ---- watchdog ---------------------------------------------------------- */
void plat_wdt_start(void);                    /* ~1 s timeout                   */
void plat_wdt_feed(void);

/* ---- time -------------------------------------------------------------- */
void     plat_time_init(void);
uint32_t plat_tick_ms(void);
void     plat_delay_ms(uint32_t ms);
void     plat_delay_us(uint32_t us);
uint32_t plat_cycles(void);                   /* free-running CPU cycle counter */
uint32_t plat_cycles_per_us(void);

/* ---- UART (host link) -------------------------------------------------- */
typedef void (*plat_uart_rx_cb)(uint8_t byte);   /* called from interrupt context */
void plat_uart_init(uint32_t baud, plat_uart_rx_cb on_rx);
void plat_uart_write(const uint8_t *data, uint16_t len);   /* blocking */

/* ---- software I2C pins (open-drain) ------------------------------------ */
uint8_t plat_swi2c_bus_count(void);
void    plat_swi2c_init(uint8_t bus);
void    plat_swi2c_scl(uint8_t bus, uint8_t high);
void    plat_swi2c_sda(uint8_t bus, uint8_t high);
uint8_t plat_swi2c_sda_read(uint8_t bus);

/* ---- persistent configuration storage ---------------------------------- */
const void *plat_cfgstore_ptr(void);          /* memory-mapped, read-only       */
uint32_t    plat_cfgstore_size(void);
/* Erase the store and write len bytes from data (padded to the program unit).
 * feed() is called while waiting so long erases do not trip the watchdog.
 * Returns 0 on success. */
uint8_t     plat_cfgstore_write(const void *data, uint32_t len, void (*feed)(void));

#endif /* PLAT_H */
