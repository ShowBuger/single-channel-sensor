cmake_minimum_required(VERSION 3.22)

set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi-gcc.cmake)

project(single_channel_sensor
    VERSION 1.0.0
    LANGUAGES C ASM
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Board / port ─────────────────────────────────────────────────────────────
# BOARD selects src/board/<BOARD>/ (board facts and pins); each board names the
# MCU port under src/port/<PORT>/. Only ma03_h743 is migrated so far; L4/F1
# boards are added in later refactor phases.
set(BOARD "ma03_h743" CACHE STRING "Target board (src/board/<BOARD>)")
if(BOARD STREQUAL "ma03_h743")
    set(PORT stm32h7)
else()
    message(FATAL_ERROR "Unknown BOARD '${BOARD}'")
endif()
message(STATUS "Board: ${BOARD}  Port: ${PORT}")

# ── MCU ──────────────────────────────────────────────────────────────────────
set(MCU_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32H743ZITx_FLASH.ld)

# FW_EMU_SP_FPU: 仅供主机端 Unicorn 回归测试使用。Unicorn 的 Cortex-M7 模型
# 不支持双精度 VFP 指令，打开后改用单精度 FPU 构建（double 由软件库实现），
# C 代码路径与正式固件相同。正式固件不得打开此选项。
option(FW_EMU_SP_FPU "Build with single-precision FPU for emulator tests" OFF)
if(FW_EMU_SP_FPU)
    set(FW_FPU fpv5-sp-d16)
else()
    set(FW_FPU fpv5-d16)
endif()

set(CPU_OPTIONS
    -mcpu=cortex-m7
    -mthumb
    -mfpu=${FW_FPU}
    -mfloat-abi=hard
)

# ── Compile definitions ───────────────────────────────────────────────────────
add_compile_definitions(
    USE_HAL_DRIVER
    STM32H743xx
    USE_PWR_LDO_SUPPLY    # H743 has no SMPS: core supplied by internal LDO
)

# ── Compile options ───────────────────────────────────────────────────────────
add_compile_options(
    ${CPU_OPTIONS}
    -fdata-sections
    -ffunction-sections
    -fno-common
    -fmessage-length=0
    -Wall
    -Wextra
    -Wno-unused-parameter
)
add_compile_options($<$<CONFIG:Debug>:-Og>)
add_compile_options($<$<CONFIG:Debug>:-g3>)
add_compile_options($<$<CONFIG:Debug>:-ggdb>)
add_compile_options($<$<CONFIG:Debug>:-DDEBUG>)
add_compile_options($<$<CONFIG:Release>:-Os>)
add_compile_options($<$<CONFIG:Release>:-DNDEBUG>)

# ── Link options ──────────────────────────────────────────────────────────────
add_link_options(
    ${CPU_OPTIONS}
    -T${MCU_LINKER_SCRIPT}
    -Wl,-Map=${PROJECT_NAME}.map
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    --specs=nano.specs
    --specs=nosys.specs
    -u _printf_float      # enable %f in nano.specs
)

# ── HAL driver root ───────────────────────────────────────────────────────────
set(HAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/STM32H7xx_HAL_Driver)
set(CMSIS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/CMSIS)

# ── Include directories ───────────────────────────────────────────────────────
include_directories(
    src
    src/platform             # MCU-neutral platform interface (plat.h)
    src/board/${BOARD}       # board.h, board_pins.h
    src/port/${PORT}         # HAL config and interrupt handlers
    src/app
    src/drivers/bsp
    src/drivers/sensors
    src/drivers/sensors/mlx90393
    src/drivers/sensors/tmag3001
    src/drivers/sensors/ak09974c
    src/drivers/sensors/tca9548a
    ${CMSIS_DIR}/Core/Include
    ${CMSIS_DIR}/Device/ST/STM32H7xx/Include
    ${HAL_DIR}/Inc
)

# ── Sources ───────────────────────────────────────────────────────────────────

# CMSIS device init
set(CMSIS_SOURCES
    ${CMSIS_DIR}/Device/ST/STM32H7xx/Source/Templates/system_stm32h7xx.c
)

# HAL driver sources (only modules we use)
file(GLOB HAL_SOURCES
    ${HAL_DIR}/Src/stm32h7xx_hal.c
    ${HAL_DIR}/Src/stm32h7xx_hal_cortex.c
    ${HAL_DIR}/Src/stm32h7xx_hal_rcc.c
    ${HAL_DIR}/Src/stm32h7xx_hal_rcc_ex.c
    ${HAL_DIR}/Src/stm32h7xx_hal_gpio.c
    ${HAL_DIR}/Src/stm32h7xx_hal_dma.c
    ${HAL_DIR}/Src/stm32h7xx_hal_dma_ex.c
    ${HAL_DIR}/Src/stm32h7xx_hal_mdma.c
    ${HAL_DIR}/Src/stm32h7xx_hal_exti.c
    ${HAL_DIR}/Src/stm32h7xx_hal_flash.c
    ${HAL_DIR}/Src/stm32h7xx_hal_flash_ex.c
    ${HAL_DIR}/Src/stm32h7xx_hal_pwr.c
    ${HAL_DIR}/Src/stm32h7xx_hal_pwr_ex.c
    ${HAL_DIR}/Src/stm32h7xx_hal_uart.c
    ${HAL_DIR}/Src/stm32h7xx_hal_uart_ex.c
    ${HAL_DIR}/Src/stm32h7xx_hal_iwdg.c
)

# Application sources
file(GLOB APP_SOURCES     src/app/*.c)

file(GLOB BSP_SOURCES      src/drivers/bsp/*.c)
file(GLOB PORT_SOURCES     src/port/${PORT}/*.c)

file(GLOB_RECURSE SENSOR_SOURCES  src/drivers/sensors/**/*.c)

set(MAIN_SOURCES
    src/main.c
)

set(STARTUP_ASM
    ${CMSIS_DIR}/Device/ST/STM32H7xx/Source/Templates/gcc/startup_stm32h743xx.s
)

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME}.elf
    ${CMSIS_SOURCES}
    ${HAL_SOURCES}
    ${APP_SOURCES}
    ${BSP_SOURCES}
    ${PORT_SOURCES}
    ${SENSOR_SOURCES}
    ${MAIN_SOURCES}
    ${STARTUP_ASM}
)

set_target_properties(${PROJECT_NAME}.elf PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# ── Post-build: HEX + BIN ────────────────────────────────────────────────────
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMENT "Generating HEX"
)
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMENT "Generating BIN"
)
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Size:"
)

message(STATUS "Project  : ${PROJECT_NAME}")
message(STATUS "MCU      : STM32H743xx")
message(STATUS "HAL root : ${HAL_DIR}")
