cmake_minimum_required(VERSION 3.20)

project(VidaUtilBateria VERSION 1.0.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /utf-8)
    add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

set(SOURCES
    src/main.cpp
    src/App.cpp
    src/Config.cpp
    src/BatteryMonitor.cpp
    src/AudioPlayer.cpp
    src/TrayIcon.cpp
    src/LaptopBrands.cpp
    src/ChargeControl.cpp
    src/Dialogs.cpp
    resources/app.rc
)

add_executable(VidaUtilBateria WIN32 ${SOURCES})

target_include_directories(VidaUtilBateria PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/resources
)

target_link_libraries(VidaUtilBateria PRIVATE
    shell32
    comctl32
    ole32
    oleaut32
    wbemuuid
    winmm
    shlwapi
    comdlg32
)

if(MSVC)
    set_target_properties(VidaUtilBateria PROPERTIES
        OUTPUT_NAME "VidaUtilBateria"
        WIN32_EXECUTABLE TRUE
    )
endif()
