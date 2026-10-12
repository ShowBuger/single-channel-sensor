# 单通道磁传感器系统

基于 STM32F103C8T6 的单通道差分磁场测量系统。使用一对 MLX90393 传感器（实际传感器 + 参考传感器）进行差分测量，通过消除环境磁场干扰提高测量精度，结果通过串口输出。

---

## 目录

- [硬件规格](#硬件规格)
- [仓库结构](#仓库结构)
- [固件架构](#固件架构)
- [快速开始](#快速开始)
- [串口命令](#串口命令)
- [数据处理流水线](#数据处理流水线)
- [配置参数](#配置参数)
- [开发流程](#开发流程)
- [提交规范](#提交规范)

---

## 硬件规格

| 项目 | 规格 |
|------|------|
| MCU | STM32F103C8T6（Cortex-M3，72 MHz） |
| Flash | 64 KB |
| RAM | 20 KB |
| 传感器 | MLX90393 × 2（实际 + 参考，差分测量） |
| I2C | 软件模拟 I2C，PB6（SCL）/ PB7（SDA） |
| 串口 | USART1，PA9（TX）/ PA10（RX），115200 bps |
| 调试器 | ST-Link / J-Link（SWD 接口） |

---

## 仓库结构

```
single_channel_system/
├── firmware/                   # 固件代码
│   ├── src/                    # 应用源代码
│   │   ├── main.c              # 主程序、时钟初始化、命令调度
│   │   ├── stm32f10x_it.c      # 中断服务例程（SysTick / HAL_IncTick）
│   │   ├── stm32f1xx_hal_conf.h # HAL 库模块配置
│   │   ├── app/
│   │   │   ├── sensor_manager.c/h  # 传感器调度、数据处理流水线
│   │   │   └── config_manager.c/h  # Flash 配置读写
│   │   └── drivers/
│   │       ├── bsp/
│   │       │   ├── bsp_delay.c/h   # 延时（HAL_Delay + DWT 微秒）
│   │       │   ├── bsp_i2c_soft.c/h # 软件模拟 I2C
│   │       │   └── bsp_uart.c/h    # USART1 收发 + printf 重定向
│   │       └── sensors/
│   │           ├── mlx90393.c/h    # MLX90393 驱动（基于软件 I2C）
│   │           ├── tca9548a.c/h    # TCA9548A I2C 多路复用器驱动
│   │           └── tmag3001.c/h    # TMAG3001 驱动（预留）
│   ├── libs/
│   │   ├── STM32F1xx_HAL_Driver/  # ST HAL Driver v1.1.8
│   │   └── CMSIS/                 # CMSIS Core + STM32F1xx Device
│   ├── tools/                  # PowerShell 构建与烧录脚本
│   │   ├── install_toolchain.ps1  # 一键下载安装 ARM 工具链
│   │   ├── install_hal.ps1        # 一键下载安装 HAL 库
│   │   ├── build.ps1              # CMake 构建脚本
│   │   ├── flash.ps1              # 烧录脚本（OpenOCD / ST-Link / J-Link）
│   │   ├── env_setup.ps1          # 加载工具链到当前 PATH
│   │   └── _ensure_toolchain.ps1  # 构建前自动检查工具链（内部使用）
│   ├── CMakeLists.txt          # CMake 构建配置
│   └── STM32F103C8Tx_FLASH.ld  # 链接脚本
├── docs/
│   └── COMMIT_CONVENTION.md   # Git 提交规范
├── .vscode/
│   ├── tasks.json              # VS Code 构建任务（Ctrl+Shift+B）
│   ├── launch.json             # Cortex-Debug 调试配置（F5）
│   ├── c_cpp_properties.json   # IntelliSense 配置
│   └── extensions.json         # 推荐插件列表
└── .claude/
    └── agents/commit.md        # /commit skill：按规范生成 commit message
```

---

## 固件架构

```
main.c
  └── 初始化：HAL / 时钟 / DWT / 软件 I2C / UART / Flash 配置
  └── 主循环：串口命令处理 → 读传感器 → 数据处理 → 串口输出

app/sensor_manager.c            # 核心数据流
  ├── Scan_Ports()              # 检测传感器是否存在
  ├── sensor_start()            # 初始化数据结构 + 执行标定
  ├── get_sensor_data()         # 启动单次测量 → 等待 → 读取原始数据 → 物理量转换
  └── data_process()            # 数据处理流水线（见下节）

app/config_manager.c            # Flash 末页（0x0800FC00）存储配置
  ├── Config_Init()             # 解锁 Flash
  ├── Config_Save()             # 擦除 + 写入（含魔数和校验和）
  └── Config_Load()             # 读取并验证

drivers/bsp/                    # 硬件抽象层（全部基于 STM32 HAL）
drivers/sensors/                # 传感器驱动（全部基于软件 I2C）
```

---

## 快速开始

### 1. 克隆仓库

```powershell
git clone <repo-url>
cd single_channel_system/firmware
```

### 2. 安装 ARM 工具链（首次，约 1 GB）

```powershell
.\tools\install_toolchain.ps1
```

### 3. 下载 HAL 库（首次，约 2 MB）

```powershell
.\tools\install_hal.ps1
```

### 4. 构建固件

```powershell
# 加载工具链到当前终端（如果没有永久加入 PATH）
. .\tools\env_setup.ps1

# Debug 构建（默认）
.\tools\build.ps1

# Release 构建（体积更小，速度更快）
.\tools\build.ps1 -Release

# 清理后重新构建
.\tools\build.ps1 -Clean
```

构建产物在 `firmware/build/`：

| 文件 | 说明 |
|------|------|
| `single_channel_sensor.elf` | 含调试信息，GDB 使用 |
| `single_channel_sensor.hex` | Intel HEX，烧录用 |
| `single_channel_sensor.bin` | 纯二进制，烧录用 |
| `single_channel_sensor.map` | 内存占用分析 |

### 5. 烧录

```powershell
# OpenOCD + ST-Link（默认）
.\tools\flash.ps1

# ST-Link（stlink-tools）
.\tools\flash.ps1 -Method stlink

# J-Link
.\tools\flash.ps1 -Method jlink

# 跳过烧录后校验（更快）
.\tools\flash.ps1 -NoVerify
```

### 6. 调试

在 VS Code 中按 `F5`，选择 `Cortex Debug (OpenOCD/ST-Link)` 或 `Cortex Debug (J-Link)`。

---

## 串口命令

波特率：**115200 bps**，行尾：`\r` 或 `\n`

| 命令 | 说明 |
|------|------|
| `ML START` | 启动传感器（执行标定后开始连续采集） |
| `ML STOP` | 停止传感器采集 |
| `ML MERSURE` | 单次测量并输出结果 |
| `ML CALIB` | 重新执行标定（采集 10 次取均值和标准差） |
| `ML FLAG` | 查看当前 FLAG_RES 值 |
| `ML CHFLAG <值>` | 修改 FLAG_RES，支持十进制或 0x 十六进制，保存到 Flash |
| `ML MAPPING` | 查看当前 X/Y/Z 映射系数 |
| `ML SETMAP X <值>` | 设置 X 轴映射系数并保存到 Flash |
| `ML SETMAP Y <值>` | 设置 Y 轴映射系数并保存到 Flash |
| `ML SETMAP Z <值>` | 设置 Z 轴映射系数并保存到 Flash |

### 串口输出格式

```
sensor0:<X>, <Y>, <Z>
```

示例：
```
sensor0:0.12, -0.05, 0.33
```

---

## 数据处理流水线

`get_sensor_data()` 读取原始数据后，`data_process()` 按以下顺序处理：

```
原始数据（μT）
    │
    ▼ FLAG_CALIBRATION_ENABLE（Bit 0）
标定偏移补偿   减去静止时的均值，保留相对变化量
    │
    ▼ FLAG_PREDICTIVE_COMPENSATION（Bit 3）
预测补偿       用参考传感器的运动趋势（速度 + 加速度）预测并补偿实际传感器
    │
    ▼ FLAG_DEAD_BAND_ENABLE（Bit 1）
死区滤波       绝对值 < 8.5 × 标准差 时置零，抑制静止噪声
    │
    ▼ FLAG_SLIDING_WINDOW_FILTER（Bit 2）
滑动平均       窗口大小 5，平滑抖动
    │
    ▼ FLAG_MAPPING_ENABLE（Bit 4）
线性映射       乘以 X/Y/Z_MAPPING_VALUE 系数，换算输出单位
    │
    ▼ FLAG_SMOOTH_TRANSITION_ENABLE（Bit 5）
平滑过渡       连续 10 次数据 < 0.1 μT 时，指数衰减（×0.9）逐步归零
    │
    ▼
输出（串口打印）
```

---

## 配置参数

存储在 Flash 末页（`0x0800FC00`），掉电保持。

### FLAG_RES（默认 `0x3B` = `0011 1011`）

| 位 | 宏 | 默认 | 说明 |
|----|-----|------|------|
| 0 | `FLAG_CALIBRATION_ENABLE` | 1 | 标定偏移补偿 |
| 1 | `FLAG_DEAD_BAND_ENABLE` | 1 | 死区滤波 |
| 2 | `FLAG_SLIDING_WINDOW_FILTER` | 0 | 滑动平均滤波 |
| 3 | `FLAG_PREDICTIVE_COMPENSATION` | 1 | 预测补偿 |
| 4 | `FLAG_MAPPING_ENABLE` | 1 | 线性映射 |
| 5 | `FLAG_SMOOTH_TRANSITION_ENABLE` | 1 | 平滑过渡归零 |

### 映射系数（默认）

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `X_MAPPING_VALUE` | `0.1` | X 轴输出缩放系数 |
| `Y_MAPPING_VALUE` | `0.1` | Y 轴输出缩放系数 |
| `Z_MAPPING_VALUE` | `-0.1` | Z 轴输出缩放系数（负号翻转方向） |

---

## 开发流程

### 日常构建

```powershell
cd firmware

# 加载工具链（每次新开终端执行一次，或永久加入 PATH）
. .\tools\env_setup.ps1

# 构建
.\tools\build.ps1

# 烧录
.\tools\flash.ps1
```

### 修改传感器参数

1. 编辑 `src/drivers/sensors/mlx90393.c` 或 `mlx90393.h`
2. 重新构建烧录
3. 用串口发 `ML CALIB` 重新标定

### 修改处理算法

- 算法逻辑在 `src/app/sensor_manager.c`
- 开关通过 `FLAG_RES` 控制，可用 `ML CHFLAG` 命令实时调整，无需重新烧录

### 添加新串口命令

在 `src/main.c` 的 `serial_command_process()` 函数中仿照现有命令添加 `else if` 分支。

### VS Code 快捷键

| 操作 | 快捷键 |
|------|--------|
| 构建 | `Ctrl+Shift+B` |
| 调试（OpenOCD） | `F5` |
| 停止调试 | `Shift+F5` |
| 单步 | `F10` / `F11` |

---

## 提交规范

本项目使用规范化提交信息，详见 [docs/COMMIT_CONVENTION.md](docs/COMMIT_CONVENTION.md)。

如果安装了 Claude Code，可以使用 `/commit` skill 自动分析暂存内容并生成规范的 commit message：

```powershell
git add <files>
# 在 Claude Code 中输入 /commit
```

### 常用 type

`feat` `fix` `refactor` `docs` `build` `chore`

### 示例

```
fix(i2c): 修正 MLX90393 软件 I2C 时序

feat(filter): 添加尖峰滤波，阈值 20 倍标准差

build: 迁移到 STM32 HAL 库，移除 StdPeriph 依赖
```

---

**MCU**: STM32F103C8T6 | **工具链**: ARM GCC 15.3 | **构建**: CMake + Ninja | **库**: STM32F1xx HAL v1.1.8
