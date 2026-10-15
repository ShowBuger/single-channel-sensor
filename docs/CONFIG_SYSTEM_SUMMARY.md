# 配置系统完善总结

## 完成的工作

### 1. 统一配置结构 (config_manager.h/c)

**新增字段**:
- ✅ `sensor_type` - 传感器类型选择 (MLX90393/TMAG3001)
- ✅ `range` - TMAG3001量程配置
- ✅ `op_mode` - TMAG3001工作模式
- ✅ `mag_ch` - TMAG3001通道选择
- ✅ `sensor_version` - TMAG3001版本 (A1/A2)

**保留字段** (向后兼容):
- ✅ MLX90393特有配置 (`dig_filt`, `res_x/y/z`, `tcmp_en`)
- ✅ 通用配置 (`gain_sel`, `osr`)
- ✅ 算法参数 (死区、尖峰、稳定性阈值)
- ✅ 轴映射系数
- ✅ 产品信息

**新增功能**:
- ✅ `Config_ApplyToSensor()` - 根据传感器类型自动应用配置
- ✅ 编译时传感器类型选择 (`CURRENT_SENSOR_TYPE`)
- ✅ 字段缺省处理 - 未配置字段使用默认值

### 2. 独立配置解析器 (config_parser.h/c)

**核心功能**:
- ✅ `Config_ParseKeyValues()` - 批量解析键值对
- ✅ `Config_ParseSingleKV()` - 单个键值对解析
- ✅ `Config_Validate()` - 配置合法性验证
- ✅ `Config_ToString()` - 配置转字符串（调试）
- ✅ `Config_GetFieldDescription()` - 字段描述查询

**解耦设计**:
- ✅ 与Web界面完全解耦
- ✅ 支持多种输入格式（键值对、JSON等）
- ✅ 可独立用于串口命令、文件导入等场景

### 3. 传感器配置应用

**MLX90393配置应用**:
```c
#if CURRENT_SENSOR_TYPE == SENSOR_TYPE_MLX90393
    MLX90393_Config mlx_cfg = {
        .gain_sel = cfg->gain_sel,
        .osr = cfg->osr,
        .dig_filt = cfg->dig_filt,
        .res_x/y/z = cfg->res_x/y/z,
        .tcmp_en = cfg->tcmp_en
    };
    MLX90393_configureAll(dev, i2c_id, &mlx_cfg);
#endif
```

**TMAG3001配置应用**:
```c
#if CURRENT_SENSOR_TYPE == SENSOR_TYPE_TMAG3001
    TMAG3001_SetMagGain(dev, cfg->gain_sel);
    TMAG3001_SetConvAvg(dev, cfg->osr);
    TMAG3001_SetRange(dev, cfg->range);
    TMAG3001_SetOperatingMode(dev, cfg->op_mode);
    TMAG3001_SetMagChannels(dev, cfg->mag_ch);
#endif
```

### 4. Flash存储映射文档

创建了详细的 `FLASH_CONFIG_MAP.md`，包含:
- ✅ Flash布局说明
- ✅ 所有字段的偏移量、类型、大小、默认值
- ✅ 每个字段的详细说明和取值范围
- ✅ 配置键值对映射表
- ✅ 配置应用流程图
- ✅ 示例配置文件

## Flash配置映射概览

### 存储信息
- **Flash地址**: `CONFIG_FLASH_PAGE_ADDR` (通常 0x0800FC00)
- **页大小**: 1KB (1024字节)
- **实际使用**: 76字节 (约7.4%)
- **剩余空间**: 948字节

### 配置字段分组

| 分组 | 大小 | 字段数 |
|------|------|--------|
| 元数据 | 4字节 | schema_ver, sensor_type, FLAG_RES |
| 通用传感器配置 | 2字节 | gain_sel, osr |
| MLX90393特有 | 5字节 | dig_filt, res_x/y/z, tcmp_en |
| TMAG3001特有 | 4字节 | range, op_mode, mag_ch, sensor_version |
| 系统配置 | 1字节 | log_level |
| 轴映射系数 | 12字节 | X/Y/Z_MAPPING_VALUE |
| 算法参数 | 12字节 | dead_value, spike_multiplier, stability_threshold |
| 产品信息 | 8字节 | product_model |
| 完整性 | 5字节 | padding, checksum |
| **总计** | **53字节** | - |
| 加魔数 | **57字节** | StoredConfig_TypeDef |

