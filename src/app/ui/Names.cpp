#include "ui/Names.h"

#include <QCoreApplication>

#include <array>

namespace paint::ui {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("paint::ui", text);
}

} // namespace

QString toolName(Tool tool)
{
    switch (tool) {
    case Tool::RectangleSelect:
        return tr("Rectangle selection");
    case Tool::FreeformSelect:
        return tr("Free-form selection");
    case Tool::Pencil:
        return tr("Pencil");
    case Tool::Fill:
        return tr("Fill");
    case Tool::Text:
        return tr("Text");
    case Tool::Eraser:
        return tr("Eraser");
    case Tool::ColorPicker:
        return tr("Color picker");
    case Tool::Magnifier:
        return tr("Magnifier");
    case Tool::Brush:
        return tr("Brushes");
    case Tool::Shape:
        return tr("Shapes");
    }
    return {};
}

QString brushName(BrushType type)
{
    switch (type) {
    case BrushType::Brush:
        return tr("Brush");
    case BrushType::CalligraphyBrush:
        return tr("Calligraphy brush");
    case BrushType::CalligraphyPen:
        return tr("Calligraphy pen");
    case BrushType::Airbrush:
        return tr("Airbrush");
    case BrushType::OilBrush:
        return tr("Oil brush");
    case BrushType::Crayon:
        return tr("Crayon");
    case BrushType::Marker:
        return tr("Marker");
    case BrushType::NaturalPencil:
        return tr("Natural pencil");
    case BrushType::Watercolor:
        return tr("Watercolor brush");
    }
    return {};
}

QString shapeName(ShapeType type)
{
    switch (type) {
    case ShapeType::Line:
        return tr("Line");
    case ShapeType::Curve:
        return tr("Curve");
    case ShapeType::Oval:
        return tr("Oval");
    case ShapeType::Rectangle:
        return tr("Rectangle");
    case ShapeType::RoundedRectangle:
        return tr("Rounded rectangle");
    case ShapeType::Polygon:
        return tr("Polygon");
    case ShapeType::Triangle:
        return tr("Triangle");
    case ShapeType::RightTriangle:
        return tr("Right triangle");
    case ShapeType::Diamond:
        return tr("Diamond");
    case ShapeType::Pentagon:
        return tr("Pentagon");
    case ShapeType::Hexagon:
        return tr("Hexagon");
    case ShapeType::RightArrow:
        return tr("Right arrow");
    case ShapeType::LeftArrow:
        return tr("Left arrow");
    case ShapeType::UpArrow:
        return tr("Up arrow");
    case ShapeType::DownArrow:
        return tr("Down arrow");
    case ShapeType::FourPointStar:
        return tr("Four-point star");
    case ShapeType::FivePointStar:
        return tr("Five-point star");
    case ShapeType::SixPointStar:
        return tr("Six-point star");
    case ShapeType::RoundedRectangularCallout:
        return tr("Rounded rectangular callout");
    case ShapeType::OvalCallout:
        return tr("Oval callout");
    case ShapeType::CloudCallout:
        return tr("Cloud callout");
    case ShapeType::Heart:
        return tr("Heart");
    case ShapeType::Lightning:
        return tr("Lightning");
    }
    return {};
}

namespace {

QString strokeStyleName(StrokeStyle style)
{
    switch (style) {
    case StrokeStyle::None:
    case StrokeStyle::Solid:
        break;
    case StrokeStyle::Crayon:
        return tr("Crayon");
    case StrokeStyle::Marker:
        return tr("Marker");
    case StrokeStyle::Oil:
        return tr("Oil");
    case StrokeStyle::NaturalPencil:
        return tr("Natural pencil");
    case StrokeStyle::Watercolor:
        return tr("Watercolor");
    }
    return tr("Solid color");
}

} // namespace

QString outlineStyleName(StrokeStyle style)
{
    return style == StrokeStyle::None ? tr("No outline") : strokeStyleName(style);
}

QString fillStyleName(StrokeStyle style)
{
    return style == StrokeStyle::None ? tr("No fill") : strokeStyleName(style);
}

QString paletteColorName(int index)
{
    static const std::array<const char *, 20> names{
        QT_TRANSLATE_NOOP("paint::ui", "Black"),     QT_TRANSLATE_NOOP("paint::ui", "Gray-50%"),
        QT_TRANSLATE_NOOP("paint::ui", "Dark red"),  QT_TRANSLATE_NOOP("paint::ui", "Red"),
        QT_TRANSLATE_NOOP("paint::ui", "Orange"),    QT_TRANSLATE_NOOP("paint::ui", "Yellow"),
        QT_TRANSLATE_NOOP("paint::ui", "Green"),     QT_TRANSLATE_NOOP("paint::ui", "Turquoise"),
        QT_TRANSLATE_NOOP("paint::ui", "Indigo"),    QT_TRANSLATE_NOOP("paint::ui", "Purple"),
        QT_TRANSLATE_NOOP("paint::ui", "White"),     QT_TRANSLATE_NOOP("paint::ui", "Gray-25%"),
        QT_TRANSLATE_NOOP("paint::ui", "Brown"),     QT_TRANSLATE_NOOP("paint::ui", "Rose"),
        QT_TRANSLATE_NOOP("paint::ui", "Gold"),      QT_TRANSLATE_NOOP("paint::ui", "Light yellow"),
        QT_TRANSLATE_NOOP("paint::ui", "Lime"),      QT_TRANSLATE_NOOP("paint::ui", "Light turquoise"),
        QT_TRANSLATE_NOOP("paint::ui", "Blue-gray"), QT_TRANSLATE_NOOP("paint::ui", "Lavender"),
    };
    return index >= 0 && index < int(names.size()) ? tr(names.at(index)) : QString();
}

} // namespace paint::ui
