# STM32 CMake开发环境 - 快速入门

## 🚀 5分钟快速开始

### 1. 检查环境

```bash
cd firmware
./tools/check_env.sh    # Linux/macOS
tools\check_env.bat     # Windows (如果提供)
```

### 2. 构建项目

```bash
# 方式1: 使用脚本
./tools/build.sh

# 方式2: 使用 CMake 命令
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j8

# 方式3: VSCode 中按 Ctrl+Shift+B
```

### 3. 烧录固件

```bash
# 方式1: 使用脚本
./tools/flash.sh -m openocd    # OpenOCD/ST-Link
./tools/flash.sh -m stlink     # st-flash
./tools/flash.sh -m jlink      # J-Link

# 方式2: VSCode 任务
# Ctrl+Shift+P → Tasks: Run Task → Flash (OpenOCD)
```

### 4. 调试

```bash
# VSCode 中按 F5 启动调试
# 或使用命令行
./tools/debug.sh
```

---

## 📁 项目结构

```
firmware/
├── CMakeLists.txt              # CMake 主配置
├── cmake/
│   └── arm-none-eabi-gcc.cmake # 工具链配置
├── .clangd                     # clangd 配置
├── .clang-format               # 代码格式化
│
├── src/                        # 源代码
│   ├── main.c
│   ├── stm32f10x_it.c
│   ├── app/                    # 应用层
│   └── drivers/                # 驱动层
│
├── libs/                       # 第三方库
│   ├── CMSIS/
│   └── STM32F10x_StdPeriph_Driver/
│
├── tools/                      # 辅助脚本
│   ├── build.sh                # 构建脚本
│   ├── flash.sh                # 烧录脚本
│   └── debug.sh                # 调试脚本
│
├── docs/                       # 文档
│   └── CMAKE_SETUP_GUIDE.md    # 详细配置指南
│
└── build/                      # 构建输出 (自动生成)
    ├── single_channel_sensor.elf
    ├── single_channel_sensor.hex
    ├── single_channel_sensor.bin
    └── compile_commands.json   # clangd 使用
```

---

## 🔧 常用命令

### 构建相关

```bash
# Debug 构建
./tools/build.sh

# Release 构建
./tools/build.sh -r

# 清理并重新构建
./tools/build.sh -c

# 查看固件大小
arm-none-eabi-size build/single_channel_sensor.elf

# 生成反汇编
arm-none-eabi-objdump -d build/single_channel_sensor.elf > disassembly.txt
```

### 烧录相关

```bash
# OpenOCD 烧录
./tools/flash.sh

# ST-Link 烧录
./tools/flash.sh -m stlink

# 烧录不验证
./tools/flash.sh --no-verify

# 擦除 Flash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
  -c "init" -c "reset halt" -c "stm32f1x mass_erase 0" -c "exit"
```

### 调试相关

```bash
# 启动 GDB 调试
./tools/debug.sh

# OpenOCD 服务器
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg

# 在另一个终端连接 GDB
arm-none-eabi-gdb build/single_channel_sensor.elf
(gdb) target remote :3333
(gdb) load
(gdb) break main
(gdb) continue
```

---

## 🎯 VSCode 使用

### 快捷键

| 功能 | 快捷键 |
|------|--------|
| 构建项目 | `Ctrl+Shift+B` |
| 启动调试 | `F5` |
| 运行任务 | `Ctrl+Shift+P` → Tasks |
| 跳转定义 | `F12` |
| 查找引用 | `Shift+F12` |
| 重命名符号 | `F2` |
| 格式化代码 | `Shift+Alt+F` |

### 任务列表

- **CMake: Configure** - 配置 CMake
- **CMake: Build** - 编译项目 (默认)
- **CMake: Clean** - 清理构建
- **CMake: Rebuild** - 重新构建
- **Flash (OpenOCD)** - OpenOCD 烧录
- **Flash (ST-Link)** - ST-Link 烧录
- **Show Size** - 显示固件大小

### 调试配置

- **Cortex Debug (OpenOCD/ST-Link)** - 使用 ST-Link 调试
- **Cortex Debug (J-Link)** - 使用 J-Link 调试

