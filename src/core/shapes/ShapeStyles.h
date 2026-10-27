#pragma once

#include "tools/ToolTypes.h"

#include <QColor>
#include <QPainterPath>
#include <QPointF>

class QPainter;

namespace paint {

// How a shape is painted: Paint draws the outline in colour 1 and the fill in colour 2.
struct ShapeStyle
{
    StrokeStyle outline = StrokeStyle::Solid;
    StrokeStyle fill = StrokeStyle::None;
    QColor outlineColor = Qt::black;
    QColor fillColor = Qt::white;
    qreal width = 1;
    Qt::PenCapStyle cap = Qt::RoundCap;
    // Sharp corners up to MiterLimit; sharper ones (a heart's dip) are cut off.
    Qt::PenJoinStyle join = Qt::SvgMiterJoin;
};

// How far a sharp corner of an outline may reach, in outline widths.
inline constexpr qreal MiterLimit = 4;

// Paints `region` (a closed area) in one of Paint's outline and fill styles. Textured styles take
// their grain from `grainOrigin`, so the texture travels with a shape that is moved. The painter
// must not be transformed.
void paintStyledArea(QPainter &painter, const QPainterPath &region, StrokeStyle style, const QColor &color,
                     QPointF grainOrigin);

// Paints a shape: the fill (closed shapes only) under the outline.
void paintShape(QPainter &painter, const QPainterPath &path, bool closed, const ShapeStyle &style,
                QPointF grainOrigin);

// The area `paintShape` may touch.
QRectF shapePaintBounds(const QPainterPath &path, const ShapeStyle &style);

} // namespace paint
