add_library(paint_core STATIC
    color/DefaultPalette.h
    document/Document.cpp
    document/Document.h
    document/History.cpp
    document/History.h
    document/Layer.h
    editor/Editor.cpp
    editor/Editor.h
    io/ImageIO.cpp
    io/ImageIO.h
    ops/FloodFill.cpp
    ops/FloodFill.h
    shapes/ShapeGeometry.cpp
    shapes/ShapeGeometry.h
    tools/ClickTools.cpp
    tools/ClickTools.h
    tools/StrokeTools.cpp
    tools/StrokeTools.h
    tools/Tool.h
    tools/ToolSettings.cpp
    tools/ToolSettings.h
    tools/ToolTypes.h
)

target_include_directories(paint_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(paint_core
    PUBLIC Qt6::Core Qt6::Gui
    PRIVATE paint_warnings)
