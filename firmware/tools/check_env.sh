#!/bin/bash
# 环境检查和安装脚本

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 检测操作系统
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        echo "linux"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
        echo "windows"
    else
        echo "unknown"
    fi
}

OS=$(detect_os)

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}STM32 开发环境检查工具${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "操作系统: ${OS}"
echo -e "${BLUE}========================================${NC}"
echo ""

# 检查工具是否存在
check_tool() {
    local tool=$1
    local name=$2

    if command -v "$tool" &> /dev/null; then
        local version=$($tool --version 2>&1 | head -n1)
        echo -e "${GREEN}✓${NC} $name: ${version}"
        return 0
    else
        echo -e "${RED}✗${NC} $name: 未安装"
        return 1
    fi
}

# 必需工具列表
REQUIRED_TOOLS=(
    "arm-none-eabi-gcc:ARM GCC"
    "cmake:CMake"
    "ninja:Ninja"
    "clangd:clangd"
)

# 可选工具列表
OPTIONAL_TOOLS=(
    "openocd:OpenOCD"
    "st-flash:ST-Link Tools"
    "JLinkExe:J-Link"
    "gdb-multiarch:GDB Multiarch"
)

echo -e "${YELLOW}>> 检查必需工具...${NC}"
echo ""

MISSING_REQUIRED=0
for tool_pair in "${REQUIRED_TOOLS[@]}"; do
    IFS=':' read -r tool name <<< "$tool_pair"
    if ! check_tool "$tool" "$name"; then
        MISSING_REQUIRED=1
    fi
done

echo ""
echo -e "${YELLOW}>> 检查可选工具...${NC}"
echo ""

MISSING_OPTIONAL=0
for tool_pair in "${OPTIONAL_TOOLS[@]}"; do
    IFS=':' read -r tool name <<< "$tool_pair"
    if ! check_tool "$tool" "$name"; then
        MISSING_OPTIONAL=1
    fi
done

echo ""
echo -e "${BLUE}========================================${NC}"

# 如果有缺失的必需工具，显示安装建议
if [ $MISSING_REQUIRED -eq 1 ]; then
    echo -e "${RED}发现缺失的必需工具！${NC}"
    echo ""
    echo -e "${YELLOW}安装建议:${NC}"
    echo ""

    case $OS in
        linux)
            echo -e "${BLUE}Ubuntu/Debian:${NC}"
            echo "  sudo apt update"
            echo "  sudo apt install cmake ninja-build gcc-arm-none-eabi clangd-14"
            echo ""
            ;;
        macos)
            echo -e "${BLUE}macOS (使用 Homebrew):${NC}"
            echo "  brew install cmake ninja arm-none-eabi-gcc llvm"
            echo "  # 添加 LLVM 到 PATH"
            echo "  echo 'export PATH=\"/opt/homebrew/opt/llvm/bin:\$PATH\"' >> ~/.zshrc"
            echo ""
            ;;
        windows)
            echo -e "${BLUE}Windows (使用 Chocolatey):${NC}"
            echo "  choco install cmake ninja llvm"
            echo "  # ARM GCC 需要手动下载安装"
            echo "  # https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads"
            echo ""
            ;;
    esac

    exit 1
fi

if [ $MISSING_OPTIONAL -eq 1 ]; then
    echo -e "${YELLOW}部分可选工具未安装${NC}"
    echo -e "${YELLOW}这些工具用于烧录和调试，如需使用请参考文档安装${NC}"
    echo ""
fi

echo -e "${GREEN}✓ 所有必需工具已安装！${NC}"
echo ""

# VSCode 检查
echo -e "${YELLOW}>> 检查 VSCode...${NC}"
echo ""

if command -v code &> /dev/null; then
    echo -e "${GREEN}✓${NC} VSCode 已安装"

    # 检查推荐扩展
    echo ""
    echo -e "${YELLOW}>> 检查 VSCode 扩展...${NC}"
    echo ""

    EXTENSIONS=(
        "ms-vscode.cmake-tools:CMake Tools"
        "llvm-vs-code-extensions.vscode-clangd:clangd"
        "marus25.cortex-debug:Cortex-Debug"
    )

    for ext_pair in "${EXTENSIONS[@]}"; do
        IFS=':' read -r ext_id ext_name <<< "$ext_pair"
        if code --list-extensions | grep -q "$ext_id"; then
            echo -e "${GREEN}✓${NC} $ext_name"
        else
            echo -e "${YELLOW}○${NC} $ext_name (未安装)"
        fi
    done

    echo ""
    echo -e "${YELLOW}安装缺失的扩展:${NC}"
    echo "  打开项目后 VSCode 会自动提示安装推荐扩展"
    echo "  或运行: code --install-extension <extension-id>"
else
    echo -e "${YELLOW}○${NC} VSCode 未安装或不在 PATH 中"
fi

echo ""
echo -e "${BLUE}========================================${NC}"

# 测试构建
echo -e "${YELLOW}>> 测试构建系统...${NC}"
echo ""

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

if [ -f "$PROJECT_ROOT/CMakeLists.txt" ]; then
    echo -e "${GREEN}✓${NC} 找到 CMakeLists.txt"

    # 尝试配置项目
    echo ""
    echo -e "${YELLOW}尝试配置项目...${NC}"

    TEST_BUILD_DIR="$PROJECT_ROOT/build_test"

    if cmake -B "$TEST_BUILD_DIR" -S "$PROJECT_ROOT" -G Ninja -DCMAKE_BUILD_TYPE=Debug &> /dev/null; then
        echo -e "${GREEN}✓${NC} CMake 配置成功"
        rm -rf "$TEST_BUILD_DIR"
    else
        echo -e "${RED}✗${NC} CMake 配置失败"
        echo -e "${YELLOW}请检查 CMakeLists.txt 或工具链配置${NC}"
    fi
else
    echo -e "${YELLOW}○${NC} 未找到 CMakeLists.txt"
fi

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}环境检查完成！${NC}"
echo ""
echo -e "${YELLOW}下一步:${NC}"
echo "  1. 在 VSCode 中打开项目"
echo "  2. 安装推荐扩展"
echo "  3. 运行构建: ./tools/build.sh 或 Ctrl+Shift+B"
echo "  4. 阅读完整文档: docs/CMAKE_SETUP_GUIDE.md"
echo ""
echo -e "${BLUE}========================================${NC}"
