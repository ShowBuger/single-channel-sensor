#ifndef __CONFIG_STORAGE_H
#define __CONFIG_STORAGE_H

#include <stdint.h>

// 配置参数结构体
typedef struct {
    uint8_t FLAG_RES;           // FLAG_RES标志位
    uint8_t reserved[3];        // 保留字节，用于4字节对齐
    float X_MAPPING_VALUE;      // X轴映射系数
    float Y_MAPPING_VALUE;      // Y轴映射系数
    float Z_MAPPING_VALUE;      // Z轴映射系数
    uint32_t checksum;          // 校验和，用于验证数据完整性
} Config_TypeDef;

// 函数声明
void Config_Init(void);
uint8_t Config_Save(uint8_t flag_res, float x_map, float y_map, float z_map);
uint8_t Config_Load(uint8_t *flag_res, float *x_map, float *y_map, float *z_map);
void Config_RestoreDefault(void);

#endif
