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
set(sdkVersion 2.2.0)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

#creating path to freeRtos lib to import from 
set(FREE_RTOS_KERNEL_INCLUDE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)
include(${FREE_RTOS_KERNEL_INCLUDE_PATH}/FreeRTOS_Kernel_import.cmake)

project(FreeRtosProject C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Build all EDF test sources under edf_tests/.
# (CONFIGURE_DEPENDS makes CMake re-run when files are added/removed.)
file(GLOB EDF_TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/edf_tests/*.c
)

file(GLOB SRP_TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/srp_tests/*.c
)

file(GLOB CBS_TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/cbs_tests/*.c
)

# Add executable. Default name is the project name, version 0.1
add_executable(${PROJECT_NAME}
    main.c
    ${EDF_TEST_SOURCES}
    ${SRP_TEST_SOURCES}
    ${CBS_TEST_SOURCES}
    task_trace.c
    test_utils.c
    cbs.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    FreeRTOS-Kernel-Heap4
)

pico_set_program_name(FreeRtosProject "FreeRtosProject")
pico_set_program_version(FreeRtosProject "0.1")

pico_add_extra_outputs(${PROJECT_NAME})

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(FreeRtosProject 1)
pico_enable_stdio_usb(FreeRtosProject 0)
