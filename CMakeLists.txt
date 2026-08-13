cmake_minimum_required(VERSION 3.20)

project(
    Direct3D9Renderer
    VERSION 1.0.0
    DESCRIPTION "A lightweight immediate-mode 2D renderer for Direct3D 9"
    LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Direct3D9Renderer requires a Windows toolchain")
endif()

add_library(d3d9_renderer STATIC Renderer.cpp Renderer.h)
add_library(Direct3D9Renderer::Renderer ALIAS d3d9_renderer)

target_compile_features(d3d9_renderer PUBLIC cxx_std_17)
target_include_directories(
    d3d9_renderer
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(DXSDK_LIBRARY_ARCH x64)
else()
    set(DXSDK_LIBRARY_ARCH x86)
endif()

find_path(
    D3DX9_INCLUDE_DIR
    NAMES d3dx9.h
    HINTS
        "$ENV{DXSDK_DIR}/Include"
        ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
find_library(
    D3DX9_LIBRARY
    NAMES d3dx9
    HINTS
        "$ENV{DXSDK_DIR}/Lib/${DXSDK_LIBRARY_ARCH}"
        ${CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES})

if(NOT D3DX9_INCLUDE_DIR OR NOT D3DX9_LIBRARY)
    message(
        FATAL_ERROR
        "D3DX9 was not found. Install the DirectX SDK (June 2010) and set DXSDK_DIR.")
endif()

target_include_directories(d3d9_renderer SYSTEM PUBLIC "${D3DX9_INCLUDE_DIR}")
target_link_libraries(d3d9_renderer PUBLIC d3d9 "${D3DX9_LIBRARY}")

if(MSVC)
    target_compile_options(d3d9_renderer PRIVATE /W4 /permissive-)
else()
    target_compile_options(d3d9_renderer PRIVATE -Wall -Wextra -Wpedantic)
endif()
