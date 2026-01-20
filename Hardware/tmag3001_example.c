/**
 * TMAG3001 Driver Usage Example - STM32 Standard Library
 *
 * Hardware Connection:
 * - SCL -> PB6 (I2C1) or PB10 (I2C2)
 * - SDA -> PB7 (I2C1) or PB11 (I2C2)
 * - ADDR -> GND/VCC/SDA/SCL (select I2C address)
 * - VCC -> 3.3V
 * - GND -> GND
 */

#include "tmag3001.h"
#include "HardI2C.h"
#include "Serial.h"
#include "Delay.h"

// TMAG3001 device handle
TMAG3001_Handle_t tmag3001;

/**
 * @brief Initialize TMAG3001 example
 * @note Using I2C1 (id=0), ADDR pin to GND, A1 version
 */
void TMAG3001_Example_Init(void)
{
    uint8_t result;
    uint8_t device_id;
    uint16_t manufacturer_id;

    // Initialize hardware I2C first
    HardI2C_Init();

    Serial_Printf("\r\n========== TMAG3001 Init ==========\r\n");

    // Initialize TMAG3001 (I2C1, ADDR=GND, A1 version)
    result = TMAG3001_Init(&tmag3001, 0, TMAG3001_I2C_ADDR_GND, TMAG3001_VERSION_A1);

    if (result == 0) {
        Serial_Printf("TMAG3001 Init Success\r\n");

        // Read device ID
        TMAG3001_ReadID(&tmag3001, &device_id, &manufacturer_id);
        Serial_Printf("Device ID: 0x%02X\r\n", device_id);
        Serial_Printf("Manufacturer ID: 0x%04X\r\n", manufacturer_id);
    } else {
        Serial_Printf("TMAG3001 Init Failed, Error: %d\r\n", result);
    }
}

/**
 * @brief Read and print magnetic field data (raw values only)
 */
void TMAG3001_Example_ReadMagnetic(void)
{
    uint8_t result;
    TMAG3001_Result_t data;

    result = TMAG3001_ReadMagneticData(&tmag3001, &data);

    if (result == 0) {
        Serial_Printf("Magnetic Data (Raw):\r\n");
        Serial_Printf("  X_raw: %d\r\n", data.x_raw);
        Serial_Printf("  Y_raw: %d\r\n", data.y_raw);
        Serial_Printf("  Z_raw: %d\r\n", data.z_raw);
    } else {
        Serial_Printf("Read Magnetic Failed, Error: %d\r\n", result);
    }
}


/**
 * @brief Configuration example - Set measurement range
 */
void TMAG3001_Example_SetRange(void)
{
    uint8_t result;

    // Set range to ±80mT (A1 version only)
    result = TMAG3001_SetRange(&tmag3001, TMAG3001_RANGE_80mT);

    if (result == 0) {
        Serial_Printf("Range set to +/-80mT\r\n");
    } else {
        Serial_Printf("Set Range Failed, Error: %d\r\n", result);
    }
}

/**
 * @brief Configuration example - Set averaging
 */
void TMAG3001_Example_SetAveraging(void)
{
    uint8_t result;

    // Set to 8x averaging for noise reduction
    result = TMAG3001_SetConvAvg(&tmag3001, TMAG3001_AVG_8X);

    if (result == 0) {
        Serial_Printf("Averaging set to 8x\r\n");
    } else {
        Serial_Printf("Set Averaging Failed, Error: %d\r\n", result);
    }
}

/**
 * @brief Main loop example - Continuous reading
 */
void TMAG3001_Example_MainLoop(void)
{
    while (1) {
        // Read magnetic data
        TMAG3001_Example_ReadMagnetic();

        // Delay 1 second
        Delay_ms(1000);
    }
}

/**
 * @brief Single-shot measurement mode example
 */
void TMAG3001_Example_SingleShot(void)
{
    uint8_t result;
    TMAG3001_Result_t data;
    uint8_t conv_status;
    uint16_t timeout;

    // 1. Set to Standby mode
    TMAG3001_SetOperatingMode(&tmag3001, TMAG3001_MODE_STANDBY);
    Serial_Printf("Set to Standby mode\r\n");

    // 2. Trigger conversion
    TMAG3001_TriggerConversion(&tmag3001);
    Serial_Printf("Trigger conversion...\r\n");

    // 3. Wait for conversion complete
    timeout = 0;
    do {
        Delay_ms(10);
        TMAG3001_ReadConvStatus(&tmag3001, &conv_status);
        timeout++;
        if (timeout > 100) {  // 1 second timeout
            Serial_Printf("Conversion timeout!\r\n");
            return;
        }
    } while ((conv_status & 0x01) == 0);  // Wait for CONV_COMPLETE bit

    // 4. Read data
    result = TMAG3001_ReadMagneticData(&tmag3001, &data);
    if (result == 0) {
        Serial_Printf("Single-shot complete:\r\n");
        Serial_Printf("  X_raw: %d, Y_raw: %d, Z_raw: %d\r\n",
                     data.x_raw, data.y_raw, data.z_raw);
    } else {
        Serial_Printf("Read Data Failed, Error: %d\r\n", result);
    }
}

/**
 * @brief Initialize using I2C2
 */
void TMAG3001_Example_InitI2C2(void)
{
    uint8_t result;

    // Initialize hardware I2C first
    HardI2C_Init();

    Serial_Printf("\r\n========== TMAG3001 Init (I2C2) ==========\r\n");

    // Initialize TMAG3001 (I2C2, ADDR=VCC, A1 version)
    result = TMAG3001_Init(&tmag3001, 1, TMAG3001_I2C_ADDR_VCC, TMAG3001_VERSION_A1);

    if (result == 0) {
        Serial_Printf("TMAG3001 Init Success (I2C2)\r\n");
    } else {
        Serial_Printf("TMAG3001 Init Failed, Error: %d\r\n", result);
    }
}

/**
 * @brief Measure X and Y axis only
 */
void TMAG3001_Example_MeasureXY(void)
{
    uint8_t result;
    TMAG3001_Result_t data;

    // Enable X and Y axis only
    result = TMAG3001_SetMagChannels(&tmag3001, TMAG3001_CH_XY);
    if (result == 0) {
        Serial_Printf("Set to measure X and Y only\r\n");
    }

    Delay_ms(100);  // Wait for configuration

    // Read data
    result = TMAG3001_ReadMagneticData(&tmag3001, &data);
    if (result == 0) {
        Serial_Printf("X_raw: %d, Y_raw: %d\r\n", data.x_raw, data.y_raw);
        Serial_Printf("(Z axis data invalid: %d)\r\n", data.z_raw);
    }
}

/**
 * @brief Typical usage in main function
 */
void TMAG3001_Example_Main(void)
{
    // System initialization
    // ...

    // Initialize TMAG3001
    TMAG3001_Example_Init();

    // Main loop
    while (1) {
        // Read magnetic sensor data
        TMAG3001_Example_ReadMagnetic();

        // Delay
        Delay_ms(500);
    }
}
