# Flash配置映射文档

## 概述

固件配置存储在STM32F103的Flash中的最后一页，支持掉电保存。配置系统设计为：
- **独立于Web** - 配置解析与Web界面解耦
- **传感器无关** - 同时支持MLX90393和TMAG3001
- **字段缺省** - 自动填充缺失字段的默认值
- **版本管理** - Schema版本控制，向前兼容

## Flash存储布局

### 存储地址
- **基地址**: 由`board_config.h`中的`CONFIG_FLASH_PAGE_ADDR`定义
- **大小**: 1KB (STM32F103的一页Flash)
- **STM32F103C8T6**: 通常为`0x0800FC00` (64KB Flash的最后一页)

### 存储结构

```c
typedef struct {
    uint32_t       magic;         // 魔数: 0x52454C56 ("RELV")
    Config_TypeDef config;        // 配置数据 (~72字节)
} StoredConfig_TypeDef;
```

**总大小**: ~76字节 (远小于1KB页大小)

## 配置字段映射表

### 1. 元数据字段 (4字节)

| 偏移 | 字段 | 类型 | 大小 | 说明 | 默认值 |
|------|------|------|------|------|--------|
| 0 | schema_ver | uint8_t | 1 | Schema版本号 | 5 |
| 1 | sensor_type | uint8_t | 1 | 传感器类型 (0=MLX90393, 1=TMAG3001) | 1 (TMAG3001) |
| 2 | FLAG_RES | uint8_t | 1 | 算法功能标志位掩码 | 0x7B |
| 3 | _pad1 | uint8_t | 1 | 对齐填充 | 0 |

**FLAG_RES位定义**:
- Bit 0: 标定使能
- Bit 1: 死区过滤使能
- Bit 2: 滑动窗口滤波使能
- Bit 3: 预测补偿使能
- Bit 4: 映射使能
- Bit 5: 平滑过渡使能
- Bit 6: Z轴正值过滤使能

### 2. 通用传感器配置 (2字节)

| 偏移 | 字段 | 类型 | 大小 | 说明 | MLX默认 | TMAG默认 |
|------|------|------|------|------|---------|----------|
| 4 | gain_sel | uint8_t | 1 | 增益选择 | 7 (x1) | 5 (1.0x) |
| 5 | osr | uint8_t | 1 | 过采样/平均次数 | 2 (x4) | 2 (4x) |

**gain_sel详细说明**:
- **MLX90393**: 0-7，值越大增益越低（7为x1最低增益）
- **TMAG3001**: 0-15，0=0.5x, 5=1.0x, 15=2.0x（步进0.1x）

**osr详细说明**:
- **MLX90393**: 0=×1, 1=×2, 2=×4, 3=×8
- **TMAG3001**: 0=1x, 1=2x, 2=4x, 3=8x, 4=16x, 5=32x

### 3. MLX90393特有配置 (5字节)

| 偏移 | 字段 | 类型 | 大小 | 说明 | 范围 | 默认值 |
|------|------|------|------|------|------|--------|
| 6 | dig_filt | uint8_t | 1 | 数字滤波器级数 | 0-7 | 0 |
| 7 | res_x | uint8_t | 1 | X轴分辨率 | 0-3 | 0 |
| 8 | res_y | uint8_t | 1 | Y轴分辨率 | 0-3 | 0 |
| 9 | res_z | uint8_t | 1 | Z轴分辨率 | 0-3 | 0 |
| 10 | tcmp_en | uint8_t | 1 | 温度补偿使能 | 0/1 | 0 |

*注：使用TMAG3001时这些字段无效*

### 4. TMAG3001特有配置 (4字节)

| 偏移 | 字段 | 类型 | 大小 | 说明 | 范围 | 默认值 |
|------|------|------|------|------|------|--------|
| 11 | range | uint8_t | 1 | 量程选择 | 0=小量程, 1=大量程 | 0 |
| 12 | op_mode | uint8_t | 1 | 工作模式 | 0-3 | 0 (Standby) |
| 13 | mag_ch | uint8_t | 1 | 磁场通道选择 | 0-7 | 7 (XYZ) |
| 14 | sensor_version | uint8_t | 1 | 传感器版本 | 0=A1, 1=A2 | 0 (A1) |

**range详细说明**:
- A1版本: 0=±40mT, 1=±80mT
- A2版本: 0=±120mT, 1=±240mT

**op_mode详细说明**:
- 0: Standby (待机)
- 1: Sleep (睡眠)
- 2: Continuous (连续测量)
- 3: Wakeup-and-Sleep (唤醒-睡眠)

**mag_ch详细说明**:
- 0: None, 1: X, 2: Y, 3: XY, 4: Z, 5: XZ, 6: YZ, 7: XYZ

*注：使用MLX90393时这些字段无效*

### 5. 系统配置 (1字节)

