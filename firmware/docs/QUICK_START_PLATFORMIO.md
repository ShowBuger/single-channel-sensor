# 🚀 VSCode插件一键安装指南

## 📦 推荐方案：PlatformIO IDE（自动安装所有工具）

### ✨ 为什么选择PlatformIO？

- ✅ **自动安装工具链**：ARM GCC、OpenOCD、GDB等全部自动下载
- ✅ **零配置**：无需手动设置PATH环境变量
- ✅ **跨平台**：Windows/Linux/macOS完全相同的体验
- ✅ **统一管理**：工具版本统一，避免冲突
- ✅ **开箱即用**：安装后立即可以编译、上传、调试

---

## 📋 三步完成安装

### 第1步：安装PlatformIO IDE扩展

1. 打开VSCode
2. 点击左侧扩展图标（或按 `Ctrl+Shift+X`）
3. 搜索：**PlatformIO IDE**
4. 点击 **Install** 按钮
5. 等待安装完成（约1-2分钟）
6. 重启VSCode

![安装PlatformIO](https://docs.platformio.org/en/latest/_images/platformio-ide-vscode-pkg-installer.png)

### 第2步：首次打开项目

重启VSCode后，PlatformIO会自动：
- ✅ 检测 `platformio.ini` 配置文件
- ✅ 下载ARM工具链（约200MB）
- ✅ 下载OpenOCD调试工具
- ✅ 配置开发环境

**首次加载需要5-10分钟，请耐心等待！**

你会在底部状态栏看到下载进度。

### 第3步：验证安装

安装完成后，点击左侧的PlatformIO图标（蚂蚁图标），应该看到：

```
PROJECT TASKS
├── debug
│   ├── General
│   │   ├── Build          ← 编译
│   │   ├── Clean          ← 清理
│   │   └── Upload         ← 上传
│   ├── Advanced
│   │   └── ...
├── release
│   └── ...
```

---

## 🎯 使用PlatformIO开发

### 快捷操作

| 操作 | 方法1：侧边栏 | 方法2：快捷键 | 方法3：底部工具栏 |
|------|--------------|--------------|------------------|
| **编译** | PlatformIO → Build | `Ctrl+Alt+B` | ✓ (对勾) |
| **上传** | PlatformIO → Upload | `Ctrl+Alt+U` | → (右箭头) |
| **清理** | PlatformIO → Clean | `Ctrl+Alt+C` | 🗑️ (垃圾桶) |
| **调试** | Run → Start Debugging | `F5` | 🐛 (虫子) |
| **串口监视器** | PlatformIO → Monitor | `Ctrl+Alt+S` | 🔌 (插头) |

### 底部工具栏说明

安装PlatformIO后，底部会出现快捷按钮：

```
[🏠] [✓] [→] [🗑️] [🔌] [🐛] ...
 主页  编译  上传  清理  串口  调试
```

### 完整工作流

```bash
# 1. 编辑代码
firmware/src/main.c

# 2. 编译（快捷键 Ctrl+Alt+B）
点击底部 ✓ 按钮

# 3. 连接ST-Link调试器

# 4. 上传（快捷键 Ctrl+Alt+U）
点击底部 → 按钮

# 5. 打开串口监视器（快捷键 Ctrl+Alt+S）
点击底部 🔌 按钮

# 6. 调试（快捷键 F5）
点击底部 🐛 按钮
```

---

## 🔧 配置说明

### 当前配置（platformio.ini）

项目已配置三个环境：

1. **debug** - 开发调试（默认）
   - 优化级别：`-Og`（适合调试）
   - 包含调试符号
   - 默认编译环境

2. **release** - 生产发布
   - 优化级别：`-Os`（优化大小）
   - 无调试符号
   - 链接时优化（LTO）

3. **test** - 单元测试
   - 使用Unity测试框架

### 切换编译环境

**方法1：命令行**
```bash
# 编译release版本
pio run -e release

# 编译debug版本
pio run -e debug
```

**方法2：修改默认环境**
编辑 `platformio.ini`：
```ini
[platformio]
default_envs = release  ; 改为release
```

### 修改串口号

编辑 `platformio.ini`，找到：
```ini
monitor_port = COM3  ; 改为你的串口号
```

查看可用串口：
```bash
pio device list
```

---

## 🐛 调试功能

### 启动调试

1. **设置断点**：在代码行号左侧点击，出现红点
2. **启动调试**：按 `F5` 或点击底部 🐛 按钮
3. **等待连接**：PlatformIO会自动启动OpenOCD并连接调试器

### 调试控制

| 操作 | 快捷键 | 说明 |
|------|--------|------|
| 继续 | `F5` | 继续运行到下一个断点 |
| 单步跳过 | `F10` | 执行当前行，不进入函数 |
| 单步进入 | `F11` | 进入函数内部 |
| 单步跳出 | `Shift+F11` | 跳出当前函数 |
| 停止 | `Shift+F5` | 停止调试 |
| 重启 | `Ctrl+Shift+F5` | 重新启动调试 |

### 调试面板

- **VARIABLES**：查看变量值
- **WATCH**：监视表达式
- **CALL STACK**：调用栈
- **BREAKPOINTS**：断点列表
- **PERIPHERAL**：外设寄存器（SVD）

---

## 📚 额外功能

### 库管理

PlatformIO有强大的库管理功能：

```bash
# 搜索库
pio lib search "sensor"

# 安装库
pio lib install "Adafruit MLX90393"

# 查看已安装的库
pio lib list

# 更新库
pio lib update
```

在 `platformio.ini` 中声明依赖：
```ini
lib_deps =
    adafruit/Adafruit MLX90393
```

### 单元测试

创建测试文件 `tests/test_sensor.c`：

```c
#include <unity.h>

void test_sensor_init(void) {
    TEST_ASSERT_EQUAL(0, sensor_init());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_sensor_init);
    return UNITY_END();
}
```

运行测试：
```bash
pio test
```

### 代码检查

启用静态分析：
```ini
check_tool = cppcheck, clangtidy
check_flags =
    cppcheck: --enable=all
```

运行检查：
```bash
pio check
```

---

## ❓ 常见问题

### Q1: 下载速度慢？

A: 使用国内镜像加速：

**Windows（PowerShell管理员模式）：**
```powershell
[System.Environment]::SetEnvironmentVariable("PLATFORMIO_CORE_DIR", "D:\\.platformio", "User")
```

或手动设置环境变量：
```
PLATFORMIO_PACKAGES_MIRROR = https://mirrors.tuna.tsinghua.edu.cn/platformio/
```

### Q2: 找不到调试器？

A: 检查连接和驱动：
```bash
# 列出已连接的设备
pio device list

# 测试OpenOCD连接
.platformio\packages\tool-openocd\bin\openocd.exe -f interface/stlink.cfg -f target/stm32f1x.cfg
```

确保已安装ST-Link驱动。

### Q3: 编译错误？

A: 清理并重新编译：
```bash
# 清理
pio run -t clean

# 完全清理（包括依赖）
pio run -t fullclean

# 重新编译
pio run
```

### Q4: 想保留Makefile？

A: 可以同时使用！

- **VSCode**：使用PlatformIO（自动工具链）
- **命令行**：使用Makefile（手动工具链）

两者互不干扰，编译输出在不同目录：
- PlatformIO: `.pio/build/`
- Makefile: `firmware/build/`

### Q5: 如何查看内存使用？

A: 编译后会自动显示：
```
RAM:   [====      ]  42.3% (used 8658 bytes from 20480 bytes)
Flash: [======    ]  58.2% (used 38124 bytes from 65536 bytes)
```

或手动查询：
```bash
pio run -t size
```

---

## 🔄 从Makefile迁移

### 无缝迁移方案

您的项目已经配置好，**无需改变现有代码结构**！

`platformio.ini` 已配置：
```ini
src_dir = firmware/src        ; 使用现有源代码目录
lib_dir = firmware/libs        ; 使用现有库目录
board_build.ldscript = firmware/STM32F103C8Tx_FLASH.ld  ; 使用现有链接脚本
```

### 对比

| 项目 | Makefile方案 | PlatformIO方案 |
|------|-------------|----------------|
| 工具链安装 | ❌ 手动下载配置 | ✅ 自动安装 |
| PATH配置 | ❌ 手动设置 | ✅ 无需配置 |
| 编译命令 | `make` | `pio run` 或 `Ctrl+Alt+B` |
| 上传命令 | `make flash` | `pio run -t upload` 或 `Ctrl+Alt+U` |
| 调试 | ❌ 需配置OpenOCD+GDB | ✅ 按F5即可 |
| 串口监视 | 需外部工具 | ✅ 内置 `Ctrl+Alt+S` |
| 跨平台 | ⚠️ 需适配 | ✅ 完全一致 |

---

## 🎓 学习资源

- [PlatformIO官方文档](https://docs.platformio.org/)
- [STM32平台指南](https://docs.platformio.org/en/latest/platforms/ststm32.html)
- [调试指南](https://docs.platformio.org/en/latest/plus/debugging.html)
- [单元测试](https://docs.platformio.org/en/latest/advanced/unit-testing/index.html)

---

## 🎉 总结

安装PlatformIO后，您将获得：

✅ **ARM工具链** - 自动下载和配置  
✅ **OpenOCD** - 自动安装和配置  
✅ **GDB调试器** - 开箱即用  
✅ **串口监视器** - 内置支持  
✅ **代码补全** - IntelliSense  
✅ **一键编译上传** - 快捷键支持  
✅ **图形化调试** - 断点、变量监视  
✅ **跨平台** - Windows/Linux/macOS  

**无需手动安装任何工具，无需配置环境变量！**

---

## 📞 下一步

1. ✅ 安装PlatformIO IDE扩展
2. ✅ 重启VSCode
3. ✅ 等待工具链下载（5-10分钟）
4. ✅ 点击底部 ✓ 按钮编译
5. ✅ 连接ST-Link调试器
6. ✅ 点击底部 → 按钮上传
7. ✅ 按F5开始调试

**准备好了吗？现在就去安装PlatformIO IDE扩展吧！** 🚀
