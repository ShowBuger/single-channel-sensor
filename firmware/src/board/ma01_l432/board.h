#ifndef BOARD_H
#define BOARD_H

/**
 * @file  board.h
 * @brief MA01 on STM32L432KCU6 (MLX90393) — MCU-neutral board facts.
 */

#define BOARD_NAME                  "ma01_l432"
#define BOARD_UART_BAUD             921600U

/* Sensor manifest: what is fitted on this board. The acquisition layer looks the
 * driver up at runtime; imported configs must match this type and product. */
#define BOARD_SENSOR_TYPE           SENSOR_TYPE_MLX90393
#define BOARD_PRODUCT_MODEL         "MA01"

/* Identity until the sensor mounting directions are measured. */
#define BOARD_SENSOR_ROTATION_ACTUAL    {1,0,0, 0,1,0, 0,0,1}
#define BOARD_SENSOR_ROTATION_REFERENCE {1,0,0, 0,1,0, 0,0,1}

#endif /* BOARD_H */
