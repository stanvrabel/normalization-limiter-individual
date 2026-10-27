cmake_minimum_required(VERSION 3.20)

project(normalization-limiter-individual VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "This project must be built as 64-bit.")
endif()

if(DEFINED ENV{VST3_SDK_ROOT})
    set(VST3_SDK_ROOT "$ENV{VST3_SDK_ROOT}")
elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../vst3sdk/CMakeLists.txt")
    set(VST3_SDK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../vst3sdk")
else()
    message(FATAL_ERROR "VST3 SDK not found. Set VST3_SDK_ROOT environment variable or place vst3sdk next to this repo.")
endif()

message(STATUS "Using VST3 SDK: ${VST3_SDK_ROOT}")

add_subdirectory(${VST3_SDK_ROOT} ${CMAKE_BINARY_DIR}/vst3sdk)

smtg_enable_vst3_sdk()

smtg_add_vst3plugin(normalization-limiter-individual
    src/version.h
    src/pluginids.h
    src/factory.cpp
    src/processor.h
    src/processor.cpp
)

smtg_target_add_plugin_resources(normalization-limiter-individual
    RESOURCES
)

target_include_directories(normalization-limiter-individual
    PRIVATE
        ${VST3_SDK_ROOT}
)

target_compile_definitions(normalization-limiter-individual
    PRIVATE
        _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
    target_compile_options(normalization-limiter-individual PRIVATE /W4 /permissive-)
endif()

smtg_target_set_bundle(normalization-limiter-individual
    BUNDLE_NAME "normalization-limiter-individual"
)
