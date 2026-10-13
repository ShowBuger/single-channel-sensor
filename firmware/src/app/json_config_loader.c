#include "json_config_loader.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* Helper: skip whitespace and count newlines */
static char *skip_ws(char *p, uint16_t *line)
{
    while (*p && isspace((unsigned char)*p))
    {
        if (*p == '\n') (*line)++;
        p++;
    }
    return p;
}

/* Helper: parse number (int or float) */
static char *parse_number(char *p, float *out)
{
    char *end;
    *out = strtof(p, &end);
    return (end > p) ? end : NULL;
}

/* Helper: parse string value (strip quotes) */
static char *parse_string(char *p, char *buf, size_t bufsize)
{
    if (*p != '"') return NULL;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i < bufsize - 1)
        buf[i++] = *p++;
    buf[i] = '\0';
    return (*p == '"') ? (p + 1) : NULL;
}

/* Helper: parse hex string like "0x3B" */
static char *parse_hex(char *p, uint32_t *out)
{
    if (p[0] == '"' && p[1] == '0' && (p[2] == 'x' || p[2] == 'X'))
    {
        char *end;
        *out = strtoul(p + 3, &end, 16);
        if (*end == '"') return end + 1;
    }
    return NULL;
}

/* Main parser */
uint8_t JsonConfig_Parse(char *json_str, Config_TypeDef *cfg, JsonParseResult_t *result)
{
    memset(result, 0, sizeof(*result));
    result->status = JSON_PARSE_OK;
    uint16_t line = 1;

    char *p = skip_ws(json_str, &line);
    if (*p != '{') { result->status = JSON_PARSE_SYNTAX_ERR; result->error_line = line; return 1; }
    p = skip_ws(p + 1, &line);

    /* Start with defaults, then override from JSON */
    Config_SetDefaults(cfg);

    while (*p && *p != '}')
    {
        /* Parse key */
        char key[24];
        p = skip_ws(p, &line);
        if (*p == ',') { p++; continue; }
        if (*p == '}') break;

        p = parse_string(p, key, sizeof(key));
        if (!p) { result->status = JSON_PARSE_SYNTAX_ERR; result->error_line = line; return 1; }

        p = skip_ws(p, &line);
        if (*p != ':') { result->status = JSON_PARSE_SYNTAX_ERR; result->error_line = line; return 1; }
        p = skip_ws(p + 1, &line);

        /* Parse value based on key */
        if (strcmp(key, "sensor") == 0 || strcmp(key, "algo") == 0 || strcmp(key, "log") == 0)
        {
            /* Nested object */
            if (*p != '{') { result->status = JSON_PARSE_SYNTAX_ERR; result->error_line = line; return 1; }
            p = skip_ws(p + 1, &line);

            while (*p && *p != '}')
            {
                char subkey[24];
                p = skip_ws(p, &line);
                if (*p == ',') { p++; continue; }
                if (*p == '}') break;

                p = parse_string(p, subkey, sizeof(subkey));
                if (!p) { result->status = JSON_PARSE_SYNTAX_ERR; result->error_line = line; return 1; }

                p = skip_ws(p, &line);
                if (*p != ':') { result->status = JSON_PARSE_SYNTAX_ERR; result->error_line = line; return 1; }
                p = skip_ws(p + 1, &line);

                float fval;
                uint32_t uval;
                char *np;

                /* Sensor config */
                if (strcmp(key, "sensor") == 0)
                {
                    if (strcmp(subkey, "gain") == 0) {
                        if (!(np = parse_number(p, &fval))) goto parse_err;
                        cfg->gain_sel = (uint8_t)fval; p = np;
                    }
                    else if (strcmp(subkey, "osr") == 0) {
                        if (!(np = parse_number(p, &fval))) goto parse_err;
                        cfg->osr = (uint8_t)fval; p = np;
                    }
                    else if (strcmp(subkey, "filt") == 0 || strcmp(subkey, "dig_filt") == 0) {
                        if (!(np = parse_number(p, &fval))) goto parse_err;
                        cfg->dig_filt = (uint8_t)fval; p = np;
                    }
                    else if (strcmp(subkey, "resx") == 0 || strcmp(subkey, "res_x") == 0) {
                        if (!(np = parse_number(p, &fval))) goto parse_err;
                        cfg->res_x = (uint8_t)fval; p = np;
                    }
                    else if (strcmp(subkey, "resy") == 0 || strcmp(subkey, "res_y") == 0) {
                        if (!(np = parse_number(p, &fval))) goto parse_err;
                        cfg->res_y = (uint8_t)fval; p = np;
                    }
                    else if (strcmp(subkey, "resz") == 0 || strcmp(subkey, "res_z") == 0) {
                        if (!(np = parse_number(p, &fval))) goto parse_err;
                        cfg->res_z = (uint8_t)fval; p = np;
                    }
                    else if (strcmp(subkey, "tcmp") == 0 || strcmp(subkey, "tcmp_en") == 0) {
                        if (!(np = parse_number(p, &fval))) goto parse_err;
                        cfg->tcmp_en = (uint8_t)fval; p = np;
                    }
                }
                /* Algorithm config */
                else if (strcmp(key, "algo") == 0)
                {
                    if (strcmp(subkey, "flag") == 0 || strcmp(subkey, "FLAG_RES") == 0) {
                        /* Try hex first, then decimal */
                        if ((np = parse_hex(p, &uval))) {
                            cfg->FLAG_RES = (uint8_t)uval; p = np;
                        }
                        else if ((np = parse_number(p, &fval))) {
                            cfg->FLAG_RES = (uint8_t)fval; p = np;
                        }
                        else goto parse_err;
                    }
                    else if (strcmp(subkey, "xmap") == 0 || strcmp(subkey, "X_MAPPING_VALUE") == 0) {
                        if (!(np = parse_number(p, &fval))) goto parse_err;
                        cfg->X_MAPPING_VALUE = fval; p = np;
                    }
                    else if (strcmp(subkey, "ymap") == 0 || strcmp(subkey, "Y_MAPPING_VALUE") == 0) {
                        if (!(np = parse_number(p, &fval))) goto parse_err;
                        cfg->Y_MAPPING_VALUE = fval; p = np;
                    }
                    else if (strcmp(subkey, "zmap") == 0 || strcmp(subkey, "Z_MAPPING_VALUE") == 0) {
                        if (!(np = parse_number(p, &fval))) goto parse_err;
                        cfg->Z_MAPPING_VALUE = fval; p = np;
                    }
                    else if (strcmp(subkey, "dead") == 0 || strcmp(subkey, "dead_value") == 0) {
                        if (!(np = parse_number(p, &fval))) goto parse_err;
                        cfg->dead_value = fval; p = np;
                    }
                    else if (strcmp(subkey, "spike") == 0 || strcmp(subkey, "spike_multiplier") == 0) {
                        if (!(np = parse_number(p, &fval))) goto parse_err;
                        cfg->spike_multiplier = fval; p = np;
                    }
                    else if (strcmp(subkey, "stab") == 0 || strcmp(subkey, "stability_threshold") == 0) {
                        if (!(np = parse_number(p, &fval))) goto parse_err;
                        cfg->stability_threshold = fval; p = np;
                    }
                }
                /* Log config */
                else if (strcmp(key, "log") == 0)
                {
                    if (strcmp(subkey, "level") == 0) {
                        if (!(np = parse_number(p, &fval))) goto parse_err;
                        cfg->log_level = (uint8_t)fval; p = np;
                    }
                }

                p = skip_ws(p, &line);
                continue;

parse_err:
                strncpy(result->error_key, subkey, sizeof(result->error_key) - 1);
                result->status = JSON_PARSE_VALUE_ERR;
                result->error_line = line;
                return 1;
            }

            if (*p == '}') p++;
        }

        p = skip_ws(p, &line);
    }

    return 0;
}

uint8_t JsonConfig_Validate(const Config_TypeDef *cfg)
{
    if (cfg->gain_sel > 7) return 1;
    if (cfg->osr > 3) return 1;
    if (cfg->dig_filt > 7) return 1;
    if (cfg->res_x > 3 || cfg->res_y > 3 || cfg->res_z > 3) return 1;
    if (cfg->tcmp_en > 1) return 1;
    if (cfg->log_level > 4) return 1;
    return 0;
}
