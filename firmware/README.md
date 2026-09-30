# STM32 CMake开发环境

现代化的STM32开发环境，使用 **CMake + Ninja + Arm GNU Toolchain + clangd + Cortex-Debug**

## 🚀 快速开始

### 1. 检查环境
```bash
./tools/check_env.sh
```

### 2. 构建项目
```bash
./tools/build.sh
# 或在VSCode中按 Ctrl+Shift+B
```

### 3. 烧录固件
```bash
./tools/flash.sh
# 或在VSCode中: Ctrl+Shift+P → Tasks → Flash
```

### 4. 调试
```bash
# 在VSCode中按 F5
```

## 📚 文档

- 📘 [详细配置指南](docs/CMAKE_SETUP_GUIDE.md) - 完整的安装和配置说明
- 📗 [快速入门](README_QUICK_START.md) - 5分钟快速上手
- 📙 [配置总结](docs/CMAKE_SETUP_SUMMARY.md) - 已完成配置的总结

## 🎯 主要特性

- ✅ **CMake** - 灵活的构建系统
- ✅ **Ninja** - 快速增量编译
- ✅ **clangd** - 智能代码补全和检查
- ✅ **Cortex-Debug** - 强大的可视化调试
- ✅ **跨平台** - Linux/macOS/Windows支持

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
