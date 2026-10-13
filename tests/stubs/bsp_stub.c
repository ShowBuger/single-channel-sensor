#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>

/* bsp_delay stubs */
void Delay_Init(void) {}
void Delay_us(uint32_t us) { (void)us; }
void Delay_ms(uint32_t ms) { (void)ms; }
void Delay_s(uint32_t s)   { (void)s; }

/* bsp_uart stubs */
void Serial_Init(void) {}
void Serial_SendByte(uint8_t b) { (void)b; }
void Serial_SendArray(uint8_t *a, uint16_t l) { (void)a;(void)l; }
void Serial_SendString(char *s) { printf("%s", s); }
void Serial_Printf(char *fmt, ...) { va_list a; va_start(a,fmt); vprintf(fmt,a); va_end(a); }
void Serial_SendFloat(float v) { printf("%.3f", v); }
uint8_t Serial_GetRxFlag(void) { return 0; }
uint8_t Serial_GetRxData(void) { return 0; }
uint8_t Serial_GetCommandFlag(void) { return 0; }
char *Serial_GetCommandBuffer(void) { return ""; }
void Serial_ProcessCommand(void) {}

/* bsp_i2c_soft stubs */
void SoftI2C_Init(void) {}
void IIC_Start(uint8_t id) { (void)id; }
void IIC_Stop(uint8_t id)  { (void)id; }
void I2C_WriteByte(uint8_t id, uint8_t b) { (void)id;(void)b; }
uint8_t I2C_WaitAck(uint8_t id) { (void)id; return 1; }
uint8_t I2C_ReadByte(uint8_t id, uint8_t ack) { (void)id;(void)ack; return 0; }
