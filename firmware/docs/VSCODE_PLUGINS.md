# VSCode插件方案：一键配置STM32开发环境

## 🎯 方案对比

| 方案 | 优点 | 缺点 | 推荐度 |
|------|------|------|--------|
| **PlatformIO IDE** | ✅ 自动安装工具链<br>✅ 统一管理依赖<br>✅ 跨平台<br>✅ 丰富的库管理 | ⚠️ 学习曲线<br>⚠️ 需要迁移项目结构 | ⭐⭐⭐⭐⭐ |
| **STM32 VS Code Extension** | ✅ ST官方支持<br>✅ 与STM32CubeMX集成<br>✅ 保持现有结构 | ⚠️ 仍需手动安装工具链 | ⭐⭐⭐⭐ |
| **Embedded Tools** | ✅ 自动下载工具链<br>✅ 轻量级 | ⚠️ 功能相对简单 | ⭐⭐⭐ |
| **当前Makefile方案** | ✅ 完全控制<br>✅ 无依赖 | ⚠️ 需手动安装工具 | ⭐⭐⭐ |

---

## 方案1: PlatformIO IDE (强烈推荐) 🌟

### 特点
- **自动安装工具链**: 无需手动下载ARM GCC、OpenOCD等
- **统一包管理**: 类似npm/pip的库管理系统
- **内置调试**: 开箱即用的调试功能
- **跨平台**: Windows/Linux/macOS完全一致的体验

### 安装步骤

#### 1. 安装PlatformIO扩展
```
VSCode扩展市场搜索: PlatformIO IDE
或直接安装: platformio.platformio-ide
```

#### 2. 创建PlatformIO配置

在项目根目录创建 `platformio.ini`:

```ini
[env:genericSTM32F103C8]
platform = ststm32
board = genericSTM32F103C8
framework = stm32cube

; 构建选项
build_flags = 
    -DUSE_STDPERIPH_DRIVER
    -DSTM32F10X_MD
    -Wl,-Map,output.map
    -mcpu=cortex-m3
    -mthumb

; 包含路径
build_unflags = 

; 上传设置
upload_protocol = stlink
debug_tool = stlink

; 串口监视器
monitor_speed = 115200
monitor_port = COM3

; 自定义源文件位置
src_dir = firmware/src
lib_dir = firmware/libs
include_dir = firmware/src

; 额外脚本
;extra_scripts = pre:script.py
```

#### 3. 调整项目结构（可选）

PlatformIO默认结构:
```
project/
├── platformio.ini
├── src/              # 源代码
├── lib/              # 库文件
├── include/          # 头文件
└── test/             # 测试
```

**选项A**: 迁移到PlatformIO结构（推荐）
```bash
# 创建符号链接，保持兼容
mklink /D src firmware\src
mklink /D lib firmware\libs
```

**选项B**: 使用自定义路径（保持现有结构）
```ini
; 在platformio.ini中指定
src_dir = firmware/src
lib_dir = firmware/libs
```

#### 4. 开始使用

- **编译**: PlatformIO侧边栏 → Build
- **上传**: PlatformIO侧边栏 → Upload  
- **调试**: PlatformIO侧边栏 → Debug
- **串口监视器**: PlatformIO侧边栏 → Serial Monitor

**快捷键**:
- `Ctrl+Alt+B` - 编译
- `Ctrl+Alt+U` - 上传
- `F5` - 调试

---

## 方案2: STM32 VS Code Extension (官方方案)

### 特点
- ST官方维护
- 与STM32CubeMX无缝集成
- 支持STM32CubeIDE项目导入

### 安装步骤

#### 1. 安装扩展
```
搜索: STM32 VS Code Extension
ID: stmicroelectronics.stm32-vscode-extension
```

#### 2. 安装必需工具（仍需手动）
扩展会提示下载：
- ✅ STM32CubeMX
- ✅ STM32CubeProgrammer  
- ⚠️ ARM工具链（需单独安装）

#### 3. 导入现有项目
```
命令面板 (Ctrl+Shift+P):
> STM32: Import STM32CubeMX Project
```

或创建新项目：
```
> STM32: Create Project from STM32CubeMX
```

---

## 方案3: Embedded Tools Extension

### 安装
```
搜索: Embedded Tools
或: WebFreak.embeddedc
```

### 特点
- 自动下载和管理ARM工具链
- 支持多种调试器
- 集成串口终端

### 配置示例
```json
{
    "embedded.toolchain.path": "auto",
    "embedded.debugger": "openocd",
    "embedded.openocd.interface": "stlink",
    "embedded.openocd.target": "stm32f1x"
}
```

---

## 方案4: 当前Makefile + 辅助插件

### 推荐插件组合

#### 核心开发
```json
{
    "recommendations": [
        "ms-vscode.cpptools",           // C/C++ IntelliSense
        "ms-vscode.makefile-tools",     // Makefile支持
        "marus25.cortex-debug",         // ARM调试
        "dan-c-underwood.arm"           // ARM汇编
    ]
}
```

#### 增强功能
```json
{
    "recommendations": [
        "zixuanwang.linkerscript",      // .ld文件语法高亮
        "ms-vscode.hexeditor",          // 十六进制编辑
        "keroc.hex-fmt",                // Intel HEX查看
        "analogue.devicetree",          // 设备树支持
        "twxs.cmake"                    // CMake支持
    ]
}
```

#### 工具链管理（自动安装）
```json
{
    "recommendations": [
        "webfreak.embeddedc",           // 嵌入式C工具
        "metalcode-eu.embedded-tools"   // 工具链管理
    ]
}
```

---

## 🚀 快速开始指南

### 方法A: 一键安装PlatformIO

