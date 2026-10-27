#pragma once

#include "tools/ToolTypes.h"

#include <QPainterPath>
#include <QRectF>

namespace paint {

// Outline of a shape stretched to fill `bounds`, as Paint draws it when the user drags out
// a rectangle. Line, Curve and Polygon are interactive shapes; for them this returns the
// representative outline used for their gallery icons.
QPainterPath shapePath(ShapeType type, const QRectF &bounds);

// Whether the shape is closed (can be filled).
constexpr bool shapeIsClosed(ShapeType type)
{
    return type != ShapeType::Line && type != ShapeType::Curve;
}

} // namespace paint
