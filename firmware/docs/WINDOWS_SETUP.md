# Windows环境下STM32开发环境配置指南

## 📦 需要安装的工具

### 1. ARM工具链 (必需)

#### 方法A: 使用Chocolatey (推荐)
```powershell
# 在PowerShell管理员模式下执行
choco install gcc-arm-embedded -y
```

#### 方法B: 手动安装
1. 下载 **GNU Arm Embedded Toolchain**
   - 访问: https://developer.arm.com/downloads/-/gnu-rm
   - 下载最新版本 (例如: gcc-arm-none-eabi-10.3-2021.10-win32.exe)
   
2. 安装选项:
   - ✅ 勾选 "Add path to environment variable"
   
3. 验证安装:
   ```cmd
   arm-none-eabi-gcc --version
   ```

### 2. Make工具 (必需)

#### 方法A: 使用MinGW-w64 (推荐)
1. 下载MSYS2: https://www.msys2.org/
2. 安装后打开MSYS2终端，执行:
   ```bash
   pacman -S mingw-w64-ucrt-x86_64-make
   ```
3. 将 `C:\msys64\ucrt64\bin` 添加到系统PATH

#### 方法B: 使用MinGW Make
1. 下载: https://sourceforge.net/projects/mingw-w64/files/
2. 安装后添加到PATH
3. 将 `mingw32-make.exe` 复制为 `make.exe`

#### 方法C: 使用xPack Windows Build Tools
```cmd
npm install --global xpm
xpm install --global @xpack-dev-tools/windows-build-tools@latest
```

### 3. OpenOCD调试工具 (推荐)

#### 方法A: xPack OpenOCD (推荐)
```cmd
npm install --global xpm
xpm install --global @xpack-dev-tools/openocd@latest
```

#### 方法B: 手动安装
1. 下载预编译版本
   - https://github.com/xpack-dev-tools/openocd-xpack/releases
2. 解压到 `C:\Program Files\OpenOCD`
3. 添加 `C:\Program Files\OpenOCD\bin` 到PATH

### 4. ST-Link工具 (备用烧录方式)

下载并安装 **STM32 ST-LINK Utility**:
- https://www.st.com/en/development-tools/stsw-link004.html

或使用命令行版本 **st-link**:
- https://github.com/stlink-org/stlink/releases

### 5. ST-Link驱动 (必需，如使用ST-Link)

下载并安装 **ST-Link/V2 Driver**:
- https://www.st.com/en/development-tools/stsw-link009.html

## 🔧 VSCode配置

### 必需扩展
在VSCode中按 `Ctrl+Shift+X` 搜索并安装:

1. **C/C++** (ms-vscode.cpptools)
2. **Cortex-Debug** (marus25.cortex-debug)
3. **ARM** (dan-c-underwood.arm)
4. **LinkerScript** (zixuanwang.linkerscript)

## 🚀 快速验证

### 1. 验证工具链
```cmd
# 打开新的命令提示符窗口
arm-none-eabi-gcc --version
arm-none-eabi-g++ --version
arm-none-eabi-gdb --version
make --version
```

### 2. 测试编译
```cmd
cd D:\SensorProject\single_channel_system\firmware
make
```

### 3. 查看编译结果
编译成功后，在 `build/` 目录下应该有:
- `single_channel_sensor.elf` - ELF可执行文件
- `single_channel_sensor.hex` - Intel HEX格式
- `single_channel_sensor.bin` - 二进制文件
- `single_channel_sensor.map` - 内存映射文件

## 📝 Windows特定问题

### 问题1: PATH环境变量设置

**设置方法:**
1. 右键"此电脑" → "属性" → "高级系统设置"
2. "环境变量" → 编辑 "Path"
3. 添加工具路径，例如:
   ```
   C:\Program Files (x86)\GNU Arm Embedded Toolchain\10 2021.10\bin
   C:\msys64\ucrt64\bin
   C:\Program Files\OpenOCD\bin
   ```

### 问题2: Make命令未找到

如果安装了mingw32-make但没有make命令:
```cmd
# 方法A: 创建别名
copy "C:\msys64\ucrt64\bin\mingw32-make.exe" "C:\msys64\ucrt64\bin\make.exe"

# 方法B: 使用mingw32-make替代
mingw32-make -C firmware
```

### 问题3: 权限问题

某些操作可能需要管理员权限:
- 安装驱动程序
- 首次访问ST-Link
- 修改系统PATH

以管理员身份运行VSCode:
```
右键VSCode快捷方式 → "以管理员身份运行"
```

### 问题4: 编译速度慢

使用并行编译:
```cmd
make -j8  # 使用8个并行任务
```

在VSCode的 `tasks.json` 中已配置 `-j4`

### 问题5: 中文路径问题

建议避免在项目路径中使用中文字符，如果必须使用:
```cmd
# 在Makefile中添加
export LC_ALL=C.UTF-8
```

## 🎯 推荐的工作流

### 开发流程 (VSCode)
1. **编辑代码** - 在VSCode中编辑
2. **编译** - `Ctrl+Shift+B` 或 `F5`
3. **烧录** - `Ctrl+Shift+P` → "Tasks: Run Task" → "Flash"
4. **调试** - `F5` 启动调试会话

### 命令行流程
```cmd
# 1. 编译
cd firmware
make

# 2. 烧录 (选择其一)
# OpenOCD方式
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg ^
  -c "program build/single_channel_sensor.elf verify reset exit"

# ST-Link方式
st-flash write build/single_channel_sensor.bin 0x8000000

# 3. 调试
# 终端1: 启动OpenOCD服务器
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg

# 终端2: 启动GDB
arm-none-eabi-gdb build/single_channel_sensor.elf
```

## 🔗 下载链接汇总

| 工具 | 链接 |
|------|------|
| ARM工具链 | https://developer.arm.com/downloads/-/gnu-rm |
| MSYS2 | https://www.msys2.org/ |
| OpenOCD (xPack) | https://github.com/xpack-dev-tools/openocd-xpack/releases |
| ST-Link工具 | https://github.com/stlink-org/stlink/releases |
| ST-Link驱动 | https://www.st.com/en/development-tools/stsw-link009.html |
| VSCode | https://code.visualstudio.com/ |

## 💡 提示

### 使用Git Bash
如果已安装Git for Windows，可以使用Git Bash作为Unix风格的终端:
```bash
cd /d/SensorProject/single_channel_system/firmware
make
```

### 使用WSL2
Windows 10/11用户也可以使用WSL2 (Windows Subsystem for Linux):
```bash
# 在WSL2中安装工具链
sudo apt-get update
sudo apt-get install gcc-arm-none-eabi make openocd

# 访问Windows文件系统
cd /mnt/d/SensorProject/single_channel_system/firmware
make
```

## 📞 获取帮助

遇到问题时:
1. 检查所有工具是否正确安装和添加到PATH
2. 重启终端/VSCode以刷新环境变量
3. 以管理员身份运行
4. 查看详细的错误信息
