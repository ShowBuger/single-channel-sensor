cmake_minimum_required(VERSION 3.22)

# 设置工具链文件（必须在project()之前）
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi-gcc.cmake)

# 项目信息
project(single_channel_sensor
    VERSION 1.0.0
    LANGUAGES C ASM
)

# 设置C标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# 输出编译数据库供clangd使用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# MCU配置
set(MCU_FAMILY STM32F1)
set(MCU_MODEL STM32F103xB)
set(MCU_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32F103C8Tx_FLASH.ld)

# CPU选项
set(CPU_OPTIONS
    -mcpu=cortex-m3
    -mthumb
    -mfloat-abi=soft
)

# 编译器定义
add_compile_definitions(
    USE_STDPERIPH_DRIVER
    STM32F10X_MD
    ${MCU_MODEL}
)

# 编译选项
add_compile_options(
    ${CPU_OPTIONS}
    -fdata-sections
    -ffunction-sections
    -fno-common
    -fmessage-length=0
    -Wall
    -Wextra
    -Wno-unused-parameter
    $<$<CONFIG:Debug>:-Og>
    $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Debug>:-ggdb>
    $<$<CONFIG:Debug>:-DDEBUG>
    $<$<CONFIG:Release>:-Os>
    $<$<CONFIG:Release>:-DNDEBUG>
)

# 链接选项
add_link_options(
    ${CPU_OPTIONS}
    -T${MCU_LINKER_SCRIPT}
    -Wl,-Map=${PROJECT_NAME}.map
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    --specs=nano.specs
    --specs=nosys.specs
)

# ===== 包含目录 =====
include_directories(
    src
    src/app
    src/drivers/bsp
    src/drivers/sensors
    libs/CMSIS
    libs/STM32F10x_StdPeriph_Driver
)

# ===== 源文件收集 =====

# CMSIS
file(GLOB CMSIS_SOURCES
    libs/CMSIS/system_stm32f10x.c
    libs/CMSIS/core_cm3.c
)

# 标准外设库
file(GLOB_RECURSE STD_PERIPH_SOURCES
    libs/STM32F10x_StdPeriph_Driver/*.c
)

# 应用层源文件
file(GLOB APP_SOURCES
    src/app/*.c
)

# BSP驱动
file(GLOB BSP_SOURCES
    src/drivers/bsp/*.c
)

# 传感器驱动
file(GLOB SENSOR_SOURCES
    src/drivers/sensors/*.c
)

# 主程序和中断
set(MAIN_SOURCES
    src/main.c
    src/stm32f10x_it.c
)

# 启动文件 - 需要根据实际情况选择
set(STARTUP_ASM
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/CMSIS/startup_stm32f10x_md_gcc.s
)

# ===== 创建可执行文件 =====
add_executable(${PROJECT_NAME}.elf
    ${CMSIS_SOURCES}
    ${STD_PERIPH_SOURCES}
    ${APP_SOURCES}
    ${BSP_SOURCES}
    ${SENSOR_SOURCES}
    ${MAIN_SOURCES}
    ${STARTUP_ASM}
)

# 设置输出目录
set_target_properties(${PROJECT_NAME}.elf PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# ===== 生成额外的输出文件 =====

# 生成 HEX 文件
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMENT "Generating HEX file"
)

# 生成 BIN 文件
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMENT "Generating BIN file"
)

# 显示大小信息
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Size information:"
)

# ===== 打印构建信息 =====
message(STATUS "======================================")
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "MCU: ${MCU_MODEL}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "======================================")

