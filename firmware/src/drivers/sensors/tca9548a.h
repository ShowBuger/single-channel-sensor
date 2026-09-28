#ifndef __TCA9548A_H
#define __TCA9548A_H

#include <stdint.h>

#define TCA9548A_ADDR_0    0x70    // A2=0, A1=0, A0=0 (U5在I2C1, U6在I2C2, 地址相同)

#define TCA9548A_CH0    0x01
#define TCA9548A_CH1    0x02
#define TCA9548A_CH2    0x04
#define TCA9548A_CH3    0x08
#define TCA9548A_CH4    0x10
#define TCA9548A_CH5    0x20
#define TCA9548A_CH6    0x40
#define TCA9548A_CH7    0x80
#define TCA9548A_CH_NONE 0x00

// 初始化TCA9548A (配置RESET引脚并释放复位)
void TCA9548A_Init(void);

uint8_t TCA9548A_SelectChannel(uint8_t i2c_id, uint8_t tca_addr, uint8_t channel);

uint8_t TCA9548A_ReadChannel(uint8_t i2c_id, uint8_t tca_addr, uint8_t *channel);

uint8_t TCA9548A_DisableAllChannels(uint8_t i2c_id, uint8_t tca_addr);

#endif
