#ifndef __CONFIG_STORAGE_H
#define __CONFIG_STORAGE_H

#include <stdint.h>

/**
 * @file config_manager.h
 * @brief Persistent configuration -- Flash storage.
 *
 * Config_TypeDef holds ALL tunable parameters:
 *   - Sensor hardware settings (written to sensor registers via configureAll)
 *   - Algorithm parameters (runtime-tunable, formerly hardcoded macros)
 *   - Axis mapping coefficients
 *   - Feature enable flags (FLAG_RES)
 *
 * Layout is append-only for forward compatibility: add fields before checksum,
 * bump CONFIG_SCHEMA_VERSION when doing so (old Flash is detected and defaults applied).
 */

#define CONFIG_SCHEMA_VERSION  2U   /* bump when layout changes */

typedef struct {
    /* -- Schema version -------------------------------- */
    uint8_t  schema_ver;             /* CONFIG_SCHEMA_VERSION            */

    /* -- Feature flags (FLAG_RES) ---------------------- */
    uint8_t  FLAG_RES;               /* bit-mask; see FLAG_* defines      */

    /* -- Sensor hardware config ------------------------ */
    uint8_t  gain_sel;               /* 0-7, default 7 (x1)              */
    uint8_t  osr;                    /* 0-3, default 3 (x8)              */
    uint8_t  dig_filt;               /* 0-7, default 0                   */
    uint8_t  res_x, res_y, res_z;   /* 0-3 per axis, default 0          */
    uint8_t  tcmp_en;                /* 0/1, default 0                   */

    /* -- Axis mapping coefficients --------------------- */
    float    X_MAPPING_VALUE;        /* default  0.100f                  */
    float    Y_MAPPING_VALUE;        /* default  0.100f                  */
    float    Z_MAPPING_VALUE;        /* default -0.100f                  */

    /* -- Algorithm parameters -------------------------- */
    float    dead_value;             /* dead-band x-sigma multiplier, def 8.5 */
    float    spike_multiplier;       /* spike filter x-sigma, def 20.0        */
    float    stability_threshold;    /* stability delta uT, def 0.1           */

    /* -- Padding for 4-byte alignment ------------------ */
    uint8_t  _pad[3];

    /* -- Integrity ------------------------------------- */
    uint32_t checksum;
} Config_TypeDef;                   /* ~52 bytes; fits in 1 KB page    */

/* Default values */
#define CONFIG_DEFAULT_FLAG_RES         0x3BU
#define CONFIG_DEFAULT_GAIN_SEL         7U
#define CONFIG_DEFAULT_OSR              3U
#define CONFIG_DEFAULT_DIG_FILT         0U
#define CONFIG_DEFAULT_RES              0U
#define CONFIG_DEFAULT_TCMP_EN          0U
#define CONFIG_DEFAULT_X_MAP            0.100f
#define CONFIG_DEFAULT_Y_MAP            0.100f
#define CONFIG_DEFAULT_Z_MAP           -0.100f
#define CONFIG_DEFAULT_DEAD_VALUE       8.5f
#define CONFIG_DEFAULT_SPIKE_MULT       20.0f
#define CONFIG_DEFAULT_STABILITY_THR    0.1f

/* Public API */
void    Config_Init(void);
uint8_t Config_Save(const Config_TypeDef *cfg);
uint8_t Config_Load(Config_TypeDef *cfg);
void    Config_SetDefaults(Config_TypeDef *cfg);
void    Config_RestoreDefault(void);

#endif /* __CONFIG_STORAGE_H */
