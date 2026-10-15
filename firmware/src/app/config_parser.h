#ifndef __CONFIG_PARSER_H
#define __CONFIG_PARSER_H

#include <stdint.h>
#include "config_manager.h"

/**
 * @file config_parser.h
 * @brief 配置解析器 - 独立于Web，支持多种输入格式
 *
 * 支持从JSON字符串、键值对等格式解析配置
 * 自动填充缺失字段的默认值
 */

/* 键值对结构 */
typedef struct {
    const char *key;
    const char *value;
} ConfigKeyValue_t;

/**
 * @brief 从键值对数组解析配置
 * @param cfg 输出的配置结构（会先用默认值填充）
 * @param kvs 键值对数组
 * @param count 键值对数量
 * @return 成功解析的字段数量
 *
 * 示例:
 *   ConfigKeyValue_t kvs[] = {
 *       {"sensor.gain", "5"},
 *       {"sensor.osr", "3"},
 *       {"algo.flag", "0x7B"}
 *   };
 *   uint8_t parsed = Config_ParseKeyValues(&cfg, kvs, 3);
 */
uint8_t Config_ParseKeyValues(Config_TypeDef *cfg, const ConfigKeyValue_t *kvs, uint8_t count);

/**
 * @brief 解析单个键值对并应用到配置
 * @param cfg 配置结构
 * @param key 配置键（如 "sensor.gain"）
 * @param value 配置值（字符串格式）
 * @return 0=成功, 1=未知键, 2=值无效
 */
uint8_t Config_ParseSingleKV(Config_TypeDef *cfg, const char *key, const char *value);

/**
 * @brief 验证配置合法性
 * @param cfg 配置结构
 * @return 0=合法, 非0=字段错误码
 */
uint8_t Config_Validate(const Config_TypeDef *cfg);

/**
 * @brief 打印配置到字符串（用于调试）
 * @param cfg 配置结构
 * @param buf 输出缓冲区
 * @param size 缓冲区大小
 * @return 写入的字符数
 */
uint16_t Config_ToString(const Config_TypeDef *cfg, char *buf, uint16_t size);

/**
 * @brief 获取配置字段的描述信息
 * @param key 配置键
 * @param desc_buf 描述输出缓冲区
 * @param size 缓冲区大小
 * @return 0=成功, 1=未知键
 */
uint8_t Config_GetFieldDescription(const char *key, char *desc_buf, uint16_t size);

#endif /* __CONFIG_PARSER_H */
