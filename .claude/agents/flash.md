---
name: flash
description: 编译固件并烧录到 STM32，自动检测构建类型和调试器
---

# Flash Skill

收到 `/flash` 指令后，执行以下步骤：

## 1. 确认工作目录

切换到固件目录：
```
firmware/
```
（相对于仓库根目录）

## 2. 询问选项（如果用户未指定）

- **构建类型**：Debug（默认）或 Release
- **调试器**：daplink（默认）/ openocd / stlink / jlink

如果用户直接输入 `/flash` 不带参数，直接用默认值（Debug + daplink），不需要额外询问。

## 3. 执行构建

运行：
```powershell
.\tools\build.ps1
```
Release 构建：
```powershell
.\tools\build.ps1 -Release
```

- 如果构建**失败**：输出错误信息，停止，不执行烧录
- 如果构建**成功**：继续烧录

## 4. 执行烧录

```powershell
.\tools\flash.ps1 -Method daplink
```

根据用户选择的调试器替换 `-Method` 参数：
- `daplink`（默认，CMSIS-DAP）
- `openocd`（OpenOCD + ST-Link）
- `stlink`（st-flash）
- `jlink`（J-Link）

## 5. 输出结果

- 成功：告知烧录完成，并显示 Flash/RAM 占用
- 失败：显示 OpenOCD 错误信息，给出排查建议

## 使用示例

```
/flash                        # Debug 构建 + DAPLink 烧录
/flash release                # Release 构建 + DAPLink 烧录
/flash jlink                  # Debug 构建 + J-Link 烧录
/flash release stlink         # Release 构建 + ST-Link 烧录
```
