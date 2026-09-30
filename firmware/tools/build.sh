#!/bin/bash
# STM32 CMake 构建脚本

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 获取脚本所在目录
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"

# 默认配置
BUILD_TYPE="Debug"
CLEAN_BUILD=0
VERBOSE=0
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# 打印帮助信息
print_help() {
    cat << EOF
用法: $0 [选项]

选项:
    -h, --help          显示此帮助信息
    -r, --release       Release 构建（默认: Debug）
    -c, --clean         清理后重新构建
    -v, --verbose       显示详细编译信息
    -j N, --jobs N      并行编译任务数（默认: $JOBS）

示例:
    $0                  # Debug 构建
    $0 -r               # Release 构建
    $0 -c               # 清理并构建
    $0 -r -j 8          # Release 构建，使用 8 线程
EOF
}

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            print_help
            exit 0
            ;;
        -r|--release)
            BUILD_TYPE="Release"
            shift
            ;;
        -c|--clean)
            CLEAN_BUILD=1
            shift
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        *)
            echo -e "${RED}错误: 未知选项 $1${NC}"
            print_help
            exit 1
            ;;
    esac
done

# 打印信息
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}STM32 CMake 构建系统${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "项目目录: ${PROJECT_ROOT}"
echo -e "构建目录: ${BUILD_DIR}"
echo -e "构建类型: ${BUILD_TYPE}"
echo -e "并行任务: ${JOBS}"
echo -e "${BLUE}========================================${NC}"

# 检查工具链
echo -e "${YELLOW}>> 检查工具链...${NC}"
if ! command -v arm-none-eabi-gcc &> /dev/null; then
    echo -e "${RED}错误: 找不到 arm-none-eabi-gcc${NC}"
    echo -e "${YELLOW}请安装 ARM GNU Toolchain${NC}"
    exit 1
fi

if ! command -v cmake &> /dev/null; then
    echo -e "${RED}错误: 找不到 cmake${NC}"
    exit 1
fi

if ! command -v ninja &> /dev/null; then
    echo -e "${RED}错误: 找不到 ninja${NC}"
    exit 1
fi

echo -e "${GREEN}✓ 工具链检查通过${NC}"
echo -e "  GCC 版本: $(arm-none-eabi-gcc --version | head -n1)"
echo -e "  CMake 版本: $(cmake --version | head -n1)"
echo -e "  Ninja 版本: $(ninja --version)"

# 清理构建目录
if [ $CLEAN_BUILD -eq 1 ]; then
    echo -e "${YELLOW}>> 清理构建目录...${NC}"
    rm -rf "$BUILD_DIR"
    echo -e "${GREEN}✓ 清理完成${NC}"
fi

# CMake 配置
echo -e "${YELLOW}>> CMake 配置...${NC}"
cmake -B "$BUILD_DIR" \
      -S "$PROJECT_ROOT" \
      -G Ninja \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

if [ $? -ne 0 ]; then
    echo -e "${RED}✗ CMake 配置失败${NC}"
    exit 1
fi
echo -e "${GREEN}✓ CMake 配置完成${NC}"

# 构建
echo -e "${YELLOW}>> 开始构建...${NC}"
if [ $VERBOSE -eq 1 ]; then
    cmake --build "$BUILD_DIR" -j "$JOBS" --verbose
else
    cmake --build "$BUILD_DIR" -j "$JOBS"
fi

if [ $? -ne 0 ]; then
    echo -e "${RED}✗ 构建失败${NC}"
    exit 1
fi

echo -e "${GREEN}✓ 构建成功${NC}"

# 显示大小信息
echo -e "${BLUE}========================================${NC}"
echo -e "${YELLOW}>> 固件大小信息:${NC}"
arm-none-eabi-size "$BUILD_DIR/single_channel_sensor.elf"
echo -e "${BLUE}========================================${NC}"

# 输出文件列表
echo -e "${YELLOW}>> 生成的文件:${NC}"
ls -lh "$BUILD_DIR"/*.elf "$BUILD_DIR"/*.hex "$BUILD_DIR"/*.bin 2>/dev/null || true

echo -e "${GREEN}✓ 构建完成！${NC}"
echo -e "${BLUE}========================================${NC}"
