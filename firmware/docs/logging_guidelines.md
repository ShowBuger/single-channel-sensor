# 日志系统使用规范

## 概述

本项目使用统一的日志系统 (`log_manager.h`) 来管理调试和诊断输出。

## 使用规则

### ✅ 使用日志系统的场景

**所有驱动、传感器、业务逻辑的调试和状态信息都必须使用日志系统：**

1. **传感器驱动** (`src/drivers/sensors/`)
   - 初始化状态
   - 配置信息
   - 错误和警告
   - 数据读取状态

2. **BSP驱动** (`src/drivers/bsp/`)
   - I2C 总线扫描
   - 外设初始化状态
   - 硬件错误信息

3. **应用层** (`src/app/`)
   - 传感器管理状态
   - 校准过程
   - 配置加载/保存
   - 算法处理信息

### ❌ 不使用日志系统的场景

**上位机通信协议必须使用 `Serial_Printf`：**

1. **命令响应** (`main.c`)
   - `ML START` → `"Sensor started\r\n"`
   - `ML GETCFG` → JSON配置数据
   - `ML CHFLAG` → 参数确认
   - 错误提示：`"Unknown: %s\r\n"`

2. **配置导入协议** (`config_import.c`)
   - `READY` - 准备接收
   - `ACK_<bytes>` - 接收确认
   - `IMPORT_OK` / `IMPORT_ERROR` - 导入结果
   - `IMPORT_CANCELLED` - 取消确认

**原因：** 这些是与上位机的协议通信，必须保持固定格式，不能添加日志等级前缀。

## 日志等级

```c
typedef enum {
    LOG_NONE  = 0,  // 关闭所有日志
    LOG_ERROR = 1,  // 错误（硬件故障、致命错误）
    LOG_WARN  = 2,  // 警告（配置错误、可恢复的问题）
    LOG_INFO  = 3,  // 信息（状态变化、重要事件）
    LOG_DEBUG = 4   // 调试（详细诊断信息）
} LogLevel_t;
```

### 使用建议

- **LOG_E**: 硬件通信失败、Flash 写入失败、传感器无响应
- **LOG_W**: 校准失败、参数超出范围、配置回退到默认值
- **LOG_I**: 系统启动、传感器就绪、配置加载成功、校准完成
- **LOG_D**: 寄存器读写、I2C 地址扫描、详细的状态转换

## 使用示例

### ✅ 正确示例

```c
// sensor_manager.c
LOG_I("SCAN", "Sensor ready for measurement.");
LOG_W("CALIB", "Calibration skipped: sensor not running");
LOG_D("SCAN", "[scan] actual begin -> %s", r_actual == 0 ? "OK" : "FAIL");

// mlx90393.c
LOG_D("MLX", "[begin] addr=0x%02X A1=%d A0=%d", handle->I2C_Address, A1, A0);
LOG_W("MLX", "[begin] exit_mode FAIL");
LOG_I("MLX", "[begin] OK");

// bsp_i2c_soft.c
LOG_I("I2C", "Scanning I2C bus...");
LOG_I("I2C", "Found device at 0x%02X", addr);
LOG_W("I2C", "No I2C devices found.");
```

### ❌ 错误示例

```c
// ❌ 不要在驱动代码中使用 Serial_Printf
Serial_Printf("Sensor initialized\r\n");  // 应该用 LOG_I

// ❌ 不要在协议响应中使用日志宏
LOG_I("CMD", "READY");  // 应该用 Serial_Printf("READY\r\n")
```

## 日志格式

日志输出格式：
```
[LEVEL][TAG] message
```

示例：
```
[INFO][SCAN] Sensor ready for measurement.
[WARN][CALIB] Calibration skipped: sensor not running
[DEBUG][MLX] [begin] addr=0x0C A1=0 A0=0
```

## 运行时控制

### 通过串口命令修改日志等级

```
ML LOGLEVEL 4       # 设置为 DEBUG 等级
ML LOGLEVEL 3       # 设置为 INFO 等级
ML LOGLEVEL 2       # 设置为 WARN 等级
ML LOGLEVEL 1       # 设置为 ERROR 等级
ML LOGLEVEL 0       # 关闭所有日志
ML LOGLEVEL         # 查询当前日志等级
```

### 通过 JSON 配置修改

```json
{
  "log": {
    "level": 3
  }
}
```

## 编译时优化

可以通过 `LOG_COMPILE_LEVEL` 宏在编译时剔除低优先级日志：

```c
// log_manager.h
#define LOG_COMPILE_LEVEL LOG_INFO  // 编译时剔除所有 DEBUG 日志
```

在 `-O1` 及以上优化等级下，被剔除的日志不会产生代码。

## 当前状态

### ✅ 已正确使用日志系统
- `sensor_manager.c` - 31个日志调用
- `mlx90393.c` - 11个日志调用  
- `bsp_i2c_soft.c` - 4个日志调用（已修复）
- `bsp_i2c.c` - 3个日志调用（已修复）

### ✅ 正确使用 Serial_Printf（协议通信）
- `main.c` - 命令响应协议
- `config_import.c` - 配置导入协议
- `bsp_uart.c` - Serial_Printf 实现

## 最佳实践

1. **选择合适的日志等级** - 根据信息重要性选择
2. **使用描述性的 TAG** - 模块名或功能名（3-6个字符）
3. **日志信息简洁明确** - 说明发生了什么和关键参数
4. **避免日志风暴** - 不要在高频循环中使用 LOG_I/LOG_D
5. **协议通信不用日志** - 保持协议格式的纯净性
