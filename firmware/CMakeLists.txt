cmake_minimum_required(VERSION 3.22)

set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi-gcc.cmake)

project(single_channel_sensor
    VERSION 1.1.0
    LANGUAGES C ASM
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Board ────────────────────────────────────────────────────────────────────
# BOARD selects src/board/<BOARD>/ (board facts and pins) and the MCU port under
# src/port/<PORT>/. The fitted sensor is declared in board.h (BOARD_SENSOR_TYPE);
# its driver is looked up at runtime from the sensor registry.
#   ma01_l432 : STM32L432KC + MLX90393
#   ma02_f103 : STM32F103C8 + TMAG3001
#   ma03_h743 : STM32H743ZI + AK09974C
set(BOARD "ma03_h743" CACHE STRING "Target board (src/board/<BOARD>)")
set_property(CACHE BOARD PROPERTY STRINGS ma01_l432 ma02_f103 ma03_h743)

# BOARD_SENSOR_DRIVERS: drivers linked into this board's image (directory names
# under src/drivers/sensors). Each becomes SENSOR_DRV_<NAME> for the registry,
# so unused drivers cost no Flash (matters on the 64 KB F103).
if(BOARD STREQUAL "ma03_h743")
    set(PORT stm32h7)
    set(BOARD_SENSOR_DRIVERS ak09974c)
elseif(BOARD STREQUAL "ma01_l432")
    set(PORT stm32l4)
    set(BOARD_SENSOR_DRIVERS mlx90393)
elseif(BOARD STREQUAL "ma02_f103")
    set(PORT stm32f1)
    set(BOARD_SENSOR_DRIVERS tmag3001)
else()
    message(FATAL_ERROR "Unknown BOARD '${BOARD}'")
endif()

# ── MCU port ─────────────────────────────────────────────────────────────────
set(CMSIS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/CMSIS)

# FW_EMU_SP_FPU: 仅供主机端 Unicorn 回归测试使用（H7）。Unicorn 的 Cortex-M7
# 模型不支持双精度 VFP 指令，打开后改用单精度 FPU 构建（double 由软件库实现），
# C 代码路径与正式固件相同。正式固件不得打开此选项。
option(FW_EMU_SP_FPU "Build with single-precision FPU for emulator tests" OFF)

if(PORT STREQUAL "stm32h7")
    set(MCU_NAME        STM32H743xx)
    set(HAL_FAMILY      STM32H7xx)
    set(HAL_PREFIX      stm32h7xx)
    set(MCU_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32H743ZITx_FLASH.ld)
    set(STARTUP_FILE    startup_stm32h743xx.s)
    if(FW_EMU_SP_FPU)
        set(FW_FPU fpv5-sp-d16)
    else()
        set(FW_FPU fpv5-d16)
    endif()
    set(CPU_OPTIONS -mcpu=cortex-m7 -mthumb -mfpu=${FW_FPU} -mfloat-abi=hard)
    set(MCU_DEFINES STM32H743xx USE_PWR_LDO_SUPPLY)   # H743: core supplied by LDO
    set(HAL_MODULES hal hal_cortex hal_rcc hal_rcc_ex hal_gpio hal_dma hal_dma_ex
                    hal_mdma hal_exti hal_flash hal_flash_ex hal_pwr hal_pwr_ex
                    hal_uart hal_uart_ex hal_iwdg hal_tim hal_tim_ex)
elseif(PORT STREQUAL "stm32l4")
    set(MCU_NAME        STM32L432xx)
    set(HAL_FAMILY      STM32L4xx)
    set(HAL_PREFIX      stm32l4xx)
    set(MCU_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32L432KCUx_FLASH.ld)
    set(STARTUP_FILE    startup_stm32l432xx.s)
    set(CPU_OPTIONS -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)
    set(MCU_DEFINES STM32L432xx)
    set(HAL_MODULES hal hal_cortex hal_rcc hal_rcc_ex hal_gpio hal_dma hal_dma_ex
                    hal_flash hal_flash_ex hal_pwr hal_pwr_ex
                    hal_uart hal_uart_ex hal_iwdg)
elseif(PORT STREQUAL "stm32f1")
    set(MCU_NAME        STM32F103xB)
    set(HAL_FAMILY      STM32F1xx)
    set(HAL_PREFIX      stm32f1xx)
    set(MCU_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32F103C8Tx_FLASH.ld)
    set(STARTUP_FILE    startup_stm32f103xb.s)
    set(CPU_OPTIONS -mcpu=cortex-m3 -mthumb -mfloat-abi=soft)
    set(MCU_DEFINES STM32F103xB)
    set(HAL_MODULES hal hal_cortex hal_rcc hal_rcc_ex hal_gpio hal_gpio_ex hal_dma
                    hal_flash hal_flash_ex hal_pwr hal_uart hal_iwdg)
endif()

set(HAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/${HAL_FAMILY}_HAL_Driver)
message(STATUS "Board: ${BOARD}  Port: ${PORT}  MCU: ${MCU_NAME}")

# ── Compile definitions ───────────────────────────────────────────────────────
add_compile_definitions(
    USE_HAL_DRIVER
    ${MCU_DEFINES}
    FW_VERSION="${PROJECT_VERSION}"   # reported by ML VERSION
    USE_FREERTOS                      # P3: Enable FreeRTOS integration
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
    ${CMSIS_DIR}/Device/ST/${HAL_FAMILY}/Include
    ${HAL_DIR}/Inc
)

# ── Sources ───────────────────────────────────────────────────────────────────
set(CMSIS_SOURCES
    ${CMSIS_DIR}/Device/ST/${HAL_FAMILY}/Source/Templates/system_${HAL_PREFIX}.c
)

set(HAL_SOURCES)
foreach(mod ${HAL_MODULES})
    list(APPEND HAL_SOURCES ${HAL_DIR}/Src/${HAL_PREFIX}_${mod}.c)
endforeach()

# ── FreeRTOS ──────────────────────────────────────────────────────────────────
# P3: FreeRTOS kernel source must be downloaded separately (see libs/README.md)
set(FREERTOS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/FreeRTOS-Kernel)

if(NOT EXISTS ${FREERTOS_DIR}/tasks.c)
    message(WARNING "FreeRTOS-Kernel not found in libs/. Run: cd libs && curl -L https://github.com/FreeRTOS/FreeRTOS-Kernel/archive/refs/tags/V11.1.0.zip -o freertos.zip && unzip freertos.zip && mv FreeRTOS-Kernel-11.1.0 FreeRTOS-Kernel")
    message(WARNING "Building without FreeRTOS (bare-metal mode)")
    set(FREERTOS_SOURCES)
else()
    # Cortex-M port selection
    if(PORT STREQUAL "stm32h7")
        set(FREERTOS_PORT_DIR ${FREERTOS_DIR}/portable/GCC/ARM_CM7/r0p1)
    elseif(PORT STREQUAL "stm32l4")
        set(FREERTOS_PORT_DIR ${FREERTOS_DIR}/portable/GCC/ARM_CM4F)
    elseif(PORT STREQUAL "stm32f1")
        set(FREERTOS_PORT_DIR ${FREERTOS_DIR}/portable/GCC/ARM_CM3)
    endif()

    set(FREERTOS_SOURCES
        ${FREERTOS_DIR}/tasks.c
        ${FREERTOS_DIR}/queue.c
        ${FREERTOS_DIR}/list.c
        ${FREERTOS_DIR}/timers.c
        ${FREERTOS_DIR}/event_groups.c
        ${FREERTOS_DIR}/stream_buffer.c
        ${FREERTOS_DIR}/portable/MemMang/heap_4.c
        ${FREERTOS_PORT_DIR}/port.c
    )

    include_directories(
        ${FREERTOS_DIR}/include
        ${FREERTOS_PORT_DIR}
        src/port/${PORT}  # FreeRTOSConfig.h lives here
    )

    message(STATUS "FreeRTOS V11.1.0 enabled (port: ${FREERTOS_PORT_DIR})")
endif()

# ── Application sources ───────────────────────────────────────────────────────

file(GLOB APP_SOURCES     src/app/*.c)
file(GLOB BSP_SOURCES     src/drivers/bsp/*.c)
file(GLOB PORT_SOURCES    src/port/${PORT}/*.c src/port/stm32_common/*.c)
set(SENSOR_SOURCES src/drivers/sensors/sensor_registry.c)
set(SENSOR_DRIVER_DEFINES)
foreach(drv ${BOARD_SENSOR_DRIVERS} tca9548a)
    file(GLOB drv_src src/drivers/sensors/${drv}/*.c)
    list(APPEND SENSOR_SOURCES ${drv_src})
endforeach()
foreach(drv ${BOARD_SENSOR_DRIVERS})
    string(TOUPPER ${drv} DRV)
    list(APPEND SENSOR_DRIVER_DEFINES SENSOR_DRV_${DRV}=1)
endforeach()
add_compile_definitions(${SENSOR_DRIVER_DEFINES})
message(STATUS "Sensor drivers: ${BOARD_SENSOR_DRIVERS}")

set(MAIN_SOURCES
    src/main.c
)

set(STARTUP_ASM
    ${CMSIS_DIR}/Device/ST/${HAL_FAMILY}/Source/Templates/gcc/${STARTUP_FILE}
)

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME}.elf
    ${CMSIS_SOURCES}
    ${HAL_SOURCES}
    ${FREERTOS_SOURCES}
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
message(STATUS "HAL root : ${HAL_DIR}")
