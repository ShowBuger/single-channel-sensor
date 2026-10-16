#include "json_config_loader.h"
#include "config_parser.h"
#include <string.h>
#include <ctype.h>

static char *skip_ws(char *p, uint16_t *line)
{
    while (*p && isspace((unsigned char)*p)) {
        if (*p == '\n') (*line)++;
        p++;
    }
    return p;
}

/* 配置中的键和值都是ASCII；不支持需要转义的字符串。 */
static char *parse_string(char *p, char *out, size_t size)
{
    if (*p++ != '"') return NULL;
    size_t n = 0;
    while (*p && *p != '"') {
        if ((unsigned char)*p < 32 || *p == '\\' || n + 1 >= size) return NULL;
        out[n++] = *p++;
    }
    out[n] = '\0';
    return *p == '"' ? p + 1 : NULL;
}

static char *parse_value(char *p, char *out, size_t size, uint8_t allow_string)
{
    if (*p == '"') return allow_string ? parse_string(p, out, size) : NULL;
    char *start = p;
    if (*p == '-') p++;
    if (*p == '0') p++;
    else {
        if (*p < '1' || *p > '9') return NULL;
        while (isdigit((unsigned char)*p)) p++;
    }
    if (*p == '.') {
        p++;
        if (!isdigit((unsigned char)*p)) return NULL;
        while (isdigit((unsigned char)*p)) p++;
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (!isdigit((unsigned char)*p)) return NULL;
        while (isdigit((unsigned char)*p)) p++;
    }
    size_t n = (size_t)(p - start);
    if (!n || n >= size) return NULL;
    memcpy(out, start, n);
    out[n] = '\0';
    return p;
}

uint8_t JsonConfig_Parse(char *json, Config_TypeDef *cfg, JsonParseResult_t *result)
{
    memset(result, 0, sizeof(*result));
    Config_SetDefaults(cfg);
    uint16_t line = 1;
    char key[24] = "";
    /* 第一遍确定产品；第二遍应用字段，使默认补全与JSON键顺序无关。 */
    for (uint8_t pass = 0; pass < 2; pass++) {
        line = 1;
        char *p = skip_ws(json, &line);
        if (*p++ != '{') goto syntax_err;
        p = skip_ws(p, &line);
        while (*p && *p != '}') {
            char value[48];
            p = parse_string(p, key, sizeof(key));
            if (!p) goto syntax_err;
            p = skip_ws(p, &line);
            if (*p++ != ':') goto syntax_err;
            p = skip_ws(p, &line);
            if (!strcmp(key, "product") || !strcmp(key, "$schema")) {
                p = parse_string(p, value, sizeof(value));
                if (!p) goto value_err;
                if (!strcmp(key, "product")) {
                    if (!pass && Config_SetDefaultsForProduct(cfg, value)) goto value_err;
                } else if (strcmp(value, "sensor_config_v1")) goto value_err;
            } else {
                const char *group = !strcmp(key, "algorithm") ? "algo" : key;
                if (strcmp(group, "sensor") && strcmp(group, "algo") && strcmp(group, "log")) goto value_err;
                if (*p++ != '{') goto syntax_err;
                p = skip_ws(p, &line);
                while (*p && *p != '}') {
                    char subkey[24], field[48];
                    p = parse_string(p, subkey, sizeof(subkey));
                    if (!p) goto syntax_err;
                    p = skip_ws(p, &line);
                    if (*p++ != ':') goto syntax_err;
                    p = skip_ws(p, &line);
                    strcpy(field, group);
                    strcat(field, ".");
                    strcat(field, subkey);
                    uint8_t string_ok = !strcmp(field, "sensor.type") || !strcmp(field, "sensor.sensor_type") ||
                        !strcmp(field, "algo.flag") || !strcmp(field, "algo.FLAG_RES") || !strcmp(field, "algo.flag_res");
                    p = parse_value(p, value, sizeof(value), string_ok);
                    if (!p) goto value_err;
                    if (pass && Config_ParseSingleKV(cfg, field, value)) {
                        strncpy(key, subkey, sizeof(key) - 1);
                        goto value_err;
                    }
                    p = skip_ws(p, &line);
                    if (*p == '}') break;
                    if (*p++ != ',') goto syntax_err;
                    p = skip_ws(p, &line);
                    if (*p == '}') goto syntax_err;
                }
                if (*p++ != '}') goto syntax_err;
            }
            p = skip_ws(p, &line);
            if (*p == '}') break;
            if (*p++ != ',') goto syntax_err;
            p = skip_ws(p, &line);
            if (*p == '}') goto syntax_err;
        }
        if (*p++ != '}' || *skip_ws(p, &line)) goto syntax_err;
    }
    return 0;

syntax_err:
    result->status = JSON_PARSE_SYNTAX_ERR;
    goto failed;
value_err:
    result->status = JSON_PARSE_VALUE_ERR;
failed:
    result->error_line = line;
    strncpy(result->error_key, key, sizeof(result->error_key) - 1);
    return 1;
}

uint8_t JsonConfig_Validate(const Config_TypeDef *cfg)
{
    return Config_Validate(cfg);
}
