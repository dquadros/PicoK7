# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.0.0)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.0.0)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(PicoK7 C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_subdirectory(../SDLib/src build)

add_executable(PicoK7 
	PicoK7.c
    tape.c
	display.c
	encoder.c 
    ws2812.c
	sdhw_config.c
)

pico_set_program_name(PicoK7 "PicoK7")
pico_set_program_version(PicoK7 "0.1")

# Modify the line bellow to enable/disable output over USB
pico_enable_stdio_usb(PicoK7 0)

# Add the standard library to the build
target_link_libraries(PicoK7
    pico_stdlib
	no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
    hardware_pio
    hardware_gpio
    hardware_spi
)

# Add the standard include files to the build
target_include_directories(PicoK7 PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
)

pico_generate_pio_header(PicoK7 ${CMAKE_CURRENT_LIST_DIR}/k7.pio)
pico_generate_pio_header(PicoK7 ${CMAKE_CURRENT_LIST_DIR}/encoder.pio)
pico_generate_pio_header(PicoK7 ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

pico_add_extra_outputs(PicoK7)

