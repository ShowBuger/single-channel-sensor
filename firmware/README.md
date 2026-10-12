# STM32 CMake开发环境

现代化的STM32开发环境，使用 **CMake + Ninja + Arm GNU Toolchain + clangd + Cortex-Debug**

## 🚀 快速开始

### Windows（PowerShell）

**第一步：安装工具链**（首次克隆后只需运行一次）
```powershell
.\tools\install_toolchain.ps1
```
脚本会自动下载 ARM GNU Toolchain 15.3 并配置好路径，下载约 1 GB。

**第二步：加载环境**（每次打开新终端后运行）
```powershell
. .\tools\env_setup.ps1
```

**第三步：构建**
```powershell
.\tools\build.ps1
```

**第四步：烧录**
```powershell
.\tools\flash.ps1                   # OpenOCD（默认）
.\tools\flash.ps1 -Method stlink   # ST-Link
.\tools\flash.ps1 -Method jlink    # J-Link
```

## 📚 文档

- 📘 [详细配置指南](docs/CMAKE_SETUP_GUIDE.md) - 完整的安装和配置说明

## 🎯 主要特性

- ✅ **CMake** - 灵活的构建系统
- ✅ **Ninja** - 快速增量编译
- ✅ **clangd** - 智能代码补全和检查
- ✅ **Cortex-Debug** - 强大的可视化调试
- ✅ **Windows** - PowerShell 脚本一键构建与烧录

## 📊 项目结构

```
firmware/
├── CMakeLists.txt              # CMake配置
├── cmake/                      # CMake模块
├── src/                        # 源代码
├── libs/                       # 第三方库
├── tools/                      # 辅助脚本
├── docs/                       # 文档
└── build/                      # 构建输出
```

## 🔧 VSCode任务

- **CMake: Build** - 编译 (Ctrl+Shift+B)
- **Flash (OpenOCD)** - 烧录固件
- **Show Size** - 显示固件大小

## 📖 更多信息

查看 [详细配置指南](docs/CMAKE_SETUP_GUIDE.md) 了解：
- 工具安装说明
- VSCode配置详情
- 调试技巧
- 常见问题解决

---

**MCU**: STM32F103C8T6 (64KB Flash, 20KB RAM)  
**工具链**: Arm GNU Toolchain (arm-none-eabi-gcc)  
**构建系统**: CMake 3.22+ with Ninja
