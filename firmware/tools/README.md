# 构建与烧录工具说明

本目录包含 STM32 固件开发的全套 PowerShell 脚本。

---

## 脚本一览

| 脚本 | 用途 |
|------|------|
| `install_toolchain.ps1` | 首次安装：下载并配置 ARM GNU 工具链 |
| `env_setup.ps1` | 每次开终端后加载工具链到 PATH |
| `build.ps1` | 编译固件 |
| `flash.ps1` | 烧录固件到芯片 |
| `_ensure_toolchain.ps1` | 内部脚本，被 build/flash 自动调用，勿手动运行 |

---

## 快速开始

### 第一次使用（只需做一次）

```powershell
.\tools\install_toolchain.ps1
```

从 ARM 官网下载 Toolchain 15.3（约 1 GB），解压到 `D:\toolchain\`，并自动更新 `env_setup.ps1` 中的路径。

如需安装到其他目录：

```powershell
.\tools\install_toolchain.ps1 -InstallDir C:\toolchain
```

---

### 每次打开新终端后

```powershell
. .\tools\env_setup.ps1
```

注意开头的 `.` 是 dot-source，不能省略，否则 PATH 只在子进程生效。

> 如果已经将工具链永久加入用户 PATH（安装时选了 `y`），此步可跳过。

---

### 编译固件

```powershell
# Debug 构建（默认）
.\tools\build.ps1

# Release 构建
.\tools\build.ps1 -Release

# 清理后重新编译
.\tools\build.ps1 -Clean

# Release + 清理 + 8 线程
.\tools\build.ps1 -Release -Clean -Jobs 8
```

编译产物输出到 `build\`：

- `single_channel_sensor.elf` — 含调试信息，GDB 使用
- `single_channel_sensor.hex` — 烧录用
- `single_channel_sensor.bin` — 烧录用
- `single_channel_sensor.map` — 内存占用分析

---

### 烧录固件

```powershell
# OpenOCD + ST-Link（默认）
.\tools\flash.ps1

# ST-Link（stlink-tools）
.\tools\flash.ps1 -Method stlink

# J-Link
.\tools\flash.ps1 -Method jlink

# 指定 HEX 文件
.\tools\flash.ps1 -HexFile D:\path\to\custom.hex

# 跳过烧录后校验（更快）
.\tools\flash.ps1 -NoVerify

# 烧录后不复位
.\tools\flash.ps1 -NoReset
```

> build.ps1 和 flash.ps1 在运行前会自动检查工具链是否就绪，
> 未安装时会自动触发 `install_toolchain.ps1`，无需手动干预。

---

## 典型工作流

```powershell
# 克隆仓库后第一次
.\tools\install_toolchain.ps1

# 日常开发
. .\tools\env_setup.ps1          # 加载环境
.\tools\build.ps1                # 编译
.\tools\flash.ps1                # 烧录
```
