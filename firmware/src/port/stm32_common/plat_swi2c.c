/* STM32 HAL ports (F1/L4/H7): open-drain GPIOs for the software I2C bus.
 * The GPIO HAL API is identical across these families; pins come from board_pins.h. */
#include "plat.h"
#include "board_pins.h"

typedef struct {
    GPIO_TypeDef *scl_port;
    uint16_t      scl_pin;
    GPIO_TypeDef *sda_port;
    uint16_t      sda_pin;
} swi2c_pins_t;

static const swi2c_pins_t s_bus[] = {
    { BOARD_SWI2C0_SCL_PORT, BOARD_SWI2C0_SCL_PIN, BOARD_SWI2C0_SDA_PORT, BOARD_SWI2C0_SDA_PIN },
};
#define BUS_COUNT ((uint8_t)(sizeof(s_bus) / sizeof(s_bus[0])))

uint8_t plat_swi2c_bus_count(void) { return BUS_COUNT; }

void plat_swi2c_init(uint8_t bus)
{
    GPIO_InitTypeDef gpio = {0};
    if (bus >= BUS_COUNT) return;

    BOARD_SWI2C_CLK_EN();
    gpio.Mode  = GPIO_MODE_OUTPUT_OD;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    gpio.Pin = s_bus[bus].scl_pin;
    HAL_GPIO_Init(s_bus[bus].scl_port, &gpio);
    HAL_GPIO_WritePin(s_bus[bus].scl_port, s_bus[bus].scl_pin, GPIO_PIN_SET);

    gpio.Pin = s_bus[bus].sda_pin;
    HAL_GPIO_Init(s_bus[bus].sda_port, &gpio);
    HAL_GPIO_WritePin(s_bus[bus].sda_port, s_bus[bus].sda_pin, GPIO_PIN_SET);
}

void plat_swi2c_scl(uint8_t bus, uint8_t high)
{
    HAL_GPIO_WritePin(s_bus[bus].scl_port, s_bus[bus].scl_pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void plat_swi2c_sda(uint8_t bus, uint8_t high)
{
    HAL_GPIO_WritePin(s_bus[bus].sda_port, s_bus[bus].sda_pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

uint8_t plat_swi2c_sda_read(uint8_t bus)
{
    return HAL_GPIO_ReadPin(s_bus[bus].sda_port, s_bus[bus].sda_pin) == GPIO_PIN_SET;
}
