cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# получаем версию из git
execute_process(
        COMMAND git describe --abbrev=7 --dirty --always --tags
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# получаем remote url и приводим его к нужному виду
execute_process(
        COMMAND git config --get remote.origin.url
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_REMOTE
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# применяем sed-замену: можно через string(REGEX REPLACE)
string(REGEX REPLACE "^.*github" "github" GIT_REMOTE "${GIT_REMOTE}")

message(STATUS "GIT_VERSION = ${GIT_VERSION}")
message(STATUS "GIT_REMOTE  = ${GIT_REMOTE}")

# Define the build type
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif ()

# Set the project name
set(CMAKE_PROJECT_NAME usb-to-can-converter-based-on-lawicel-protocol)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME}
        Core/Src/main.c
)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user sources here
        Core/Src/prj_can.c
        Core/Src/system.c
        Core/Src/slcan.c
        Core/Src/error.c
        Core/Src/utils_impl.c
        Core/Src/discrete_output_impl.c
        lib/utils/src/utils.c
        lib/utils/src/RTT/SEGGER_RTT.c
        lib/utils/src/RTT/SEGGER_RTT_printf.c
        lib/utils/src/RTT/SEGGER_RTT_ASM_ARMv7M.S
        lib/discrete_io/discrete_output.c
        lib/circular_buffer/circular-buffer.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user defined include paths
        Core/Inc
        lib/utils/src
        lib/utils/src/RTT
        lib/discrete_io
        lib/circular_buffer
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
        stm32cubemx

        # Add user defined libraries
)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex
        $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
        ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary
        $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
        ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin
        COMMENT "Generating HEX and BIN files"
        VERBATIM
)