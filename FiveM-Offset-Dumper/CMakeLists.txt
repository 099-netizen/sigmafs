cmake_minimum_required(VERSION 3.20)
project(FiveMOffsetDumper CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(FATAL_ERROR "This project targets Windows x64 (DirectX 11 + Win32 API). Build with MSVC on Windows.")
endif()

# Dear ImGui sources (populated by setup.bat)
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/lib/imgui)
file(GLOB IMGUI_SOURCES
    ${IMGUI_DIR}/*.cpp
)
set(IMGUI_BACKENDS
    ${IMGUI_DIR}/imgui_impl_win32.cpp
    ${IMGUI_DIR}/imgui_impl_dx11.cpp
)

add_executable(${PROJECT_NAME} WIN32
    src/main.cpp
    ${IMGUI_SOURCES}
    ${IMGUI_BACKENDS}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${IMGUI_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    d3d11
    dxgi
    d3dcompiler
    psapi
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "999_Offset_Dumper"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