### 完整配置字段列表

#### 传感器通用配置
| 字段 | 类型 | MLX默认 | TMAG默认 | 说明 |
|------|------|---------|----------|------|
| sensor_type | uint8_t | 0 | 1 | 0=MLX90393, 1=TMAG3001 |
| gain_sel | uint8_t | 7 | 5 | 增益选择 |
| osr | uint8_t | 2 | 2 | 过采样/平均次数 |

#### MLX90393特有配置
| 字段 | 类型 | 默认值 | 范围 | 说明 |
|------|------|--------|------|------|
| dig_filt | uint8_t | 0 | 0-7 | 数字滤波器级数 |
| res_x | uint8_t | 0 | 0-3 | X轴分辨率 |
| res_y | uint8_t | 0 | 0-3 | Y轴分辨率 |
| res_z | uint8_t | 0 | 0-3 | Z轴分辨率 |
| tcmp_en | uint8_t | 0 | 0/1 | 温度补偿使能 |

#### TMAG3001特有配置
| 字段 | 类型 | 默认值 | 范围 | 说明 |
|------|------|--------|------|------|
| range | uint8_t | 0 | 0/1 | 量程选择 (A1: 40/80mT, A2: 120/240mT) |
| op_mode | uint8_t | 0 | 0-3 | 工作模式 (0=Standby, 1=Sleep, 2=Continuous, 3=Wakeup) |
| mag_ch | uint8_t | 7 | 0-7 | 磁场通道 (7=XYZ) |
| sensor_version | uint8_t | 0 | 0/1 | 传感器版本 (0=A1, 1=A2) |

#### 算法配置
| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| FLAG_RES | uint8_t | 0x7B | 算法功能标志位掩码 |
| X_MAPPING_VALUE | float | 0.1 | X轴映射系数 |
| Y_MAPPING_VALUE | float | 0.1 | Y轴映射系数 |
| Z_MAPPING_VALUE | float | -0.1 | Z轴映射系数 |
| dead_value | float | 8.5 | 死区倍数（×标准差） |
| spike_multiplier | float | 20.0 | 尖峰滤波倍数（×标准差） |
| stability_threshold | float | 0.1 | 稳定性阈值 (μT) |

#### 系统配置
| 字段 | 类型 | 默认值 | 范围 | 说明 |
|------|------|--------|------|------|
| log_level | uint8_t | 3 | 0-4 | 日志级别 (0=无, 1=错误, 2=警告, 3=信息, 4=调试) |
| product_model | char[8] | "MA01" | - | 产品型号字符串 |

## 配置键值对映射

| Web/串口键名 | 配置字段 | 适用传感器 |
|--------------|----------|------------|
| sensor.type | sensor_type | 通用 |
| sensor.gain | gain_sel | 通用 |
| sensor.osr | osr | 通用 |
| sensor.filt | dig_filt | 仅MLX90393 |
| sensor.resx | res_x | 仅MLX90393 |
| sensor.resy | res_y | 仅MLX90393 |
| sensor.resz | res_z | 仅MLX90393 |
| sensor.tcmp | tcmp_en | 仅MLX90393 |
| sensor.range | range | 仅TMAG3001 |
| sensor.opmode | op_mode | 仅TMAG3001 |
| sensor.magch | mag_ch | 仅TMAG3001 |
| sensor.version | sensor_version | 仅TMAG3001 |
| algo.flag | FLAG_RES | 通用 |
| algo.xmap | X_MAPPING_VALUE | 通用 |
| algo.ymap | Y_MAPPING_VALUE | 通用 |
| algo.zmap | Z_MAPPING_VALUE | 通用 |
| algo.dead | dead_value | 通用 |
| algo.spike | spike_multiplier | 通用 |
| algo.stab | stability_threshold | 通用 |
| log.level | log_level | 通用 |
| product | product_model | 通用 |

