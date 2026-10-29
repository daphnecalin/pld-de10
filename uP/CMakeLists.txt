cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(pld_de10 C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(main src/blink.c)
add_subdirectory(libraries/pico-ssd1306)
target_link_libraries(main pico_stdlib pico_ssd1306 hardware_i2c hardware_spi)

pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 1)
pico_add_extra_outputs(main)
