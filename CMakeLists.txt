cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(firmware)

pico_sdk_init()

add_executable(${PROJECT_NAME})

target_sources(
    ${PROJECT_NAME} PUBLIC
    main.c
    lvds/lvds.c
    lvds/lut.c
    lvds/lut_low.S
)

target_link_libraries(
    ${PROJECT_NAME} PUBLIC
    pico_stdlib
    hardware_flash
    hardware_interp
    hardware_dma
    pico_multicore
    hardware_pwm
    hardware_pio
    hardware_interp
)

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/lvds/lvds.pio)

# create map/bin/hex file etc.
pico_add_extra_outputs(${PROJECT_NAME})

# pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)