## 使用示例

### 1. 编译时选择传感器

```c
// config_manager.h
#define CURRENT_SENSOR_TYPE  SENSOR_TYPE_TMAG3001  // 或 SENSOR_TYPE_MLX90393
```

### 2. 串口命令配置

```
ML SET sensor.gain 5        // 设置增益
ML SET sensor.osr 3         // 设置过采样
ML SET sensor.range 1       // 设置量程（仅TMAG3001）
ML SET algo.flag 0x7B       // 设置算法标志
ML SAVECFG                  // 保存到Flash
ML APPLYCFG                 // 应用配置到传感器
```

### 3. 程序内使用

```c
// 加载配置
Config_Load(&g_config);

// 解析键值对
ConfigKeyValue_t kvs[] = {
    {"sensor.gain", "5"},
    {"sensor.osr", "3"}
};
Config_ParseKeyValues(&g_config, kvs, 2);

// 验证配置
if (Config_Validate(&g_config) == 0) {
    // 保存到Flash
    Config_Save(&g_config);
    
    // 应用到传感器
    Config_ApplyToSensor(&g_config, &sensor_handle, 0);
}
```

### 4. 查询配置

```c
char buf[256];
Config_ToString(&g_config, buf, sizeof(buf));
Serial_Printf("%s", buf);

// 输出示例:
// Config v5 [TMAG3001]
// Product: MA01
// Sensor: gain=5 osr=2 range=0 mode=0 ch=7 ver=0
// Algo: flag=0x7B map=0.100/0.100/-0.100 dead=8.5 spike=20.0 stab=0.100
// Log: level=3
```

## 字段缺省处理示例

```c
// 情况1: 仅配置部分字段
ConfigKeyValue_t kvs[] = {
    {"sensor.gain", "10"}  // 仅设置增益
};
Config_ParseKeyValues(&cfg, kvs, 1);
// 结果: gain=10, 其他字段使用默认值

// 情况2: 传感器切换时忽略不适用字段
ConfigKeyValue_t kvs[] = {
    {"sensor.gain", "5"},
    {"sensor.range", "1"},  // TMAG3001字段
    {"sensor.filt", "3"}    // MLX90393字段
};
Config_ParseKeyValues(&cfg, kvs, 3);
// 使用TMAG3001时: gain=5, range=1, dig_filt被忽略
// 使用MLX90393时: gain=5, dig_filt=3, range被忽略
```

## 文件清单

| 文件 | 说明 | 状态 |
|------|------|------|
| firmware/src/app/config_manager.h | 配置管理头文件 | ✅ 已更新 |
| firmware/src/app/config_manager.c | 配置管理实现 | ✅ 已更新 |
| firmware/src/app/config_parser.h | 配置解析器头文件 | ✅ 新建 |
| firmware/src/app/config_parser.c | 配置解析器实现 | ✅ 新建 |
| firmware/src/app/sensor_manager.c | 传感器管理器 | ✅ 已更新 |
| docs/FLASH_CONFIG_MAP.md | Flash配置映射文档 | ✅ 新建 |
| docs/CONFIG_SYSTEM_SUMMARY.md | 本文档 | ✅ 新建 |

## 技术亮点

1. **传感器无关设计** - 通过宏和条件编译支持多种传感器
2. **字段缺省机制** - 未配置字段自动填充默认值
3. **配置验证** - 自动检查字段合法性和范围
4. **解耦架构** - 配置解析独立于Web和串口
5. **向前兼容** - Schema版本管理，老配置可识别
6. **完整性保护** - 魔数、版本、校验和三重验证

## 下一步建议

1. ✅ 完成 - 基础配置系统
2. ✅ 完成 - 配置解析器
3. ✅ 完成 - Flash映射文档
4. 🔄 待完成 - Web界面适配新配置系统
5. 🔄 待完成 - 配置导入功能测试
6. 🔄 待完成 - 硬件验证（实际传感器测试）

---
**版本**: 1.0  
**日期**: 2025-10-15  
**作者**: Claude Opus 5.5
