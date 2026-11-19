#ifndef BOARD_H
#define BOARD_H

/**
 * @file  board.h
 * @brief MA03 on STM32H743ZIT6 — MCU-neutral board facts.
 *
 * Included by application and driver code. Pin and peripheral assignments
 * that need MCU types live in board_pins.h and are used only by the port.
 */

#define BOARD_NAME                  "ma03_h743"
#define BOARD_UART_BAUD             921600U

/* Sensor manifest: what is fitted on this board. The acquisition layer looks the
 * driver up at runtime; imported configs must match this type and product. */
#define BOARD_SENSOR_TYPE           SENSOR_TYPE_AK09974C
#define BOARD_PRODUCT_MODEL         "MA03"

/* AK09974C: VIO must match the sensor I/O rail (1V8 on MA03).
 * Address follows IF1/IF2 wiring: 0x21 = IF1->SDA/IF2->SCL, 0x22 = swapped. */
#define BOARD_AK09974C_VIO          AK09974C_VIO_1V8
#define BOARD_AK09974C_ACTUAL_ADDR  AK09974C_I2C_ADDR_CASE1
#define BOARD_AK09974C_REF_ADDR     AK09974C_I2C_ADDR_CASE2

/* Identity until the sensor mounting directions are measured. */
#define BOARD_SENSOR_ROTATION_ACTUAL    {1,0,0, 0,1,0, 0,0,1}
#define BOARD_SENSOR_ROTATION_REFERENCE {1,0,0, 0,1,0, 0,0,1}

#endif /* BOARD_H */
