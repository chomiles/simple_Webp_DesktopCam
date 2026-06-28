cmake_minimum_required(VERSION 3.20)
project(Webp_DesktopCam LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(LIBWEBP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libwebp-1.6.0")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_ANIM_UTILS OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_CWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_DWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_GIF2WEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_IMG2WEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_VWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPINFO OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPMUX OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBP_JS OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_FUZZTEST OFF CACHE BOOL "" FORCE)
add_subdirectory("${LIBWEBP_ROOT}" "${CMAKE_BINARY_DIR}/libwebp-build")

add_executable(Webp_DesktopCam WIN32
    src/main.cpp
    res/Webp_DesktopCam.rc
)

target_compile_definitions(Webp_DesktopCam PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_include_directories(Webp_DesktopCam PRIVATE
    "${LIBWEBP_ROOT}/src"
)

if(MSVC)
    target_compile_options(Webp_DesktopCam PRIVATE /utf-8)
endif()

target_link_libraries(Webp_DesktopCam PRIVATE
    webp
    libwebpmux
    user32
    gdi32
    ole32
    windowscodecs
    shlwapi
    shell32
    comctl32
)
