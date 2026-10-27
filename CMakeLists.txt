cmake_minimum_required(VERSION 3.25)

project(PaintForLinux
    VERSION 0.1.0
    DESCRIPTION "A native Linux take on Windows 11 Paint"
    HOMEPAGE_URL "https://mikigal.pl"
    LANGUAGES CXX)

set(PAINT_APP_ID "pl.mikigal.PaintForLinux")
set(PAINT_APP_NAME "Paint for Linux")
set(PAINT_EXECUTABLE "paint-for-linux")

option(PAINT_BUILD_TESTS "Build unit and widget tests" ON)
option(PAINT_AUTOMATION "Build the automation server used by UI scenarios (never enable for releases)" OFF)
option(PAINT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Svg)
if(PAINT_AUTOMATION)
    # QPA input injection (qwindowsysteminterface.h) for the automation server.
    set(QT_NO_PRIVATE_MODULE_WARNING ON)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Network Test GuiPrivate)
endif()
if(PAINT_BUILD_TESTS)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
endif()

qt_standard_project_setup()
include(GNUInstallDirs)

add_library(paint_warnings INTERFACE)
target_compile_options(paint_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
    -Woverloaded-virtual -Wnull-dereference -Wimplicit-fallthrough
    $<$<BOOL:${PAINT_WARNINGS_AS_ERRORS}>:-Werror>)
target_compile_definitions(paint_warnings INTERFACE
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
    QT_NO_URL_CAST_FROM_STRING
    QT_NO_CAST_FROM_BYTEARRAY
    QT_USE_QSTRINGBUILDER
    QT_NO_KEYWORDS)

add_subdirectory(src/core)
add_subdirectory(src/app)

if(PAINT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
