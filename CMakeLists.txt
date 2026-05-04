cmake_minimum_required(VERSION 3.24)

project(bluetooth_audio_sink LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(bluetooth_audio_sink WIN32
    src/main.cpp
)

target_compile_definitions(bluetooth_audio_sink PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(bluetooth_audio_sink PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(bluetooth_audio_sink PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(bluetooth_audio_sink PRIVATE
    comctl32
    shell32
    windowsapp
)
