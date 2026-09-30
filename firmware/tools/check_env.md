# Windows环境快速检查清单

运行此脚本检查开发环境是否配置完整。

## 检查脚本

将以下内容保存为 `check_env.bat`：

```batch
@echo off
echo ==========================================
echo   STM32开发环境检查
echo ==========================================
echo.

echo [1/5] 检查 ARM 工具链...
where arm-none-eabi-gcc >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] arm-none-eabi-gcc 已安装
    arm-none-eabi-gcc --version | findstr "gcc"
) else (
    echo [FAIL] arm-none-eabi-gcc 未找到
    echo       请安装 GNU Arm Embedded Toolchain
    echo       https://developer.arm.com/downloads/-/gnu-rm
)
echo.

echo [2/5] 检查 Make 工具...
where make >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] make 已安装
    make --version | findstr "Make"
) else (
    where mingw32-make >nul 2>&1
    if %errorlevel% equ 0 (
        echo [OK] mingw32-make 已安装 ^(可创建 make 别名^)
        mingw32-make --version | findstr "Make"
    ) else (
        echo [FAIL] make 或 mingw32-make 未找到
        echo       请安装 MSYS2 或 MinGW
    )
)
echo.

echo [3/5] 检查 OpenOCD...
where openocd >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] openocd 已安装
    openocd --version 2>&1 | findstr "Open On-Chip Debugger"
) else (
    echo [WARN] openocd 未找到 ^(可选，用于调试^)
    echo        https://github.com/xpack-dev-tools/openocd-xpack/releases
)
echo.

echo [4/5] 检查 ST-Link 工具...
where st-flash >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] st-flash 已安装
) else (
    where STM32_Programmer_CLI >nul 2>&1
    if %errorlevel% equ 0 (
        echo [OK] STM32_Programmer_CLI 已安装
    ) else (
        echo [WARN] ST-Link 工具未找到 ^(可选，用于烧录^)
        echo        https://github.com/stlink-org/stlink/releases
    )
)
echo.

echo [5/5] 检查 Git...
where git >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] git 已安装
    git --version
) else (
    echo [WARN] git 未找到 ^(推荐安装^)
)
echo.

echo ==========================================
echo   检查完成
echo ==========================================
echo.
echo 必需工具:
echo   - ARM 工具链: arm-none-eabi-gcc
echo   - Make 工具: make 或 mingw32-make
echo.
echo 可选工具:
echo   - OpenOCD: 用于调试
echo   - ST-Link: 用于烧录
echo   - Git: 版本控制
echo.
pause
```

## PowerShell版本

保存为 `check_env.ps1`：

```powershell
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "  STM32开发环境检查" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

function Test-Command {
    param($cmdname)
    return [bool](Get-Command -Name $cmdname -ErrorAction SilentlyContinue)
}

# 1. ARM工具链
Write-Host "[1/5] 检查 ARM 工具链..." -ForegroundColor Yellow
if (Test-Command arm-none-eabi-gcc) {
    Write-Host "[OK] arm-none-eabi-gcc 已安装" -ForegroundColor Green
    arm-none-eabi-gcc --version | Select-String "gcc" | Write-Host
} else {
    Write-Host "[FAIL] arm-none-eabi-gcc 未找到" -ForegroundColor Red
    Write-Host "      请安装 GNU Arm Embedded Toolchain" -ForegroundColor Red
    Write-Host "      https://developer.arm.com/downloads/-/gnu-rm"
}
Write-Host ""

# 2. Make工具
Write-Host "[2/5] 检查 Make 工具..." -ForegroundColor Yellow
if (Test-Command make) {
    Write-Host "[OK] make 已安装" -ForegroundColor Green
    make --version | Select-String "Make" | Write-Host
} elseif (Test-Command mingw32-make) {
    Write-Host "[OK] mingw32-make 已安装 (可创建 make 别名)" -ForegroundColor Green
    mingw32-make --version | Select-String "Make" | Write-Host
} else {
    Write-Host "[FAIL] make 未找到" -ForegroundColor Red
    Write-Host "      请安装 MSYS2 或 MinGW" -ForegroundColor Red
}
Write-Host ""

# 3. OpenOCD
Write-Host "[3/5] 检查 OpenOCD..." -ForegroundColor Yellow
if (Test-Command openocd) {
    Write-Host "[OK] openocd 已安装" -ForegroundColor Green
    openocd --version 2>&1 | Select-String "Open On-Chip Debugger" | Write-Host
} else {
    Write-Host "[WARN] openocd 未找到 (可选，用于调试)" -ForegroundColor DarkYellow
    Write-Host "       https://github.com/xpack-dev-tools/openocd-xpack/releases"
}
Write-Host ""

# 4. ST-Link工具
Write-Host "[4/5] 检查 ST-Link 工具..." -ForegroundColor Yellow
if (Test-Command st-flash) {
    Write-Host "[OK] st-flash 已安装" -ForegroundColor Green
} elseif (Test-Command STM32_Programmer_CLI) {
    Write-Host "[OK] STM32_Programmer_CLI 已安装" -ForegroundColor Green
} else {
    Write-Host "[WARN] ST-Link 工具未找到 (可选，用于烧录)" -ForegroundColor DarkYellow
    Write-Host "       https://github.com/stlink-org/stlink/releases"
}
Write-Host ""

# 5. Git
Write-Host "[5/5] 检查 Git..." -ForegroundColor Yellow
if (Test-Command git) {
    Write-Host "[OK] git 已安装" -ForegroundColor Green
    git --version | Write-Host
} else {
    Write-Host "[WARN] git 未找到 (推荐安装)" -ForegroundColor DarkYellow
}
Write-Host ""

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "  检查完成" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "必需工具:"
Write-Host "  - ARM 工具链: arm-none-eabi-gcc"
Write-Host "  - Make 工具: make 或 mingw32-make"
Write-Host ""
Write-Host "可选工具:"
Write-Host "  - OpenOCD: 用于调试"
Write-Host "  - ST-Link: 用于烧录"
Write-Host "  - Git: 版本控制"
Write-Host ""
