cmake_minimum_required(VERSION 3.22)
project(sensor_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# stubs BEFORE firmware/src so stub headers shadow the real ones
include_directories(
    stubs
    ../firmware/src
    ../firmware/src/app
    ../firmware/src/drivers/sensors
    ../firmware/src/drivers/bsp
)

add_compile_definitions(HOST_TEST)

set(STUB_SRCS
    stubs/hal_stub.c
    stubs/bsp_stub.c
)

# test_config: config_manager save/load tests
add_executable(test_config
    ${STUB_SRCS}
    ../firmware/src/app/config_manager.c
    test_config_manager.c
)

# test_algo: algo_params tests
add_executable(test_algo
    ${STUB_SRCS}
    ../firmware/src/app/config_manager.c
    ../firmware/src/app/algo_params.c
    test_algo_params.c
)

# test_hal: sensor HAL vtable and mlx90393_hal adapter
add_executable(test_hal
    stubs/hal_stub.c
    ../firmware/src/drivers/sensors/mlx90393_hal.c
    test_hal.c
)

enable_testing()
add_test(NAME test_config COMMAND test_config)
add_test(NAME test_algo   COMMAND test_algo)
add_test(NAME test_hal    COMMAND test_hal)
