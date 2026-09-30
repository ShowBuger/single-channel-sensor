# STM32 CMake开发环境配置指南

本项目使用现代化的 **CMake + Ninja + Arm GNU Toolchain + clangd + Cortex-Debug** 工具链进行STM32开发。

## 目录
- [环境要求](#环境要求)
- [工具安装](#工具安装)
- [VSCode配置](#vscode配置)
- [构建项目](#构建项目)
- [调试](#调试)
- [常见问题](#常见问题)

---

## 环境要求

### 必需工具
- **CMake** >= 3.22
- **Ninja** 构建系统
- **Arm GNU Toolchain** (arm-none-eabi-gcc)
- **clangd** 语言服务器
- **OpenOCD** 或 **J-Link** 调试器
- **VSCode** 编辑器

---

## 工具安装

### Windows

#### 1. 安装 Arm GNU Toolchain
```powershell
# 下载最新版本
https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads

# 安装后添加到PATH (例如)
C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\13.2 Rel1\bin

# 验证安装
arm-none-eabi-gcc --version
```

#### 2. 安装 CMake
```powershell
# 使用 Chocolatey
choco install cmake --installargs 'ADD_CMAKE_TO_PATH=System'

# 或下载安装包
https://cmake.org/download/

# 验证安装
cmake --version
```

#### 3. 安装 Ninja
```powershell
# 使用 Chocolatey
choco install ninja

# 或下载预编译版本
https://github.com/ninja-build/ninja/releases

# 将 ninja.exe 放到 PATH 目录中
# 验证安装
ninja --version
```

#### 4. 安装 clangd
```powershell
# 使用 Chocolatey
choco install llvm

# 或下载 LLVM
https://releases.llvm.org/download.html

# 验证安装
clangd --version
```

#### 5. 安装 OpenOCD
```powershell
# 下载 xPack OpenOCD
https://github.com/xpack-dev-tools/openocd-xpack/releases

# 解压并添加到 PATH
# 验证安装
openocd --version
```

#### 6. 安装 ST-Link 驱动 (可选)
```powershell
# 下载 ST-Link 驱动
https://www.st.com/en/development-tools/stsw-link009.html
```

---

### macOS

```bash
# 使用 Homebrew 一键安装所有工具
brew install cmake ninja arm-none-eabi-gcc llvm openocd

# 添加 LLVM 到 PATH (包含 clangd)
echo 'export PATH="/opt/homebrew/opt/llvm/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc

# 验证安装
arm-none-eabi-gcc --version
cmake --version
ninja --version
clangd --version
openocd --version
```

---

### Linux (Ubuntu/Debian)

```bash
# 安装 CMake 和 Ninja
sudo apt update
sudo apt install cmake ninja-build

# 安装 Arm GNU Toolchain
sudo apt install gcc-arm-none-eabi gdb-multiarch

# 安装 clangd
sudo apt install clangd-14
sudo update-alternatives --install /usr/bin/clangd clangd /usr/bin/clangd-14 100

# 安装 OpenOCD
sudo apt install openocd

# 配置 udev 规则 (ST-Link)
sudo tee /etc/udev/rules.d/99-stlink.rules > /dev/null <<EOF
# ST-Link V2
SUBSYSTEM=="usb", ATTR{idVendor}=="0483", ATTR{idProduct}=="3748", MODE="0666"
# ST-Link V2.1
SUBSYSTEM=="usb", ATTR{idVendor}=="0483", ATTR{idProduct}=="374b", MODE="0666"
# ST-Link V3
SUBSYSTEM=="usb", ATTR{idVendor}=="0483", ATTR{idProduct}=="374e", MODE="0666"
EOF

sudo udevadm control --reload-rules
sudo udevadm trigger

# 验证安装
arm-none-eabi-gcc --version
cmake --version
ninja --version
clangd --version
openocd --version
```

---

## VSCode配置

### 1. 安装推荐扩展

打开项目后，VSCode会自动提示安装以下扩展：

- **CMake Tools** (`ms-vscode.cmake-tools`) - CMake支持
- **CMake Language Support** (`twxs.cmake`) - CMake语法高亮
- **clangd** (`llvm-vs-code-extensions.vscode-clangd`) - C/C++语言服务器
- **Cortex-Debug** (`marus25.cortex-debug`) - ARM Cortex调试
- **ARM** (`dan-c-underwood.arm`) - ARM汇编支持
- **LinkerScript** (`zixuanwang.linkerscript`) - 链接脚本语法
- **Hex Editor** (`ms-vscode.hexeditor`) - 二进制文件查看

### 2. 禁用 C/C++ IntelliSense

由于使用 clangd，需要禁用默认的 C/C++ IntelliSense：

1. 打开设置 (`Ctrl+,`)
2. 搜索 `C_Cpp.intelliSenseEngine`
3. 设置为 `disabled`

或者已经在 `.vscode/settings.json` 中配置好了。

---

## 构建项目

### 方式1: VSCode 任务

#### 配置项目
按 `Ctrl+Shift+P`，选择 `Tasks: Run Task` → `CMake: Configure`

或直接运行：
```bash
cmake -B firmware/build -S firmware -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

#### 编译项目
按 `Ctrl+Shift+B` (默认构建任务)

或按 `Ctrl+Shift+P`，选择 `Tasks: Run Task` → `CMake: Build`

#### 清理项目
`Tasks: Run Task` → `CMake: Clean`

#### 重新构建
`Tasks: Run Task` → `CMake: Rebuild`

---

### 方式2: 命令行

```bash
# 进入 firmware 目录
cd firmware

# 配置 (Debug模式)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug

# 编译
cmake --build build -j8

# 查看大小
arm-none-eabi-size build/single_channel_sensor.elf

# 清理
cmake --build build --target clean

# 配置 Release 模式
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
```

---

## 烧录固件

### 方式1: VSCode 任务

`Ctrl+Shift+P` → `Tasks: Run Task` → `Flash (OpenOCD)` 或 `Flash (ST-Link)`

### 方式2: 命令行

#### 使用 OpenOCD (ST-Link)
```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
  -c "program firmware/build/single_channel_sensor.elf verify reset exit"
```

#### 使用 st-flash
```bash
st-flash write firmware/build/single_channel_sensor.bin 0x8000000
```

#### 擦除 Flash
```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
  -c "init" -c "reset halt" -c "stm32f1x mass_erase 0" -c "shutdown"
```

---

## 调试

### VSCode 图形化调试

1. **按 F5 启动调试**
2. 选择调试配置：
   - `Cortex Debug (OpenOCD/ST-Link)` - 使用 ST-Link
   - `Cortex Debug (J-Link)` - 使用 J-Link

#### 调试功能
- **断点**: 点击行号左侧设置断点
- **条件断点**: 右键断点 → 编辑断点 → 添加条件
- **数据断点**: 变量上右键 → Break When Value Changes
- **查看变量**: 鼠标悬停或在 VARIABLES 面板查看
- **监视表达式**: DEBUG CONSOLE 中输入
- **寄存器查看**: CORTEX PERIPHERALS 面板
- **内存查看**: MEMORY 面板

#### 调试快捷键
- `F5` - 继续执行
- `F10` - 单步跳过 (Step Over)
- `F11` - 单步进入 (Step Into)
- `Shift+F11` - 单步跳出 (Step Out)
- `Ctrl+Shift+F5` - 重启调试
- `Shift+F5` - 停止调试

---

### 命令行调试 (GDB)

```bash
# 终端1: 启动 OpenOCD 服务器
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg

# 终端2: 启动 GDB
arm-none-eabi-gdb firmware/build/single_channel_sensor.elf

# GDB 命令
(gdb) target remote :3333
(gdb) monitor reset halt
(gdb) load
(gdb) break main
(gdb) continue
(gdb) info registers
(gdb) x/16x 0x20000000  # 查看内存
(gdb) quit
```

---

## clangd 代码智能提示

### 特性

- **实时语法检查**: 代码编写时即时反馈错误
- **代码补全**: 智能变量/函数/宏补全
- **跳转定义**: `F12` 或 `Ctrl+鼠标左键`
- **查找引用**: `Shift+F12`
- **重命名符号**: `F2`
- **代码格式化**: `Shift+Alt+F`
- **内联提示**: 显示参数名称和推导类型
- **代码诊断**: 使用 clang-tidy 检查代码质量

### 配置文件

- `firmware/.clangd` - clangd 配置
- `firmware/.clang-format` - 代码格式化规则

### 重新生成 compile_commands.json

每次修改 CMakeLists.txt 后需要重新配置：

```bash
cmake -B firmware/build -S firmware -G Ninja
```

clangd 会自动读取 `firmware/build/compile_commands.json`

---

## 构建输出文件

构建成功后，`firmware/build/` 目录包含：

```
build/
├── single_channel_sensor.elf    # ELF可执行文件 (用于调试)
├── single_channel_sensor.hex    # Intel HEX 格式
├── single_channel_sensor.bin    # 二进制格式 (用于烧录)
├── single_channel_sensor.map    # 链接映射文件
└── compile_commands.json        # 编译数据库 (clangd使用)
```

---

## 常见问题

### 1. 找不到 arm-none-eabi-gcc

**原因**: 工具链未安装或未添加到 PATH

**解决**:
```bash
# 验证安装
arm-none-eabi-gcc --version

# Windows: 添加到系统 PATH
# Linux/macOS: 添加到 ~/.bashrc 或 ~/.zshrc
export PATH="/path/to/arm-toolchain/bin:$PATH"
```

---

### 2. CMake 找不到工具链

**错误**: `Could not find compiler`

**解决**: 确保 `cmake/arm-none-eabi-gcc.cmake` 中的工具链配置正确

---

### 3. clangd 报错找不到头文件

**解决**:
1. 确保已运行 `CMake: Configure` 生成 `compile_commands.json`
2. 检查 `.clangd` 文件中的 `--query-driver` 参数
3. 重启 clangd: `Ctrl+Shift+P` → `clangd: Restart language server`

---

### 4. OpenOCD 无法连接调试器

**错误**: `Error: libusb_open() failed with LIBUSB_ERROR_ACCESS`

**Linux 解决**: 配置 udev 规则（见安装章节）

**Windows 解决**: 安装 ST-Link 驱动

**通用测试**:
```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg
# 应该看到: Info : stm32f1x.cpu: hardware has 6 breakpoints
```

---

### 5. Ninja 构建失败

**错误**: `ninja: error: loading 'build.ninja'`

**解决**: 重新配置项目
```bash
rm -rf firmware/build
cmake -B firmware/build -S firmware -G Ninja
```

---

### 6. 调试时无法命中断点

**可能原因**:
1. 优化级别过高 - 使用 Debug 模式 (`-Og`)
2. 代码未被执行
3. 调试信息未生成 - 检查 `-g3 -ggdb` 标志

---

### 7. 编译通过但程序不运行

**排查步骤**:
1. 检查启动文件是否正确 (startup_stm32f10x_md.s)
2. 检查链接脚本的 Flash/RAM 大小
3. 检查系统时钟初始化
4. 使用 GDB 单步调试启动代码

---

## 项目结构

```
firmware/
├── CMakeLists.txt               # CMake 构建脚本
├── cmake/
│   └── arm-none-eabi-gcc.cmake  # 工具链配置
├── .clangd                      # clangd 配置
├── .clang-format                # 代码格式化规则
├── STM32F103C8Tx_FLASH.ld      # 链接脚本
├── src/                         # 源代码
│   ├── main.c
│   ├── stm32f10x_it.c
│   ├── stm32f10x_conf.h
│   ├── app/                     # 应用层
│   └── drivers/                 # 驱动层
├── libs/                        # 第三方库
│   ├── CMSIS/
│   └── STM32F10x_StdPeriph_Driver/
└── build/                       # 构建输出 (gitignore)
```

---

## 性能优化建议

### 编译优化级别

在 `CMakeLists.txt` 中配置：

```cmake
# Debug: -Og -g3 -ggdb (默认)
# Release: -Os (优化大小)

# 切换构建类型
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

### 查看代码大小

```bash
# 查看段大小
arm-none-eabi-size firmware/build/single_channel_sensor.elf

# 按函数大小排序
arm-none-eabi-nm -S --size-sort firmware/build/single_channel_sensor.elf | tail -20

# 生成反汇编
arm-none-eabi-objdump -d firmware/build/single_channel_sensor.elf > disassembly.txt
```

---

## 参考资料

### 官方文档
- [STM32F103 数据手册](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)
- [STM32F10x 参考手册](https://www.st.com/resource/en/reference_manual/cd00171190.pdf)
- [ARM Cortex-M3 技术手册](https://developer.arm.com/documentation/dui0552/a/)

### 工具文档
- [CMake 官方文档](https://cmake.org/documentation/)
- [Ninja 构建系统](https://ninja-build.org/)
- [clangd 用户手册](https://clangd.llvm.org/)
- [OpenOCD 用户手册](http://openocd.org/doc/html/index.html)
- [Cortex-Debug Wiki](https://github.com/Marus/cortex-debug/wiki)

---

## 总结

现在你拥有了一个现代化、高效的 STM32 开发环境：

✅ **CMake** - 灵活的构建系统  
✅ **Ninja** - 快速的增量编译  
✅ **Arm GNU Toolchain** - 官方 ARM 编译器  
✅ **clangd** - 智能代码提示和检查  
✅ **Cortex-Debug** - 强大的可视化调试  

开始编码吧！🚀
