cmake_minimum_required(VERSION 3.24)
project(MobileTexDBEditor VERSION 2.0.0 LANGUAGES CXX)

if(NOT MSVC)
    message(FATAL_ERROR "MobileTexDBEditor is a Windows/MSVC project. Configure it with Visual Studio 2022 or MSVC Build Tools.")
endif()

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui)

add_executable(MobileTexDBEditor WIN32
    src/main.cpp
    src/TextureDb.cpp
    src/ImageIO.cpp
    include/TextureDb.h
    include/ImageIO.h
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_impl_win32.cpp
    ${IMGUI_DIR}/imgui_impl_dx11.cpp
)

target_include_directories(MobileTexDBEditor PRIVATE include ${IMGUI_DIR})
target_compile_features(MobileTexDBEditor PRIVATE cxx_std_20)
target_compile_definitions(MobileTexDBEditor PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(MobileTexDBEditor PRIVATE /W4 /permissive- /EHsc /utf-8 /Zc:__cplusplus)

target_link_libraries(MobileTexDBEditor PRIVATE d3d11 dxgi comdlg32 windowscodecs ole32 shell32)
set_target_properties(MobileTexDBEditor PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:MobileTexDBEditor>")
install(TARGETS MobileTexDBEditor RUNTIME DESTINATION .)
