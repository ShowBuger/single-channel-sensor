#ifndef BOARD_H
#define BOARD_H

/**
 * @file  board.h
 * @brief MA02 on STM32F103C8T6 (TMAG3001) — MCU-neutral board facts.
 */

#define BOARD_NAME                  "ma02_f103"
#define BOARD_UART_BAUD             921600U

/* Sensor manifest: what is fitted on this board. The acquisition layer looks the
 * driver up at runtime; imported configs must match this type and product. */
#define BOARD_SENSOR_TYPE           SENSOR_TYPE_TMAG3001
#define BOARD_PRODUCT_MODEL         "MA02"

/* Identity until the sensor mounting directions are measured. */
#define BOARD_SENSOR_ROTATION_ACTUAL    {1,0,0, 0,1,0, 0,0,1}
#define BOARD_SENSOR_ROTATION_REFERENCE {1,0,0, 0,1,0, 0,0,1}

#endif /* BOARD_H */
