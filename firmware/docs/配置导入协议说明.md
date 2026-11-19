# JSON 配置导入协议

## 概述

本文档描述下位机JSON配置导入系统的协议和使用方法。

## 配置文件格式

```json
{
  "sensor": {
    "gain": 7,          // 增益选择 (0-7, 默认7表示x1)
    "osr": 3,           // 过采样率 (0-3, 默认3表示x8)
    "filt": 0,          // 数字滤波器 (0-7, 默认0)
    "resx": 0,          // X轴分辨率 (0-3, 默认0)
    "resy": 0,          // Y轴分辨率 (0-3, 默认0)
    "resz": 0,          // Z轴分辨率 (0-3, 默认0)
    "tcmp": 0           // 温度补偿使能 (0/1, 默认0)
  },
  "algo": {
    "flag": "0x3B",     // 特性标志位 (十六进制或十进制, 默认0x3B)
    "xmap": 1.0,        // X轴映射系数 (浮点数, 默认1.0)
    "ymap": 1.0,        // Y轴映射系数 (浮点数, 默认1.0)
    "zmap": 1.0,        // Z轴映射系数 (浮点数, 默认1.0)
    "dead": 8.5,        // 死区倍数 (浮点数, 默认8.5)
    "spike": 20.0,      // 尖峰滤波倍数 (浮点数, 默认20.0)
    "stab": 0.1         // 稳定性阈值 uT (浮点数, 默认0.1)
  },
  "log": {
    "level": 3          // 日志等级 (0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG)
  }
}
```

## 导入协议

### 流程

1. **开始导入**
   ```
   Host → Device: ML IMPORT_START <total_bytes>
   Device → Host: READY
   ```

2. **发送数据**（分块发送，每块≤64字节）
   ```
   Host → Device: ML IMPORT_DATA <json_chunk>
   Device → Host: ACK_<bytes_received>
   ```
   重复此步骤直到所有数据发送完毕

3. **结束导入**
   ```
   Host → Device: ML IMPORT_END
   Device → Host: IMPORT_OK: config applied and saved
   ```
   或出错时：
   ```
   Device → Host: IMPORT_ERROR: <error_message>
   ```

4. **取消导入**（可选）
   ```
   Host → Device: ML IMPORT_CANCEL
   Device → Host: IMPORT_CANCELLED
   ```

### 示例会话

```
Host: ML IMPORT_START 312
Device: READY

Host: ML IMPORT_DATA {"sensor":{"gain":7,"osr":3,"filt":0,"resx":0,
Device: ACK_58

Host: ML IMPORT_DATA "resy":0,"resz":0,"tcmp":0},"algo":{"flag":"0x3B",
Device: ACK_116

Host: ML IMPORT_DATA "xmap":1.0,"ymap":1.0,"zmap":1.0,"dead":8.5,
Device: ACK_176

Host: ML IMPORT_DATA "spike":20.0,"stab":0.1},"log":{"level":3}}
Device: ACK_312

Host: ML IMPORT_END
Device: IMPORT_OK: config applied and saved
```

## 错误处理

### 错误类型

- **Buffer overflow**: 配置数据超过1024字节
  ```
  IMPORT_ERROR: size=2048 exceeds buffer=1024
  ```

- **Parse error**: JSON格式错误
  ```
  IMPORT_ERROR: parse failed at line 5, key=gain, status=1
  ```

- **Validation error**: 参数超出范围
  ```
  IMPORT_ERROR: validation failed (out of range)
  ```

- **Flash save error**: 保存到Flash失败
  ```
  IMPORT_ERROR: save to flash failed
  ```

### 错误恢复

出现错误后，发送 `ML IMPORT_CANCEL` 重置状态，然后重新开始导入流程。

## 参数范围

| 参数 | 类型 | 范围 | 默认值 |
|------|------|------|--------|
| sensor.gain | uint8 | 0-7 | 7 |
| sensor.osr | uint8 | 0-3 | 3 |
| sensor.filt | uint8 | 0-7 | 0 |
| sensor.resx/y/z | uint8 | 0-3 | 0 |
| sensor.tcmp | uint8 | 0-1 | 0 |
| algo.flag | uint8 | 0-255 | 0x3B |
| algo.xmap/ymap/zmap | float | any | 1.0 |
| algo.dead | float | any | 8.5 |
| algo.spike | float | any | 20.0 |
| algo.stab | float | any | 0.1 |
| log.level | uint8 | 0-4 | 3 |

## 应用效果

导入成功后，系统会：

1. 更新全局配置结构体 `g_config`
2. 应用到运行时变量（FLAG_RES、映射系数、算法参数）
3. 设置日志等级
4. 保存到Flash（断电保持）
5. 如果传感器正在运行，重启传感器以应用新的硬件配置

## 兼容性

### 键名兼容

支持以下两种键名格式：

- 短键名：`gain`, `osr`, `filt`, `resx`, `flag`, `xmap`, `dead`, `spike`, `stab`
- 长键名：`dig_filt`, `res_x`, `FLAG_RES`, `X_MAPPING_VALUE`, `dead_value`, `spike_multiplier`, `stability_threshold`

### 十六进制支持

`algo.flag` 字段支持：
- 十六进制字符串：`"0x3B"` 或 `"0X3B"`
- 十进制数字：`59`

## 实现细节

### 内存使用

- 导入缓冲区：1024字节（静态分配）
- JSON解析：零堆内存分配（单次遍历）
- 状态机：枚举类型（4字节）

### 解析器特性

- 容错性：跳过逗号、处理空白符
- 嵌套支持：sensor{}、algo{}、log{} 三层结构
- 类型自动识别：整数、浮点数、十六进制字符串
- 行号跟踪：错误时报告近似行号

### 安全性

1. 缓冲区溢出保护
2. 范围验证
3. Flash写入前验证
4. 看门狗兼容（分块处理）