---

## 🐛 代码智能提示 (clangd)

### 特性

- ✅ 实时语法检查
- ✅ 智能代码补全
- ✅ 跳转到定义/声明
- ✅ 查找所有引用
- ✅ 重命名符号
- ✅ 代码格式化
- ✅ 内联提示 (参数名、类型推导)
- ✅ 静态分析 (clang-tidy)

### 配置文件

- `.clangd` - clangd 配置
- `.clang-format` - 代码格式化规则
- `build/compile_commands.json` - 编译数据库

### 刷新 IntelliSense

```bash
# 重新配置 CMake
cmake -B build -G Ninja

# VSCode 中重启 clangd
Ctrl+Shift+P → clangd: Restart language server
```

---

## 📊 构建输出解读

### 固件大小

```
   text    data     bss     dec     hex filename
  12345     100    2048   14493    3897 single_channel_sensor.elf
```

- **text**: 代码段 (Flash)
- **data**: 已初始化数据 (Flash + RAM)
- **bss**: 未初始化数据 (RAM)
- **dec**: 总大小 (十进制)
- **hex**: 总大小 (十六进制)

### Flash 和 RAM 计算

STM32F103C8T6: **64KB Flash**, **20KB RAM**

```
Flash 使用 = text + data
RAM 使用 = data + bss
```

---

## ⚙️ 编译优化

### 优化级别

在 `CMakeLists.txt` 中配置：

```cmake
# Debug: -Og (调试优化)
# Release: -Os (优化大小)
# 也可选择: -O0, -O1, -O2, -O3
```

### 切换构建类型

```bash
# Debug
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Release
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

---

## 🔍 调试技巧

### 断点类型

- **普通断点**: 点击行号左侧
- **条件断点**: 右键断点 → 编辑断点
- **数据断点**: 变量上右键 → Break When Value Changes

### 查看外设寄存器

在 Cortex-Debug 中可以查看 STM32 外设寄存器（需要 SVD 文件）

SVD 文件下载：
```
https://github.com/posborne/cmsis-svd/blob/master/data/STMicro/STM32F103xx.svd
```

将其放到 `firmware/docs/STM32F103xx.svd`

### GDB 常用命令

```gdb
info registers           # 查看寄存器
x/16x 0x20000000        # 查看内存
print variable          # 打印变量
set variable = 100      # 修改变量
backtrace               # 查看调用栈
step                    # 单步进入
next                    # 单步跳过
continue                # 继续执行
monitor reset halt      # 复位并停止
```

---

## 📚 资源链接

### 芯片资料
- [STM32F103 数据手册](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)
- [STM32F10x 参考手册](https://www.st.com/resource/en/reference_manual/cd00171190.pdf)

### 工具文档
- [CMake 文档](https://cmake.org/documentation/)
- [clangd 用户手册](https://clangd.llvm.org/)
- [OpenOCD 用户手册](http://openocd.org/doc/html/index.html)
- [Cortex-Debug Wiki](https://github.com/Marus/cortex-debug/wiki)

### 教程
- 完整配置指南: `docs/CMAKE_SETUP_GUIDE.md`

---

## ❓ 常见问题

### Q: 找不到 arm-none-eabi-gcc？
**A**: 确保 ARM 工具链已安装并添加到 PATH。运行 `./tools/check_env.sh` 检查。

### Q: clangd 报错找不到头文件？
**A**: 运行 `cmake -B build` 生成 `compile_commands.json`，然后重启 clangd。

### Q: 调试时无法命中断点？
**A**: 确保使用 Debug 构建模式 (`-DCMAKE_BUILD_TYPE=Debug`)。

### Q: 烧录失败？
**A**: 检查调试器连接，Windows 下需安装驱动，Linux 下需配置 udev 规则。

---

## 🎉 开始开发

现在你已经准备好开始开发了！

1. ✅ 环境已配置
2. ✅ 工具已安装
3. ✅ 项目可构建
4. ✅ 调试可用
5. ✅ 代码智能提示已启用

**享受现代化的嵌入式开发体验！** 🚀
