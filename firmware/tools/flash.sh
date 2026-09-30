#!/bin/bash
# STM32 烧录脚本

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

# 固件文件
ELF_FILE="${BUILD_DIR}/single_channel_sensor.elf"
BIN_FILE="${BUILD_DIR}/single_channel_sensor.bin"

# 默认配置
METHOD="openocd"
VERIFY=1

# 打印帮助信息
print_help() {
    cat << EOF
用法: $0 [选项]

选项:
    -h, --help          显示此帮助信息
    -m, --method METHOD 烧录方法: openocd (默认), stlink, jlink
    --no-verify         不验证烧录结果

示例:
    $0                  # 使用 OpenOCD 烧录
    $0 -m stlink        # 使用 st-flash 烧录
    $0 -m jlink         # 使用 J-Link 烧录
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
        --no-verify)
            VERIFY=0
            shift
            ;;
        *)
            echo -e "${RED}错误: 未知选项 $1${NC}"
            print_help
            exit 1
            ;;
    esac
done

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}STM32 烧录工具${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "烧录方法: ${METHOD}"
echo -e "固件文件: ${ELF_FILE}"
echo -e "${BLUE}========================================${NC}"

# 检查固件文件
if [ ! -f "$ELF_FILE" ]; then
    echo -e "${RED}错误: 找不到固件文件${NC}"
    echo -e "${YELLOW}请先运行构建: ./tools/build.sh${NC}"
    exit 1
fi

# 显示固件信息
echo -e "${YELLOW}>> 固件信息:${NC}"
arm-none-eabi-size "$ELF_FILE"
echo ""

# 根据方法烧录
case $METHOD in
    openocd)
        echo -e "${YELLOW}>> 使用 OpenOCD 烧录...${NC}"

        if ! command -v openocd &> /dev/null; then
            echo -e "${RED}错误: 找不到 openocd${NC}"
            exit 1
        fi

        VERIFY_ARG=""
        if [ $VERIFY -eq 1 ]; then
            VERIFY_ARG="verify"
        fi

        openocd \
            -f interface/stlink.cfg \
            -f target/stm32f1x.cfg \
            -c "program $ELF_FILE $VERIFY_ARG reset exit"
        ;;

    stlink)
        echo -e "${YELLOW}>> 使用 st-flash 烧录...${NC}"

        if ! command -v st-flash &> /dev/null; then
            echo -e "${RED}错误: 找不到 st-flash${NC}"
            exit 1
        fi

        if [ ! -f "$BIN_FILE" ]; then
            echo -e "${RED}错误: 找不到 BIN 文件${NC}"
            exit 1
        fi

        st-flash write "$BIN_FILE" 0x8000000
        ;;

    jlink)
        echo -e "${YELLOW}>> 使用 J-Link 烧录...${NC}"

        if ! command -v JLinkExe &> /dev/null; then
            echo -e "${RED}错误: 找不到 JLinkExe${NC}"
            exit 1
        fi

        # 创建临时 J-Link 脚本
        JLINK_SCRIPT=$(mktemp)
        cat > "$JLINK_SCRIPT" << EOF
si SWD
speed 4000
device STM32F103C8
loadfile $ELF_FILE
r
g
qc
EOF

        JLinkExe -CommanderScript "$JLINK_SCRIPT"
        rm "$JLINK_SCRIPT"
        ;;

    *)
        echo -e "${RED}错误: 不支持的烧录方法: $METHOD${NC}"
        print_help
        exit 1
        ;;
esac

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ 烧录成功！${NC}"
else
    echo -e "${RED}✗ 烧录失败${NC}"
    exit 1
fi

echo -e "${BLUE}========================================${NC}"
