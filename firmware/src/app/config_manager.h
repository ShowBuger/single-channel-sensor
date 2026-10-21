#ifndef __CONFIG_MANAGER_H
#define __CONFIG_MANAGER_H

#include <stdint.h>
#include <stddef.h>

/**
 * @file config_manager.h
 * @brief 统一的传感器配置管理系统
 *
 * 支持MLX90393和TMAG3001的配置存储和应用
 * 配置存储在Flash中，支持字段缺省和默认值填充
 */

#define CONFIG_SCHEMA_VERSION  5U   /* bump when layout changes */

/* 类型也用于预处理判断，必须是数值宏。 */
#define SENSOR_TYPE_MLX90393 0U
#define SENSOR_TYPE_TMAG3001 1U
#define SENSOR_TYPE_INVALID 0xFFU
typedef uint8_t SensorType_t;

/* 选择当前使用的传感器类型 */
#define CURRENT_SENSOR_TYPE  SENSOR_TYPE_TMAG3001

typedef struct {
    /* -- Schema version -------------------------------- */
    uint8_t  schema_ver;             /* CONFIG_SCHEMA_VERSION            */
    uint8_t  sensor_type;            /* SensorType_t: 传感器类型         */

    /* -- Feature flags (FLAG_RES) ---------------------- */
    uint8_t  FLAG_RES;               /* bit-mask; see FLAG_* defines      */

    /* -- 通用传感器配置 (两种传感器都支持) ------------- */
    uint8_t  gain_sel;               /* 增益选择
                                      * MLX90393: 0-7 (7=×1最低增益)
                                      * TMAG3001: 0-15 (5=1.0x, 0=0.5x, 15=2.0x) */
    uint8_t  osr;                    /* 过采样/平均次数
                                      * MLX90393: 0-3 (×1/2/4/8)
                                      * TMAG3001: 0-5 (1x/2x/4x/8x/16x/32x) */

    /* -- MLX90393 特有配置 ----------------------------- */
    uint8_t  dig_filt;               /* 数字滤波器 0-7 (仅MLX90393)       */
    uint8_t  res_x, res_y, res_z;   /* 各轴分辨率 0-3 (仅MLX90393)       */
    uint8_t  tcmp_en;                /* 温度补偿使能 0/1 (仅MLX90393)     */

    /* -- TMAG3001 特有配置 ----------------------------- */
    uint8_t  range;                  /* 量程选择 (仅TMAG3001)
                                      * A1版本: 0=±40mT, 1=±80mT
                                      * A2版本: 0=±120mT, 1=±240mT */
    uint8_t  op_mode;                /* 工作模式 (仅TMAG3001)
                                      * 0=Standby, 1=Sleep, 2=Continuous, 3=Wakeup */
    uint8_t  mag_ch;                 /* 磁场通道选择 (仅TMAG3001)
                                      * 0=None, 1=X, 2=Y, 3=XY, 4=Z, 5=XZ, 6=YZ, 7=XYZ */
    uint8_t  sensor_version;         /* 传感器版本 (仅TMAG3001)
                                      * 0=A1(±40/80mT), 1=A2(±120/240mT) */

    /* -- 通用配置 -------------------------------------- */
    uint8_t  log_level;              /* LogLevel_t stored as uint8_t     */

    /* -- Axis mapping coefficients --------------------- */
    float    X_MAPPING_VALUE;        /* default  1.0f                    */
    float    Y_MAPPING_VALUE;        /* default  1.0f                    */
    float    Z_MAPPING_VALUE;        /* default  1.0f                    */

    /* -- Algorithm parameters -------------------------- */
    float    dead_value;             /* dead-band x-sigma multiplier, def 8.5 */
    float    spike_multiplier;       /* spike filter x-sigma, def 20.0        */
    float    stability_threshold;    /* stability delta uT, def 0.1           */

    /* -- Product information --------------------------- */
    char     product_model[8];       /* product model, e.g. "MA01"            */

    /* -- Padding for 4-byte alignment ------------------ */
    uint8_t  _pad[1];

    /* -- Integrity ------------------------------------- */
    uint32_t checksum;
} Config_TypeDef;                   /* 56 bytes, same layout for both products */
_Static_assert(sizeof(Config_TypeDef) == 56, "Config layout changed");
_Static_assert(offsetof(Config_TypeDef, range) == 10, "Config range offset changed");
_Static_assert(offsetof(Config_TypeDef, product_model) == 40, "Config product offset changed");
_Static_assert(offsetof(Config_TypeDef, checksum) == 52, "Config checksum offset changed");

/* Default values - 通用 */
#define CONFIG_DEFAULT_SENSOR_TYPE      CURRENT_SENSOR_TYPE
#define CONFIG_DEFAULT_FLAG_RES         0x00U
#define CONFIG_DEFAULT_LOG_LEVEL        4U     /* Debug 级别 */
#define CONFIG_DEFAULT_X_MAP            1.0f
#define CONFIG_DEFAULT_Y_MAP            1.0f
#define CONFIG_DEFAULT_Z_MAP            1.0f
#define CONFIG_DEFAULT_DEAD_VALUE       8.5f
#define CONFIG_DEFAULT_SPIKE_MULT       20.0f
#define CONFIG_DEFAULT_STABILITY_THR    0.1f

/* 根据当前传感器类型自动选择默认产品型号 */
#if CURRENT_SENSOR_TYPE == SENSOR_TYPE_MLX90393
#define CONFIG_DEFAULT_PRODUCT_MODEL    "MA01"
#elif CURRENT_SENSOR_TYPE == SENSOR_TYPE_TMAG3001
#define CONFIG_DEFAULT_PRODUCT_MODEL    "MA02"
#else
#error "Unknown sensor type for CONFIG_DEFAULT_PRODUCT_MODEL"
#endif

/* Default values - MLX90393 */
#define CONFIG_DEFAULT_MLX_GAIN_SEL     7U     /* x1 最低增益 */
#define CONFIG_DEFAULT_MLX_OSR          2U     /* x4 过采样 */
#define CONFIG_DEFAULT_MLX_DIG_FILT     0U
#define CONFIG_DEFAULT_MLX_RES          0U
#define CONFIG_DEFAULT_MLX_TCMP_EN      0U

/* Default values - TMAG3001 */
#define CONFIG_DEFAULT_TMAG_GAIN_SEL    5U     /* 1.0x 增益 */
#define CONFIG_DEFAULT_TMAG_OSR         4U     /* 16x 平均 - 更高精度 */
#define CONFIG_DEFAULT_TMAG_RANGE       0U     /* ±40mT (A1) / ±120mT (A2) */
#define CONFIG_DEFAULT_TMAG_OP_MODE     2U     /* Continuous 连续测量模式 */
#define CONFIG_DEFAULT_TMAG_MAG_CH      7U     /* XYZ */
#define CONFIG_DEFAULT_TMAG_VERSION     0U     /* A1 version */

/* Public API */
void    Config_Init(void);
uint8_t Config_Save(const Config_TypeDef *cfg);
uint8_t Config_Load(Config_TypeDef *cfg);
void    Config_SetDefaults(Config_TypeDef *cfg);
uint8_t Config_SetDefaultsForProduct(Config_TypeDef *cfg, const char *product);
SensorType_t Config_SensorTypeForProduct(const char *product);
void    Config_RestoreDefault(void);

/* 传感器配置应用 - 根据传感器类型自动应用配置 */
uint8_t Config_ApplyToSensor(const Config_TypeDef *cfg, void *sensor_handle, uint8_t i2c_id);

#endif /* __CONFIG_MANAGER_H */
