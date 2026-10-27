add_library(paint_core STATIC
    color/DefaultPalette.h
    document/Document.cpp
    document/Document.h
    document/Layer.h
    shapes/ShapeGeometry.cpp
    shapes/ShapeGeometry.h
    tools/ToolSettings.cpp
    tools/ToolSettings.h
    tools/ToolTypes.h
)

target_include_directories(paint_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(paint_core
    PUBLIC Qt6::Core Qt6::Gui
    PRIVATE paint_warnings)
