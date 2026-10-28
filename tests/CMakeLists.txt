# Tests run headless (offscreen platform) against a private, deterministic desktop environment:
# Breeze style, a fixed KDE colour scheme and fonts, and scratch XDG directories inside the build
# tree, so neither the developer's own settings nor the tests' writes leak across.

set(PAINT_TEST_ENV_DIR ${CMAKE_BINARY_DIR}/test-env)
foreach(theme light dark)
    file(MAKE_DIRECTORY ${PAINT_TEST_ENV_DIR}/${theme}/data ${PAINT_TEST_ENV_DIR}/${theme}/cache)
    configure_file(env/breeze-${theme}/kdeglobals ${PAINT_TEST_ENV_DIR}/${theme}/config/kdeglobals COPYONLY)
endforeach()
configure_file(env/offscreen.json ${PAINT_TEST_ENV_DIR}/offscreen.json COPYONLY)

add_library(paint_test_support STATIC
    common/GoldenImage.cpp
    common/GoldenImage.h
)
target_include_directories(paint_test_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common)
target_link_libraries(paint_test_support
    PUBLIC Qt6::Gui Qt6::Test
    PRIVATE paint_warnings)
target_compile_definitions(paint_test_support PUBLIC
    PAINT_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
    PAINT_TEST_OUTPUT_DIR="${CMAKE_BINARY_DIR}/test-output")

# paint_add_test(<name> SOURCES <files...> [LIBRARIES <libs...>] [THEME light|dark])
function(paint_add_test name)
    cmake_parse_arguments(ARG "" "THEME" "SOURCES;LIBRARIES" ${ARGN})
    if(NOT ARG_THEME)
        set(ARG_THEME light)
    endif()
    set(env_dir ${PAINT_TEST_ENV_DIR}/${ARG_THEME})

    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE paint_test_support paint_warnings ${ARG_LIBRARIES})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        TIMEOUT 120
        ENVIRONMENT_MODIFICATION
            "QT_QPA_PLATFORM=set:offscreen:configfile=${PAINT_TEST_ENV_DIR}/offscreen.json;\
QT_QPA_PLATFORMTHEME=set:kde;\
XDG_CONFIG_HOME=set:${env_dir}/config;\
XDG_DATA_HOME=set:${env_dir}/data;\
XDG_CACHE_HOME=set:${env_dir}/cache;\
QT_SCALE_FACTOR=unset:;\
QT_SCREEN_SCALE_FACTORS=unset:;\
QT_FONT_DPI=unset:;\
LANG=set:C.UTF-8;\
LC_ALL=set:C.UTF-8")
endfunction()

add_subdirectory(core)
add_subdirectory(widgets)
add_subdirectory(bench)
