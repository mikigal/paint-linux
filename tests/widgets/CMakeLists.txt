paint_add_test(tst_mainwindow SOURCES tst_mainwindow.cpp LIBRARIES paint_app)
paint_add_test(tst_mainwindow_dark SOURCES tst_mainwindow.cpp LIBRARIES paint_app THEME dark)
target_compile_definitions(tst_mainwindow_dark PRIVATE PAINT_TEST_DARK_THEME)

if(PAINT_AUTOMATION)
    paint_add_test(tst_automation SOURCES tst_automation.cpp LIBRARIES paint_app)
endif()

paint_add_test(tst_icons SOURCES tst_icons.cpp LIBRARIES paint_app Qt6::Svg)