| 偏移 | 字段 | 类型 | 大小 | 说明 | 范围 | 默认值 |
|------|------|------|------|------|------|--------|
| 15 | log_level | uint8_t | 1 | 日志输出级别 | 0-4 | 3 |

**log_level详细说明**:
- 0: NONE (无日志)
- 1: ERROR (仅错误)
- 2: WARNING (警告及以上)
- 3: INFO (信息及以上)
- 4: DEBUG (所有日志)

### 6. 轴映射系数 (12字节)

| 偏移 | 字段 | 类型 | 大小 | 说明 | 默认值 |
|------|------|------|------|------|--------|
| 16 | X_MAPPING_VALUE | float | 4 | X轴映射系数 | 0.1 |
| 20 | Y_MAPPING_VALUE | float | 4 | Y轴映射系数 | 0.1 |
| 24 | Z_MAPPING_VALUE | float | 4 | Z轴映射系数 | -0.1 |

**说明**: 用于将传感器原始数据映射到应用层单位

### 7. 算法参数 (12字节)

| 偏移 | 字段 | 类型 | 大小 | 说明 | 默认值 |
|------|------|------|------|------|--------|
| 28 | dead_value | float | 4 | 死区倍数（×标准差） | 8.5 |
| 32 | spike_multiplier | float | 4 | 尖峰滤波倍数（×标准差） | 20.0 |
| 36 | stability_threshold | float | 4 | 稳定性阈值 (μT) | 0.1 |

**算法参数说明**:
- **dead_value**: 低于此倍数标准差的数据置零
- **spike_multiplier**: 超过此倍数标准差视为尖峰
- **stability_threshold**: 稳定性判断的绝对阈值

### 8. 产品信息 (8字节)

| 偏移 | 字段 | 类型 | 大小 | 说明 | 默认值 |
|------|------|------|------|------|--------|
| 40 | product_model | char[8] | 8 | 产品型号字符串 | "MA01" |

**说明**: 以'\0'结尾的字符串，最多7个字符

### 9. 对齐和校验 (5字节)

| 偏移 | 字段 | 类型 | 大小 | 说明 |
|------|------|------|------|------|
| 48 | _pad | uint8_t[1] | 1 | 4字节对齐填充 |
| 49 | checksum | uint32_t | 4 | 配置校验和 |

**checksum计算**: 所有字段（除checksum自身）的字节和

## 配置键值对映射

### Web/串口命令键名

| 配置键 | 对应字段 | 类型 | 示例值 |
|--------|----------|------|--------|
| sensor.type | sensor_type | 整数/字符串 | "TMAG3001" 或 1 |
| sensor.gain | gain_sel | 整数 | 5 |
| sensor.osr | osr | 整数 | 2 |
| sensor.filt | dig_filt | 整数 | 0 (仅MLX) |
| sensor.resx | res_x | 整数 | 0 (仅MLX) |
| sensor.resy | res_y | 整数 | 0 (仅MLX) |
| sensor.resz | res_z | 整数 | 0 (仅MLX) |
| sensor.tcmp | tcmp_en | 整数 | 0 (仅MLX) |
| sensor.range | range | 整数 | 0 (仅TMAG) |
| sensor.opmode | op_mode | 整数 | 0 (仅TMAG) |
| sensor.magch | mag_ch | 整数 | 7 (仅TMAG) |
| sensor.version | sensor_version | 整数 | 0 (仅TMAG) |
| algo.flag | FLAG_RES | 十六进制/整数 | "0x7B" 或 123 |
| algo.xmap | X_MAPPING_VALUE | 浮点 | 0.1 |
| algo.ymap | Y_MAPPING_VALUE | 浮点 | 0.1 |
| algo.zmap | Z_MAPPING_VALUE | 浮点 | -0.1 |
| algo.dead | dead_value | 浮点 | 8.5 |
| algo.spike | spike_multiplier | 浮点 | 20.0 |
| algo.stab | stability_threshold | 浮点 | 0.1 |
| log.level | log_level | 整数 | 3 |
| product | product_model | 字符串 | "MA01" |

## 配置应用流程

### 1. 启动时加载

```
系统启动
    ↓
Config_Init() - 解锁Flash
    ↓
Config_Load(&g_config) - 从Flash加载
    ↓
校验魔数、版本、校验和
    ↓
  合法？
  ├─ 是 → 使用加载的配置
  └─ 否 → 使用默认值
    ↓
应用到全局变量
    ↓
Scan_Ports() - 初始化传感器
    ↓
Config_ApplyToSensor() - 应用配置到传感器硬件
```

### 2. 运行时修改

```
接收串口命令 (如 "ML SET sensor.gain 5")
    ↓
解析键值对
    ↓
Config_ParseSingleKV() - 更新内存配置
    ↓
用户执行 "ML SAVECFG"
    ↓
Config_Save() - 写入Flash
    ↓
用户执行 "ML APPLYCFG"
    ↓
Config_ApplyToSensor() - 应用到传感器
```

