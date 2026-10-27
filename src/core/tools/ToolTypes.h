#pragma once

#include <QtGlobal>

#include <array>

namespace paint {

enum class Tool {
    RectangleSelect,
    FreeformSelect,
    Pencil,
    Fill,
    Text,
    Eraser,
    ColorPicker,
    Magnifier,
    Brush,
    Shape,
};

enum class BrushType {
    Brush,
    CalligraphyBrush,
    CalligraphyPen,
    Airbrush,
    OilBrush,
    Crayon,
    Marker,
    NaturalPencil,
    Watercolor,
};

// In the order of Paint's shapes gallery.
enum class ShapeType {
    Line,
    Curve,
    Oval,
    Rectangle,
    RoundedRectangle,
    Polygon,
    Triangle,
    RightTriangle,
    Diamond,
    Pentagon,
    Hexagon,
    RightArrow,
    LeftArrow,
    UpArrow,
    DownArrow,
    FourPointStar,
    FivePointStar,
    SixPointStar,
    RoundedRectangularCallout,
    OvalCallout,
    CloudCallout,
    Heart,
    Lightning,
};

// Shape outline and fill styles.
enum class StrokeStyle {
    None,
    Solid,
    Crayon,
    Marker,
    Oil,
    NaturalPencil,
    Watercolor,
};

enum class ColorSlot {
    Primary,
    Secondary,
};

inline constexpr std::array AllBrushTypes{
    BrushType::Brush,    BrushType::CalligraphyBrush, BrushType::CalligraphyPen,
    BrushType::Airbrush, BrushType::OilBrush,         BrushType::Crayon,
    BrushType::Marker,   BrushType::NaturalPencil,    BrushType::Watercolor,
};

inline constexpr std::array AllShapeTypes{
    ShapeType::Line,
    ShapeType::Curve,
    ShapeType::Oval,
    ShapeType::Rectangle,
    ShapeType::RoundedRectangle,
    ShapeType::Polygon,
    ShapeType::Triangle,
    ShapeType::RightTriangle,
    ShapeType::Diamond,
    ShapeType::Pentagon,
    ShapeType::Hexagon,
    ShapeType::RightArrow,
    ShapeType::LeftArrow,
    ShapeType::UpArrow,
    ShapeType::DownArrow,
    ShapeType::FourPointStar,
    ShapeType::FivePointStar,
    ShapeType::SixPointStar,
    ShapeType::RoundedRectangularCallout,
    ShapeType::OvalCallout,
    ShapeType::CloudCallout,
    ShapeType::Heart,
    ShapeType::Lightning,
};

inline constexpr std::array AllStrokeStyles{
    StrokeStyle::None, StrokeStyle::Solid,         StrokeStyle::Crayon,     StrokeStyle::Marker,
    StrokeStyle::Oil,  StrokeStyle::NaturalPencil, StrokeStyle::Watercolor,
};

// Tools whose stroke width comes from the size slider.
constexpr bool toolUsesSize(Tool tool)
{
    return tool == Tool::Pencil || tool == Tool::Brush || tool == Tool::Eraser || tool == Tool::Shape;
}

// Tools with an opacity slider (the eraser's one is called transparency in Paint).
constexpr bool toolUsesOpacity(Tool tool)
{
    return tool == Tool::Pencil || tool == Tool::Brush || tool == Tool::Eraser;
}

constexpr bool isSelectionTool(Tool tool)
{
    return tool == Tool::RectangleSelect || tool == Tool::FreeformSelect;
}

} // namespace paint
