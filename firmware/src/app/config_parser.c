#include "config_parser.h"
#include "config_manager.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* 辅助函数：字符串转整数 */
static uint8_t parse_uint8(const char *str, uint8_t *out)
{
    char *endptr;
    double val;

    if (strncmp(str, "0x", 2) == 0 || strncmp(str, "0X", 2) == 0) {
        val = strtol(str, &endptr, 16);
    } else {
        val = strtod(str, &endptr);
    }

    if (endptr == str || *endptr != '\0' || !isfinite(val) || val < 0 || val > 255 || val != (uint8_t)val) {
        return 0;  /* 解析失败 */
    }

    *out = (uint8_t)val;
    return 1;
}

/* 辅助函数：字符串转浮点数 */
static uint8_t parse_float(const char *str, float *out)
{
    char *endptr;
    float val = strtof(str, &endptr);

    if (endptr == str || *endptr != '\0' || !isfinite(val)) {
        return 0;  /* 解析失败 */
    }

    *out = val;
    return 1;
}

uint8_t Config_ParseSingleKV(Config_TypeDef *cfg, const char *key, const char *value)
{
    uint8_t tmp_u8;
    float tmp_float;

    if (!key || !value || !cfg) return 2;

    /* -- 传感器类型 -- */
    if (strcmp(key, "sensor.type") == 0 || strcmp(key, "sensor.sensor_type") == 0) {
        if (strcmp(value, "MLX90393") == 0 || strcmp(value, "mlx90393") == 0) {
            cfg->sensor_type = SENSOR_TYPE_MLX90393;
        } else if (strcmp(value, "TMAG3001") == 0 || strcmp(value, "tmag3001") == 0) {
            cfg->sensor_type = SENSOR_TYPE_TMAG3001;
        } else if (parse_uint8(value, &tmp_u8)) {
            cfg->sensor_type = tmp_u8;
        } else return 2;
        return 0;
    }

    /* -- 通用传感器配置 -- */
    if (strcmp(key, "sensor.gain") == 0 || strcmp(key, "sensor.gain_sel") == 0) {
        if (parse_uint8(value, &tmp_u8)) { cfg->gain_sel = tmp_u8; return 0; }
        return 2;
    }
    if (strcmp(key, "sensor.osr") == 0) {
        if (parse_uint8(value, &tmp_u8)) { cfg->osr = tmp_u8; return 0; }
        return 2;
    }

    /* -- MLX90393 特有配置 -- */
    if (strcmp(key, "sensor.filt") == 0 || strcmp(key, "sensor.dig_filt") == 0) {
        if (parse_uint8(value, &tmp_u8)) { cfg->dig_filt = tmp_u8; return 0; }
        return 2;
    }
    if (strcmp(key, "sensor.resx") == 0 || strcmp(key, "sensor.res_x") == 0) {
        if (parse_uint8(value, &tmp_u8)) { cfg->res_x = tmp_u8; return 0; }
        return 2;
    }
    if (strcmp(key, "sensor.resy") == 0 || strcmp(key, "sensor.res_y") == 0) {
        if (parse_uint8(value, &tmp_u8)) { cfg->res_y = tmp_u8; return 0; }
        return 2;
    }
    if (strcmp(key, "sensor.resz") == 0 || strcmp(key, "sensor.res_z") == 0) {
        if (parse_uint8(value, &tmp_u8)) { cfg->res_z = tmp_u8; return 0; }
        return 2;
    }
    if (strcmp(key, "sensor.tcmp") == 0 || strcmp(key, "sensor.tcmp_en") == 0) {
        if (parse_uint8(value, &tmp_u8)) { cfg->tcmp_en = tmp_u8; return 0; }
        return 2;
    }

    /* -- TMAG3001 特有配置 -- */
    if (strcmp(key, "sensor.range") == 0) {
        if (parse_uint8(value, &tmp_u8)) { cfg->range = tmp_u8; return 0; }
        return 2;
    }
    if (strcmp(key, "sensor.opmode") == 0 || strcmp(key, "sensor.mode") == 0 || strcmp(key, "sensor.op_mode") == 0) {
        if (parse_uint8(value, &tmp_u8)) { cfg->op_mode = tmp_u8; return 0; }
        return 2;
    }
    if (strcmp(key, "sensor.magch") == 0 || strcmp(key, "sensor.channel") == 0 || strcmp(key, "sensor.mag_ch") == 0) {
        if (parse_uint8(value, &tmp_u8)) { cfg->mag_ch = tmp_u8; return 0; }
        return 2;
    }
    if (strcmp(key, "sensor.version") == 0 || strcmp(key, "sensor.sensor_version") == 0) {
        if (parse_uint8(value, &tmp_u8)) { cfg->sensor_version = tmp_u8; return 0; }
        return 2;
    }

    /* -- 算法配置 -- */
    if (strcmp(key, "algo.flag") == 0 || strcmp(key, "algo.FLAG_RES") == 0 || strcmp(key, "algo.flag_res") == 0) {
        if (parse_uint8(value, &tmp_u8)) { cfg->FLAG_RES = tmp_u8; return 0; }
        return 2;
    }
    if (strcmp(key, "algo.xmap") == 0 || strcmp(key, "algo.X_MAPPING_VALUE") == 0 || strcmp(key, "algo.x_map") == 0) {
        if (parse_float(value, &tmp_float)) { cfg->X_MAPPING_VALUE = tmp_float; return 0; }
        return 2;
    }
    if (strcmp(key, "algo.ymap") == 0 || strcmp(key, "algo.Y_MAPPING_VALUE") == 0 || strcmp(key, "algo.y_map") == 0) {
        if (parse_float(value, &tmp_float)) { cfg->Y_MAPPING_VALUE = tmp_float; return 0; }
        return 2;
    }
    if (strcmp(key, "algo.zmap") == 0 || strcmp(key, "algo.Z_MAPPING_VALUE") == 0 || strcmp(key, "algo.z_map") == 0) {
        if (parse_float(value, &tmp_float)) { cfg->Z_MAPPING_VALUE = tmp_float; return 0; }
        return 2;
    }
    if (strcmp(key, "algo.dead") == 0 || strcmp(key, "algo.dead_value") == 0) {
        if (parse_float(value, &tmp_float)) { cfg->dead_value = tmp_float; return 0; }
        return 2;
    }
    if (strcmp(key, "algo.spike") == 0 || strcmp(key, "algo.spike_multiplier") == 0) {
        if (parse_float(value, &tmp_float)) { cfg->spike_multiplier = tmp_float; return 0; }
        return 2;
    }
    if (strcmp(key, "algo.stab") == 0 || strcmp(key, "algo.stability_threshold") == 0 || strcmp(key, "algo.stability") == 0) {
        if (parse_float(value, &tmp_float)) { cfg->stability_threshold = tmp_float; return 0; }
        return 2;
    }

    /* -- 系统配置 -- */
    if (strcmp(key, "log.level") == 0) {
        if (parse_uint8(value, &tmp_u8)) { cfg->log_level = tmp_u8; return 0; }
        return 2;
    }

    /* -- 产品信息 -- */
    if (strcmp(key, "product") == 0 || strcmp(key, "product.model") == 0) {
        SensorType_t type = Config_SensorTypeForProduct(value);
        if (type == SENSOR_TYPE_INVALID) return 2;
        strncpy(cfg->product_model, value, sizeof(cfg->product_model) - 1);
        cfg->product_model[sizeof(cfg->product_model) - 1] = '\0';
        cfg->sensor_type = type;
        return 0;
    }

    return 1;  /* 未知键 */
}

