add_library(paint_core STATIC
    brushes/Bristles.cpp
    brushes/Bristles.h
    brushes/Textures.cpp
    brushes/Textures.h
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
    io/MimeImport.cpp
    io/MimeImport.h
    io/OpenRaster.cpp
    io/OpenRaster.h
    ops/FloodFill.cpp
    ops/FloodFill.h
    ops/ImageOps.cpp
    ops/ImageOps.h
    shapes/ShapeGeometry.cpp
    shapes/ShapeGeometry.h
    shapes/ShapeStyles.cpp
    shapes/ShapeStyles.h
    tools/BoxGeometry.cpp
    tools/BoxGeometry.h
    tools/ClickTools.cpp
    tools/ClickTools.h
    tools/SelectionTool.cpp
    tools/SelectionTool.h
    tools/ShapeTool.cpp
    tools/ShapeTool.h
    tools/StrokeTools.cpp
    tools/StrokeTools.h
    tools/TextTool.cpp
    tools/TextTool.h
    tools/Tool.h
    tools/ToolSettings.cpp
    tools/ToolSettings.h
    tools/ToolTypes.h
)

target_include_directories(paint_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(paint_core
    PUBLIC Qt6::Core Qt6::Gui
    PRIVATE paint_warnings PkgConfig::LIBZIP)
