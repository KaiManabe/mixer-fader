# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(mixer-fader C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(
    mixer-fader
    src/main.cpp
    src/init_gpio.cpp
    src/Encoder.cpp
    src/LevelMeter.cpp
    src/Display.cpp
    src/UsbComm.cpp
    src/usb_adapter.cpp
    src/usb_descriptors.c
)

pico_set_program_name(mixer-fader "mixer-fader")
pico_set_program_version(mixer-fader "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)

pico_generate_pio_header(mixer-fader
    ${CMAKE_CURRENT_LIST_DIR}/pio/EncoderPiso.pio
    ${CMAKE_CURRENT_LIST_DIR}/pio/LedSipo.pio
)

target_link_libraries(${PROJECT_NAME} 
    pico_stdlib 
    hardware_spi  # SPI制御に必要
    hardware_dma  # DMA制御に必要
    hardware_uart
    hardware_pio
    tinyusb_device
    tinyusb_board
)


# Add the standard include files to the build
target_include_directories(mixer-fader PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}/include/common
)

pico_add_extra_outputs(mixer-fader)