uint8_t Config_ParseKeyValues(Config_TypeDef *cfg, const ConfigKeyValue_t *kvs, uint8_t count)
{
    uint8_t parsed = 0;
    uint8_t i;

    if (!cfg || !kvs) return 0;

    /* 先填充默认值 */
    Config_SetDefaults(cfg);

    /* 型号可以出现在任意位置，先选择该产品的默认值。 */
    for (i = 0; i < count; i++) {
        if (kvs[i].key && (!strcmp(kvs[i].key, "product") || !strcmp(kvs[i].key, "product.model"))) {
            if (Config_SetDefaultsForProduct(cfg, kvs[i].value) != 0) return 0;
        }
    }

    /* 逐个解析键值对 */
    for (i = 0; i < count; i++) {
        if (kvs[i].key && (!strcmp(kvs[i].key, "product") || !strcmp(kvs[i].key, "product.model"))) {
            parsed++;
        } else if (Config_ParseSingleKV(cfg, kvs[i].key, kvs[i].value) == 0) {
            parsed++;
        }
    }

    /* 重新计算校验和 */
    cfg->checksum = 0;  /* calculate_checksum会跳过checksum字段 */

    return parsed;
}

uint8_t Config_Validate(const Config_TypeDef *cfg)
{
    if (!cfg) return 0xFF;

    /* 检查schema版本 */
    if (cfg->schema_ver != CONFIG_SCHEMA_VERSION) return 1;

    /* 型号决定生效类型，类型字段作为一致性校验。所有字段都有固定存储地址。 */
    if (!memchr(cfg->product_model, '\0', sizeof(cfg->product_model))) return 2;
    SensorType_t type = Config_SensorTypeForProduct(cfg->product_model);
    if (type == SENSOR_TYPE_INVALID || cfg->sensor_type != type) return 2;
    /* 本分支采集层固定为TMAG3001，不允许把其他型号配置写入当前硬件。 */
    if (type != CURRENT_SENSOR_TYPE) return 3;
    if (cfg->dig_filt > 7 || cfg->res_x > 3 || cfg->res_y > 3 || cfg->res_z > 3 || cfg->tcmp_en > 1) return 12;
    if (cfg->range > 1 || cfg->op_mode > 3 || cfg->mag_ch > 7 || cfg->sensor_version > 1) return 22;

    /* 根据传感器类型检查特定字段 */
    if (cfg->sensor_type == SENSOR_TYPE_MLX90393) {
        if (cfg->gain_sel > 7) return 10;
        if (cfg->osr > 3) return 11;
    } else if (cfg->sensor_type == SENSOR_TYPE_TMAG3001) {
        if (cfg->gain_sel > 15) return 20;
        if (cfg->osr > 5) return 21;
    }

    /* 检查日志级别 */
    if (cfg->log_level > 4) return 30;
    if (!isfinite(cfg->X_MAPPING_VALUE) || !isfinite(cfg->Y_MAPPING_VALUE) || !isfinite(cfg->Z_MAPPING_VALUE) ||
        !isfinite(cfg->dead_value) || !isfinite(cfg->spike_multiplier) || !isfinite(cfg->stability_threshold)) return 31;

    return 0;  /* 验证通过 */
}

