#!/bin/bash
# STM32 调试脚本

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 获取脚本所在目录
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"
ELF_FILE="${BUILD_DIR}/single_channel_sensor.elf"

# 默认配置
METHOD="openocd"
GDB_PORT=3333

print_help() {
    cat << EOF
用法: $0 [选项]

选项:
    -h, --help          显示此帮助信息
    -m, --method METHOD 调试方法: openocd (默认), jlink
    -p, --port PORT     GDB 端口（默认: 3333）

示例:
    $0                  # 使用 OpenOCD 调试
    $0 -m jlink         # 使用 J-Link 调试
EOF
}

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            print_help
            exit 0
            ;;
        -m|--method)
            METHOD="$2"
            shift 2
            ;;
        -p|--port)
            GDB_PORT="$2"
            shift 2
            ;;
        *)
            echo -e "${RED}错误: 未知选项 $1${NC}"
            print_help
            exit 1
            ;;
    esac
done

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}STM32 调试工具${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "调试方法: ${METHOD}"
echo -e "GDB 端口: ${GDB_PORT}"
echo -e "ELF 文件: ${ELF_FILE}"
echo -e "${BLUE}========================================${NC}"

# 检查固件文件
if [ ! -f "$ELF_FILE" ]; then
    echo -e "${RED}错误: 找不到 ELF 文件${NC}"
    echo -e "${YELLOW}请先运行构建: ./tools/build.sh${NC}"
    exit 1
fi

# 创建临时 GDB 初始化脚本
GDB_INIT=$(mktemp)
cat > "$GDB_INIT" << EOF
target remote :${GDB_PORT}
monitor reset halt
load
break main
continue
EOF

echo -e "${YELLOW}>> 启动调试会话...${NC}"
echo -e "${YELLOW}提示: 在另一个终端运行调试服务器${NC}"
echo ""

case $METHOD in
    openocd)
        echo -e "${BLUE}调试服务器命令:${NC}"
        echo -e "  openocd -f interface/stlink.cfg -f target/stm32f1x.cfg"
        ;;
    jlink)
        echo -e "${BLUE}调试服务器命令:${NC}"
        echo -e "  JLinkGDBServer -device STM32F103C8 -if SWD -speed 4000"
        ;;
esac

echo ""
echo -e "${YELLOW}>> 启动 GDB...${NC}"

# 启动 GDB
arm-none-eabi-gdb \
    -x "$GDB_INIT" \
    "$ELF_FILE"

# 清理临时文件
rm "$GDB_INIT"

echo -e "${GREEN}✓ 调试会话结束${NC}"
