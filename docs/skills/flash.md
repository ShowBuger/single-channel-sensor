---
name: flash
description: 编译固件并烧录到 STM32。直接调用 firmware/tools/build.ps1 和 flash.ps1，调试器由脚本自动检测
---

# Flash Skill

收到 `/flash` 指令后，**直接运行下面的脚本**，不要先浏览代码、阅读脚本内容或询问用户。
脚本内部已处理工具链检查、路径解析和调试器检测。

## 1. 定位脚本

脚本位于仓库的 `firmware/tools/`。用 git 取仓库根目录，拼出绝对路径：

```powershell
$root = git rev-parse --show-toplevel
$tools = Join-Path $root "firmware\tools"
```

脚本内部基于自身路径定位工程，**不依赖当前工作目录**，无需 `cd`。

## 2. 解析参数（不要询问用户）

从用户输入中解析，未指定则用默认值：

| 用户输入 | 含义 | 默认 |
|----------|------|------|
| `release` | Release 构建 | Debug |
| `clean` | 清理后重编 | 不清理 |
| `cubeprog` / `openocd` / `daplink` / `stlink` / `jlink` | 指定调试器 | `auto` |

调试器默认 **不传 `-Method`**，让 `flash.ps1` 自动检测
（依次尝试 STM32CubeProgrammer → OpenOCD+DAPLink → OpenOCD+ST-Link）。

## 3. 编译

```powershell
& "$tools\build.ps1"                 # Debug
& "$tools\build.ps1" -Release        # Release
& "$tools\build.ps1" -Clean          # 清理后重编（可与 -Release 组合）
```

- 退出码非 0：输出编译错误，**停止，不烧录**
- 成功：记下输出中的 Flash/RAM 占用，继续烧录

## 4. 烧录

```powershell
& "$tools\flash.ps1"                     # 自动检测调试器（默认）
& "$tools\flash.ps1" -Method openocd     # 用户指定了调试器时
```

`-Method` 可选值：`cubeprog`、`openocd`（OpenOCD + ST-Link）、`daplink`（OpenOCD + CMSIS-DAP）、`stlink`（st-flash）、`jlink`。

- 用户指定的调试器失败时，可去掉 `-Method` 用自动检测重试一次
- **禁止**手动执行 `openocd ... stm32l4x lock/unlock`，会设置 RDP 读保护导致无法再烧录

## 5. 输出结果

- 成功：构建类型、Flash/RAM 占用、实际使用的调试器、是否 Verified OK
- 失败：关键错误输出（OpenOCD/CubeProgrammer 报错原文）和排查建议（调试器是否连接、目标板是否上电）

## 使用示例

```
/flash                        # Debug 构建 + 自动检测调试器
/flash release                # Release 构建 + 自动检测调试器
/flash openocd                # Debug 构建 + OpenOCD/ST-Link
/flash release clean jlink    # 清理 + Release 构建 + J-Link
```