uint16_t Config_ToString(const Config_TypeDef *cfg, char *buf, uint16_t size)
{
    uint16_t offset = 0;

    if (!cfg || !buf || size < 100) return 0;

    offset += snprintf(buf + offset, size - offset,
                      "Config v%u [%s]\n", cfg->schema_ver,
                      cfg->sensor_type == SENSOR_TYPE_MLX90393 ? "MLX90393" : "TMAG3001");

    offset += snprintf(buf + offset, size - offset,
                      "Product: %s\n", cfg->product_model);

    offset += snprintf(buf + offset, size - offset,
                      "Sensor: gain=%u osr=%u", cfg->gain_sel, cfg->osr);

    if (cfg->sensor_type == SENSOR_TYPE_MLX90393) {
        offset += snprintf(buf + offset, size - offset,
                          " filt=%u res=%u/%u/%u tcmp=%u\n",
                          cfg->dig_filt, cfg->res_x, cfg->res_y, cfg->res_z, cfg->tcmp_en);
    } else {
        offset += snprintf(buf + offset, size - offset,
                          " range=%u mode=%u ch=%u ver=%u\n",
                          cfg->range, cfg->op_mode, cfg->mag_ch, cfg->sensor_version);
    }

    offset += snprintf(buf + offset, size - offset,
                      "Algo: flag=0x%02X map=%.3f/%.3f/%.3f dead=%.1f spike=%.1f stab=%.3f\n",
                      cfg->FLAG_RES, cfg->X_MAPPING_VALUE, cfg->Y_MAPPING_VALUE,
                      cfg->Z_MAPPING_VALUE, cfg->dead_value, cfg->spike_multiplier,
                      cfg->stability_threshold);

    offset += snprintf(buf + offset, size - offset,
                      "Log: level=%u\n", cfg->log_level);

    return offset;
}

uint8_t Config_GetFieldDescription(const char *key, char *desc_buf, uint16_t size)
{
    const char *desc = NULL;

    if (!key || !desc_buf || size < 10) return 1;

    if (strcmp(key, "sensor.type") == 0) desc = "传感器类型: 0=MLX90393, 1=TMAG3001";
    else if (strcmp(key, "sensor.gain") == 0) desc = "增益选择: MLX[0-7] TMAG[0-15]";
    else if (strcmp(key, "sensor.osr") == 0) desc = "过采样: MLX[0-3] TMAG[0-5]";
    else if (strcmp(key, "sensor.filt") == 0) desc = "数字滤波(MLX): 0-7";
    else if (strcmp(key, "sensor.resx") == 0) desc = "X轴分辨率(MLX): 0-3";
    else if (strcmp(key, "sensor.resy") == 0) desc = "Y轴分辨率(MLX): 0-3";
    else if (strcmp(key, "sensor.resz") == 0) desc = "Z轴分辨率(MLX): 0-3";
    else if (strcmp(key, "sensor.tcmp") == 0) desc = "温度补偿(MLX): 0/1";
    else if (strcmp(key, "sensor.range") == 0) desc = "量程(TMAG): 0=小量程 1=大量程";
    else if (strcmp(key, "sensor.opmode") == 0) desc = "工作模式(TMAG): 0=待机 1=睡眠 2=连续 3=唤醒";
    else if (strcmp(key, "sensor.magch") == 0) desc = "通道选择(TMAG): 0-7 (XYZ组合)";
    else if (strcmp(key, "sensor.version") == 0) desc = "版本(TMAG): 0=A1 1=A2";
    else if (strcmp(key, "algo.flag") == 0) desc = "算法标志位: 位掩码";
    else if (strcmp(key, "algo.xmap") == 0) desc = "X轴映射系数";
    else if (strcmp(key, "algo.ymap") == 0) desc = "Y轴映射系数";
    else if (strcmp(key, "algo.zmap") == 0) desc = "Z轴映射系数";
    else if (strcmp(key, "algo.dead") == 0) desc = "死区倍数";
    else if (strcmp(key, "algo.spike") == 0) desc = "尖峰滤波倍数";
    else if (strcmp(key, "algo.stab") == 0) desc = "稳定性阈值(uT)";
    else if (strcmp(key, "log.level") == 0) desc = "日志级别: 0=无 1=错误 2=警告 3=信息 4=调试";
    else if (strcmp(key, "product") == 0) desc = "产品型号";
    else return 1;

    strncpy(desc_buf, desc, size - 1);
    desc_buf[size - 1] = '\0';
    return 0;
}
