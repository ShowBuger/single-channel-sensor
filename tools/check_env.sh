#!/bin/bash
# STM32 VSCode开发环境检查脚本

echo "=========================================="
echo "  STM32 VSCode开发环境检查"
echo "=========================================="
echo ""

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

check_pass=0
check_fail=0
check_warn=0

# 检查函数
check_command() {
    if command -v $1 &> /dev/null; then
        echo -e "${GREEN}[OK]${NC} $2"
        ((check_pass++))
        return 0
    else
        echo -e "${RED}[FAIL]${NC} $2"
        ((check_fail++))
        return 1
    fi
}

check_optional() {
    if command -v $1 &> /dev/null; then
        echo -e "${GREEN}[OK]${NC} $2"
        ((check_pass++))
        return 0
    else
        echo -e "${YELLOW}[WARN]${NC} $2 (可选)"
        ((check_warn++))
        return 1
    fi
}

check_file() {
    if [ -f "$1" ]; then
        echo -e "${GREEN}[OK]${NC} $2"
        ((check_pass++))
        return 0
    else
        echo -e "${RED}[FAIL]${NC} $2"
        ((check_fail++))
        return 1
    fi
}

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "1. 检查项目配置文件"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
check_file "platformio.ini" "PlatformIO配置文件"
check_file "firmware/Makefile" "Makefile构建脚本"
check_file "firmware/STM32F103C8Tx_FLASH.ld" "链接脚本"
check_file ".vscode/tasks.json" "VSCode任务配置"
check_file ".vscode/launch.json" "VSCode调试配置"
check_file ".vscode/c_cpp_properties.json" "IntelliSense配置"
echo ""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "2. 检查PlatformIO"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if command -v pio &> /dev/null; then
    echo -e "${GREEN}[OK]${NC} PlatformIO CLI已安装"
    pio --version
    ((check_pass++))
else
    echo -e "${YELLOW}[INFO]${NC} PlatformIO CLI未在PATH中（这是正常的）"
    echo "      PlatformIO通过VSCode扩展工作"

    # 检查Windows上的PlatformIO安装
    if [ -d "$HOME/.platformio" ] || [ -d "/c/Users/$USER/.platformio" ]; then
        echo -e "${GREEN}[OK]${NC} PlatformIO已安装在用户目录"
        ((check_pass++))
    else
        echo -e "${YELLOW}[WARN]${NC} 未找到PlatformIO安装目录"
        echo "      请在VSCode中打开项目，PlatformIO会自动初始化"
        ((check_warn++))
    fi
fi
echo ""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "3. 检查ARM工具链（Makefile方案）"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
check_optional "arm-none-eabi-gcc" "ARM GCC编译器"
if command -v arm-none-eabi-gcc &> /dev/null; then
    arm-none-eabi-gcc --version | head -1
fi

check_optional "make" "Make构建工具"
if ! command -v make &> /dev/null; then
    check_optional "mingw32-make" "MinGW Make"
fi
echo ""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "4. 检查调试和烧录工具（可选）"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
check_optional "openocd" "OpenOCD调试工具"
check_optional "st-flash" "ST-Link命令行工具"
check_optional "STM32_Programmer_CLI" "STM32 Programmer CLI"
echo ""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "5. 检查源代码结构"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
check_file "firmware/src/main.c" "主程序文件"
check_file "firmware/libs/CMSIS/stm32f10x.h" "CMSIS头文件"

if [ -d "firmware/src/app" ]; then
    echo -e "${GREEN}[OK]${NC} 应用层代码目录"
    ((check_pass++))
else
    echo -e "${RED}[FAIL]${NC} 应用层代码目录"
    ((check_fail++))
fi

if [ -d "firmware/src/drivers" ]; then
    echo -e "${GREEN}[OK]${NC} 驱动层代码目录"
    ((check_pass++))
else
    echo -e "${RED}[FAIL]${NC} 驱动层代码目录"
    ((check_fail++))
fi
echo ""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "6. 检查Git状态"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
if [ -d ".git" ]; then
    echo -e "${GREEN}[OK]${NC} Git仓库已初始化"
    ((check_pass++))

    if command -v git &> /dev/null; then
        echo ""
        echo "当前分支: $(git branch --show-current)"
        echo "Git状态:"
        git status --short | head -10
    fi
else
    echo -e "${YELLOW}[WARN]${NC} 未初始化Git仓库"
    ((check_warn++))
fi
echo ""

echo "=========================================="
echo "  检查结果汇总"
echo "=========================================="
echo -e "${GREEN}通过:${NC} $check_pass"
echo -e "${YELLOW}警告:${NC} $check_warn"
echo -e "${RED}失败:${NC} $check_fail"
echo ""

if [ $check_fail -eq 0 ]; then
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "✅ 开发环境配置完成！"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""
    echo "📋 下一步操作:"
    echo ""
    echo "方案A: 使用PlatformIO (推荐)"
    echo "  1. 在VSCode中打开项目文件夹"
    echo "  2. 等待PlatformIO初始化（首次5-10分钟）"
    echo "  3. 按 Ctrl+Alt+B 编译"
    echo "  4. 按 Ctrl+Alt+U 上传"
    echo "  5. 按 F5 调试"
    echo ""
    echo "方案B: 使用Makefile"
    if ! command -v arm-none-eabi-gcc &> /dev/null; then
        echo "  ⚠️  需要先安装ARM工具链"
        echo "  参考: docs/WINDOWS_SETUP.md"
    else
        echo "  1. cd firmware"
        echo "  2. make"
        echo "  3. make flash (需要配置OpenOCD或ST-Link)"
    fi
    echo ""
    echo "📚 文档位置:"
    echo "  • docs/MIGRATION_SUMMARY.md - 迁移总结"
    echo "  • docs/QUICK_START_PLATFORMIO.md - PlatformIO快速开始"
    echo "  • docs/WINDOWS_SETUP.md - Windows环境配置"
    echo ""
else
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "⚠️  发现 $check_fail 个问题需要解决"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""
    echo "请检查上述失败的项目并修复"
    echo "参考文档: docs/MIGRATION_SUMMARY.md"
    echo ""
fi

echo "=========================================="
