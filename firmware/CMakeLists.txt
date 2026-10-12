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

# ── MCU ──────────────────────────────────────────────────────────────────────
set(MCU_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32F103C8Tx_FLASH.ld)

set(CPU_OPTIONS
    -mcpu=cortex-m3
    -mthumb
    -mfloat-abi=soft
)

# ── Compile definitions ───────────────────────────────────────────────────────
add_compile_definitions(
    USE_HAL_DRIVER
    STM32F103xB
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
)

# ── HAL driver root ───────────────────────────────────────────────────────────
set(HAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/STM32F1xx_HAL_Driver)
set(CMSIS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/CMSIS)

# ── Include directories ───────────────────────────────────────────────────────
include_directories(
    src
    src/app
    src/drivers/bsp
    src/drivers/sensors
    ${CMSIS_DIR}/Core/Include
    ${CMSIS_DIR}/Device/ST/STM32F1xx/Include
    ${HAL_DIR}/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/src   # ensure stm32f1xx_hal_conf.h is found by HAL sources
)

# ── Sources ───────────────────────────────────────────────────────────────────

# CMSIS device init
set(CMSIS_SOURCES
    ${CMSIS_DIR}/Device/ST/STM32F1xx/Source/Templates/system_stm32f1xx.c
)

# HAL driver sources (only modules we use)
file(GLOB HAL_SOURCES
    ${HAL_DIR}/Src/stm32f1xx_hal.c
    ${HAL_DIR}/Src/stm32f1xx_hal_cortex.c
    ${HAL_DIR}/Src/stm32f1xx_hal_rcc.c
    ${HAL_DIR}/Src/stm32f1xx_hal_rcc_ex.c
    ${HAL_DIR}/Src/stm32f1xx_hal_gpio.c
    ${HAL_DIR}/Src/stm32f1xx_hal_gpio_ex.c
    ${HAL_DIR}/Src/stm32f1xx_hal_dma.c
    ${HAL_DIR}/Src/stm32f1xx_hal_flash.c
    ${HAL_DIR}/Src/stm32f1xx_hal_flash_ex.c
    ${HAL_DIR}/Src/stm32f1xx_hal_pwr.c
    ${HAL_DIR}/Src/stm32f1xx_hal_uart.c
    ${HAL_DIR}/Src/stm32f1xx_hal_iwdg.c
)

# Application sources
file(GLOB APP_SOURCES     src/app/*.c)

# BSP sources -- exclude bsp_i2c.c (hardware I2C, unused) and bsp_dma.c (not ported yet)
file(GLOB BSP_SOURCES_RAW  src/drivers/bsp/*.c)
list(FILTER BSP_SOURCES_RAW EXCLUDE REGEX ".*bsp_i2c\\.c$")
list(FILTER BSP_SOURCES_RAW EXCLUDE REGEX ".*bsp_dma\\.c$")
set(BSP_SOURCES ${BSP_SOURCES_RAW})

file(GLOB SENSOR_SOURCES  src/drivers/sensors/*.c)

set(MAIN_SOURCES
    src/main.c
    src/stm32f10x_it.c
)

set(STARTUP_ASM
    ${CMSIS_DIR}/Device/ST/STM32F1xx/Source/Templates/gcc/startup_stm32f103xb.s
)

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME}.elf
    ${CMSIS_SOURCES}
    ${HAL_SOURCES}
    ${APP_SOURCES}
    ${BSP_SOURCES}
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
message(STATUS "MCU      : STM32F103xB")
message(STATUS "HAL root : ${HAL_DIR}")
