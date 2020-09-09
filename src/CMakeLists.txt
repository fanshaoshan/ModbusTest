#  Copyright (c) Microsoft Corporation. All rights reserved.
#  Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)

project(modbus_test C)

# uncomments below if need build with Tx enable pin
add_definitions(-DTX_ENABLE)

azsphere_configure_tools(TOOLS_REVISION "20.07")

azsphere_configure_api(TARGET_API_SET "6")

aux_source_directory(. DIR_SRCS)

add_executable(${PROJECT_NAME} ${DIR_SRCS})

target_link_libraries(${PROJECT_NAME} applibs pthread gcc_s c)

target_compile_options(${PROJECT_NAME} PRIVATE -Wno-sign-conversion -Wno-conversion)

azsphere_target_hardware_definition(${PROJECT_NAME} TARGET_DIRECTORY "../Hardware/ailink_wfm620rsc1" TARGET_DEFINITION "sample_hardware.json")

azsphere_target_add_image_package(${PROJECT_NAME})