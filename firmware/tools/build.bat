@echo off
REM STM32 CMake 构建脚本 (Windows)

setlocal EnableDelayedExpansion

set "BUILD_TYPE=Debug"
set "CLEAN_BUILD=0"
set "JOBS=8"

:parse_args
if "%~1"=="" goto check_tools
if /i "%~1"=="-h" goto help
if /i "%~1"=="--help" goto help
if /i "%~1"=="-r" (
    set "BUILD_TYPE=Release"
    shift
    goto parse_args
)
if /i "%~1"=="--release" (
    set "BUILD_TYPE=Release"
    shift
    goto parse_args
)
if /i "%~1"=="-c" (
    set "CLEAN_BUILD=1"
    shift
    goto parse_args
)
if /i "%~1"=="--clean" (
    set "CLEAN_BUILD=1"
    shift
    goto parse_args
)
if /i "%~1"=="-j" (
    set "JOBS=%~2"
    shift
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
echo     -r, --release       Release 构建（默认: Debug）
echo     -c, --clean         清理后重新构建
echo     -j N                并行编译任务数（默认: 8）
echo.
echo 示例:
echo     %~nx0                  # Debug 构建
echo     %~nx0 -r               # Release 构建
echo     %~nx0 -c               # 清理并构建
exit /b 0

:check_tools
echo ========================================
echo STM32 CMake 构建系统
echo ========================================
echo 构建类型: %BUILD_TYPE%
echo 并行任务: %JOBS%
echo ========================================

echo.
echo ^>^> 检查工具链...

where arm-none-eabi-gcc >nul 2>&1
if errorlevel 1 (
    echo [错误] 找不到 arm-none-eabi-gcc
    echo 请安装 ARM GNU Toolchain 并添加到 PATH
    exit /b 1
)

where cmake >nul 2>&1
if errorlevel 1 (
    echo [错误] 找不到 cmake
    exit /b 1
)

where ninja >nul 2>&1
if errorlevel 1 (
    echo [错误] 找不到 ninja
    exit /b 1
)

echo [OK] 工具链检查通过

REM 获取版本信息
for /f "tokens=*" %%i in ('arm-none-eabi-gcc --version ^| findstr /r "^arm"') do echo   GCC: %%i
for /f "tokens=*" %%i in ('cmake --version ^| findstr /r "^cmake"') do echo   CMake: %%i
for /f "tokens=*" %%i in ('ninja --version') do echo   Ninja: %%i

:build
cd /d "%~dp0.."
set "BUILD_DIR=%CD%\build"

if "%CLEAN_BUILD%"=="1" (
    echo.
    echo ^>^> 清理构建目录...
    if exist "%BUILD_DIR%" (
        rmdir /s /q "%BUILD_DIR%"
    )
    echo [OK] 清理完成
)

echo.
echo ^>^> CMake 配置...
cmake -B "%BUILD_DIR%" ^
      -S "%CD%" ^
      -G Ninja ^
      -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

if errorlevel 1 (
    echo [错误] CMake 配置失败
    exit /b 1
)
echo [OK] CMake 配置完成

echo.
echo ^>^> 开始构建...
cmake --build "%BUILD_DIR%" -j %JOBS%

if errorlevel 1 (
    echo [错误] 构建失败
    exit /b 1
)

echo [OK] 构建成功

echo.
echo ========================================
echo ^>^> 固件大小信息:
arm-none-eabi-size "%BUILD_DIR%\single_channel_sensor.elf"
echo ========================================

echo.
echo ^>^> 生成的文件:
dir /b "%BUILD_DIR%\*.elf" "%BUILD_DIR%\*.hex" "%BUILD_DIR%\*.bin" 2>nul

echo.
echo [OK] 构建完成！
echo ========================================

endlocal