### 3. Web导入配置

```
Web发送 "ML IMPORT_BEGIN"
    ↓
Web发送配置键值对
    ↓
Config_ParseSingleKV() - 逐个解析
    ↓
Web发送 "ML IMPORT_END"
    ↓
自动调用 Config_Save() - 保存到Flash
    ↓
自动调用 Config_ApplyToSensor() - 应用配置
```

## 字段缺省处理

配置解析器支持字段缺省，未提供的字段使用默认值：

```c
// 示例：仅配置增益，其他使用默认值
ConfigKeyValue_t kvs[] = {
    {"sensor.gain", "5"}
};
Config_ParseKeyValues(&cfg, kvs, 1);
// 结果：gain=5, 其他字段=默认值
```

## 传感器类型切换

通过宏`CURRENT_SENSOR_TYPE`在编译时选择传感器类型：

```c
// config_manager.h
#define CURRENT_SENSOR_TYPE  SENSOR_TYPE_TMAG3001  // 或 SENSOR_TYPE_MLX90393
```

配置系统自动处理：
1. **字段验证** - 仅验证当前传感器类型的字段
2. **配置应用** - 仅应用当前传感器支持的配置
3. **默认值** - 根据传感器类型选择不同默认值

## API函数说明

### 配置管理 (config_manager.h/c)

- `Config_Init()` - 初始化Flash访问
- `Config_Load(cfg)` - 从Flash加载配置
- `Config_Save(cfg)` - 保存配置到Flash
- `Config_SetDefaults(cfg)` - 设置默认值
- `Config_RestoreDefault()` - 恢复出厂设置
- `Config_ApplyToSensor(cfg, handle, i2c_id)` - 应用配置到传感器

### 配置解析 (config_parser.h/c)

- `Config_ParseKeyValues(cfg, kvs, count)` - 批量解析键值对
- `Config_ParseSingleKV(cfg, key, value)` - 解析单个键值对
- `Config_Validate(cfg)` - 验证配置合法性
- `Config_ToString(cfg, buf, size)` - 转为字符串（调试用）
- `Config_GetFieldDescription(key, desc, size)` - 获取字段描述

## Flash操作注意事项

1. **写入次数限制**: Flash典型擦写次数10,000次
2. **页擦除**: 每次保存先擦除整页（1KB）
3. **写入单位**: 按字（4字节）写入
4. **掉电保护**: 通过魔数和校验和保证数据完整性
5. **版本兼容**: Schema版本不匹配时使用默认值

## 配置大小汇总

| 部分 | 大小 | 说明 |
|------|------|------|
| 元数据 | 4字节 | 版本、类型、标志 |
| 通用传感器配置 | 2字节 | 增益、过采样 |
| MLX90393特有 | 5字节 | 滤波、分辨率、温补 |
| TMAG3001特有 | 4字节 | 量程、模式、通道 |
| 系统配置 | 1字节 | 日志级别 |
| 轴映射系数 | 12字节 | X/Y/Z映射 |
| 算法参数 | 12字节 | 死区、尖峰、稳定性 |
| 产品信息 | 8字节 | 型号字符串 |
| 对齐和校验 | 5字节 | 填充、校验和 |
| **Config_TypeDef总计** | **53字节** | 配置数据 |
| 加上魔数 | 4字节 | 验证标识 |
| **StoredConfig_TypeDef总计** | **57字节** | Flash存储 |
| Flash页大小 | 1024字节 | 剩余967字节 |

## 示例配置文件

### TMAG3001完整配置

```json
{
  "sensor": {
    "type": "TMAG3001",
    "gain": 5,
    "osr": 2,
    "range": 0,
    "opmode": 0,
    "magch": 7,
    "version": 0
  },
  "algo": {
    "flag": "0x7B",
    "xmap": 0.1,
    "ymap": 0.1,
    "zmap": -0.1,
    "dead": 8.5,
    "spike": 20.0,
    "stab": 0.1
  },
  "log": {
    "level": 3
  },
  "product": "MA01"
}
```

### MLX90393完整配置

```json
{
  "sensor": {
    "type": "MLX90393",
    "gain": 7,
    "osr": 2,
    "filt": 0,
    "resx": 0,
    "resy": 0,
    "resz": 0,
    "tcmp": 0
  },
  "algo": {
    "flag": "0x3B",
    "xmap": 0.1,
    "ymap": 0.1,
    "zmap": -0.1,
    "dead": 8.5,
    "spike": 20.0,
    "stab": 0.1
  },
  "log": {
    "level": 3
  },
  "product": "MA01"
}
```

---

**文档版本**: 1.0  
**对应Schema版本**: 5  
**最后更新**: 2025-10-15
