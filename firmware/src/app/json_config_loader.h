#ifndef __JSON_CONFIG_LOADER_H
#define __JSON_CONFIG_LOADER_H

#include <stdint.h>
#include "config_manager.h"

/**
 * @file json_config_loader.h
 * @brief Lightweight JSON config parser for UART import
 *
 * Design:
 * - Two-pass parser (product defaults first), no heap allocation
 * - Handles nested objects: sensor{}, algo{}, log{}
 * - Shares field parsing and validation with config_parser
 * - Returns parse status + line number on error
 */

#define JSON_PARSE_OK           0
#define JSON_PARSE_SYNTAX_ERR   1
#define JSON_PARSE_VALUE_ERR    2
#define JSON_PARSE_OVERFLOW     3

typedef struct {
    uint8_t  status;        /* JSON_PARSE_* */
    uint16_t error_line;    /* approximate line number on error */
    char     error_key[24]; /* problematic key name */
} JsonParseResult_t;

/**
 * @brief Parse JSON config string and apply to Config_TypeDef
 * @param json_str  Null-terminated JSON string (modified during parsing)
 * @param cfg       Config structure to populate
 * @param result    Parse result details
 * @return 0 on success, non-zero on error (see result->status)
 */
uint8_t JsonConfig_Parse(char *json_str, Config_TypeDef *cfg, JsonParseResult_t *result);

/**
 * @brief Validate config ranges after parsing
 * @param cfg Config to validate
 * @return 0 if valid, 1 if out-of-range values detected
 */
uint8_t JsonConfig_Validate(const Config_TypeDef *cfg);

#endif /* __JSON_CONFIG_LOADER_H */