1. 打开VSCode
2. 按 `Ctrl+Shift+X` 打开扩展市场
3. 搜索 "PlatformIO IDE"
4. 点击"Install"
5. 重启VSCode
6. 在项目根目录创建 `platformio.ini`（配置见上文）
7. 完成！工具链会自动下载

### 方法B: 使用ST官方扩展

1. 安装 "STM32 VS Code Extension"
2. 安装STM32CubeMX和STM32CubeProgrammer（扩展会提示）
3. 手动安装ARM工具链（见 WINDOWS_SETUP.md）
4. 导入现有Makefile项目

### 方法C: 保持现有Makefile方案

1. 安装推荐的辅助插件
2. 使用 Embedded Tools 自动下载工具链：
   ```
   命令面板: > Embedded: Install Toolchain
   选择: ARM GCC
   ```

---

## 📦 PlatformIO完整配置示例

### 完整的 platformio.ini

```ini
; PlatformIO Project Configuration File
;
; 项目: 单通道磁传感器系统
; MCU: STM32F103C8

[platformio]
default_envs = debug

; 调试环境
[env:debug]
platform = ststm32
board = genericSTM32F103C8
framework = stm32cube

; 构建设置
build_type = debug
build_flags = 
    -DUSE_STDPERIPH_DRIVER
    -DSTM32F10X_MD
    -DDEBUG
    -Og
    -g3
    -ggdb
    -mcpu=cortex-m3
    -mthumb
    -Wall
    -fdata-sections
    -ffunction-sections
    -Wl,-Map,${BUILD_DIR}/output.map
    -Wl,--gc-sections

; 包含路径
build_unflags = 
    -std=gnu++11

; 源文件路径
src_dir = firmware/src
lib_dir = firmware/libs

; 额外包含目录
lib_extra_dirs = 
    firmware/libs/CMSIS
    firmware/libs/STM32F10x_StdPeriph_Driver

; 上传和调试
upload_protocol = stlink
debug_tool = stlink
debug_init_break = tbreak main
debug_load_mode = modified

; 串口设置
monitor_speed = 115200
monitor_filters = 
    default
    time

; 发布环境
[env:release]
extends = env:debug
build_type = release
build_flags = 
    ${env:debug.build_flags}
    -DNDEBUG
    -Os
    -flto

; 测试环境
[env:test]
extends = env:debug
test_framework = unity
```

### 安装额外的库

```bash
# 在项目目录打开终端
pio lib install "Adafruit MLX90393"
pio lib install "Unity"
```

---

## 🔧 推荐的工作流

### 使用PlatformIO

```bash
# 初始化PlatformIO项目
pio init --ide vscode --board genericSTM32F103C8

# 编译
pio run

# 上传
pio run --target upload

# 调试
# 在VSCode中按F5

# 串口监视
pio device monitor

# 清理
pio run --target clean
```

### VSCode任务整合

在 `.vscode/tasks.json` 中添加PlatformIO任务：

```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "PlatformIO: Build",
            "type": "shell",
            "command": "pio run",
            "group": {
                "kind": "build",
                "isDefault": true
            },
            "problemMatcher": ["$gcc"]
        },
        {
            "label": "PlatformIO: Upload",
            "type": "shell",
            "command": "pio run --target upload",
            "dependsOn": "PlatformIO: Build"
        },
        {
            "label": "PlatformIO: Clean",
            "type": "shell",
            "command": "pio run --target clean"
        },
        {
            "label": "PlatformIO: Monitor",
            "type": "shell",
            "command": "pio device monitor"
        }
    ]
}
```

---

## 💡 我的推荐

### 🏆 最佳选择: PlatformIO IDE

**推荐理由:**
1. ✅ **零配置**: 自动下载工具链、调试器
2. ✅ **跨平台**: Windows/Linux/macOS完全一致
3. ✅ **库管理**: 轻松添加第三方库
4. ✅ **统一体验**: 标准化的构建和调试流程
5. ✅ **活跃社区**: 大量教程和支持

**适合场景:**
- 新项目或愿意调整项目结构
- 需要频繁切换开发环境
- 需要管理多个库依赖

### 🥈 次选: STM32 VS Code Extension

**推荐理由:**
1. ✅ ST官方支持
2. ✅ 与STM32生态无缝集成
3. ✅ 保持现有项目结构

**适合场景:**
- 已有STM32CubeMX项目
- 倾向使用官方工具
- 不想改变现有工作流

### 🥉 备选: Makefile + Embedded Tools

**推荐理由:**
1. ✅ 完全控制构建过程
2. ✅ 最小依赖
3. ✅ 可使用插件自动下载工具链

**适合场景:**
- 喜欢命令行工作流
- 需要精细控制构建过程
- 项目有特殊构建需求

---

## 📝 下一步行动

### 立即开始（选择一个）:

**选项1: 快速体验PlatformIO**
```bash
# 1. 安装PlatformIO IDE扩展
# 2. 创建platformio.ini配置文件
# 3. 运行 pio run
```

**选项2: 使用ST官方扩展**
```bash
# 1. 安装 STM32 VS Code Extension
# 2. 下载必需工具
# 3. 导入现有项目
```

**选项3: 保持Makefile，使用工具链管理插件**
```bash
# 1. 安装 Embedded Tools扩展
# 2. 命令: Embedded: Install Toolchain
# 3. 继续使用make构建
```

---

## 🔗 相关资源

- [PlatformIO文档](https://docs.platformio.org/)
- [STM32 VS Code Extension文档](https://github.com/stm32-hotspot/STM32-FOR-VSCODE)
- [Cortex-Debug使用指南](https://github.com/Marus/cortex-debug/wiki)
- [当前项目Makefile方案](../firmware/README.md)
