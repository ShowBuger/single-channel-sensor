@echo off
REM STM32 烧录脚本 (Windows)

setlocal EnableDelayedExpansion

set "METHOD=openocd"
set "VERIFY=1"

:parse_args
if "%~1"=="" goto check_firmware
if /i "%~1"=="-h" goto help
if /i "%~1"=="--help" goto help
if /i "%~1"=="-m" (
    set "METHOD=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--method" (
    set "METHOD=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--no-verify" (
    set "VERIFY=0"
    shift
    goto parse_args
)
echo 错误: 未知选项 %~1
goto help

:help
echo 用法: %~nx0 [选项]
echo.
echo 选项:
echo     -h, --help          显示此帮助信息
echo     -m, --method METHOD 烧录方法: openocd (默认), stlink, jlink
echo     --no-verify         不验证烧录结果
echo.
echo 示例:
echo     %~nx0                  # 使用 OpenOCD 烧录
echo     %~nx0 -m stlink        # 使用 st-flash 烧录
exit /b 0

:check_firmware
cd /d "%~dp0.."
set "BUILD_DIR=%CD%\build"
set "ELF_FILE=%BUILD_DIR%\single_channel_sensor.elf"
set "BIN_FILE=%BUILD_DIR%\single_channel_sensor.bin"

echo ========================================
echo STM32 烧录工具
echo ========================================
echo 烧录方法: %METHOD%
echo 固件文件: %ELF_FILE%
echo ========================================

if not exist "%ELF_FILE%" (
    echo [错误] 找不到固件文件
    echo 请先运行构建: tools\build.bat
    exit /b 1
)

echo.
echo ^>^> 固件信息:
arm-none-eabi-size "%ELF_FILE%"
echo.

:flash
if /i "%METHOD%"=="openocd" goto flash_openocd
if /i "%METHOD%"=="stlink" goto flash_stlink
if /i "%METHOD%"=="jlink" goto flash_jlink
echo [错误] 不支持的烧录方法: %METHOD%
goto help

:flash_openocd
echo ^>^> 使用 OpenOCD 烧录...
where openocd >nul 2>&1
if errorlevel 1 (
    echo [错误] 找不到 openocd
    exit /b 1
)

set "VERIFY_ARG="
if "%VERIFY%"=="1" set "VERIFY_ARG=verify"

openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program %ELF_FILE% %VERIFY_ARG% reset exit"
goto check_result

:flash_stlink
echo ^>^> 使用 st-flash 烧录...
where st-flash >nul 2>&1
if errorlevel 1 (
    echo [错误] 找不到 st-flash
    exit /b 1
)

if not exist "%BIN_FILE%" (
    echo [错误] 找不到 BIN 文件
    exit /b 1
)

st-flash write "%BIN_FILE%" 0x8000000
goto check_result

:flash_jlink
echo ^>^> 使用 J-Link 烧录...
where JLink >nul 2>&1
if errorlevel 1 (
    echo [错误] 找不到 JLink
    exit /b 1
)

set "JLINK_SCRIPT=%TEMP%\jlink_flash.jlink"
(
echo si SWD
echo speed 4000
echo device STM32F103C8
echo loadfile "%ELF_FILE%"
echo r
echo g
echo qc
) > "%JLINK_SCRIPT%"

JLink -CommanderScript "%JLINK_SCRIPT%"
del "%JLINK_SCRIPT%"
goto check_result

:check_result
if errorlevel 1 (
    echo [错误] 烧录失败
    exit /b 1
)

echo [OK] 烧录成功！
echo ========================================

endlocal
