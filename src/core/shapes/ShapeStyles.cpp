#include "shapes/ShapeStyles.h"

#include "brushes/Bristles.h"
#include "brushes/Textures.h"

#include <QImage>
#include <QPainter>
#include <QPainterPathStroker>

#include <cmath>

namespace paint {

namespace {

QImage coverageMask(const QPainterPath &region, const QRect &area)
{
    QImage mask(area.size(), QImage::Format_Alpha8);
    mask.fill(0);
    QPainter painter(&mask);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(-area.topLeft());
    painter.fillPath(region, Qt::black);
    return mask;
}

// Shapes use the same bristles wherever they are, so moving one does not change its streaks.
constexpr quint32 ShapeBristleSeed = 0x5eed;

} // namespace

void paintStyledArea(QPainter &painter, const QPainterPath &region, StrokeStyle style, const QColor &color,
                     QPointF grainOrigin)
{
    if (style == StrokeStyle::None || region.isEmpty()) {
        return;
    }
    if (style == StrokeStyle::Solid) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillPath(region, color);
        painter.restore();
        return;
    }
    QRect area = region.boundingRect().toAlignedRect().adjusted(-1, -1, 1, 1);
    if (const QPaintDevice *device = painter.device()) {
        // Room past the device's edge, so watercolour does not find an edge where the canvas ends.
        constexpr int Margin = WatercolorEdge + 1;
        area = area.intersected(
            QRect(0, 0, device->width(), device->height()).adjusted(-Margin, -Margin, Margin, Margin));
    }
    if (area.isEmpty()) {
        return;
    }
    const QPoint origin(int(std::floor(grainOrigin.x())), int(std::floor(grainOrigin.y())));
    painter.drawImage(area.topLeft(),
                      textured(coverageMask(region, area), style, color, area.topLeft() - origin));
}

void paintShape(QPainter &painter, const QPainterPath &path, bool closed, const ShapeStyle &style,
                QPointF grainOrigin)
{
    if (closed) {
        paintStyledArea(painter, path, style.fill, style.fillColor, grainOrigin);
    }
    if (style.outline == StrokeStyle::None) {
        return;
    }
    if (style.outline == StrokeStyle::Oil) {
        // Oil outlines are painted with the oil brush along the shape.
        Bristles bristles(style.outlineColor, style.width, ShapeBristleSeed);
        for (const QPolygonF &polygon : path.toSubpathPolygons()) {
            const qsizetype count = polygon.size();
            if (count > 2 && polygon.isClosed()) {
                bristles.lift(polygon.at(count - 1) - polygon.at(count - 2));
            } else {
                bristles.lift();
            }
            for (qsizetype i = 1; i < polygon.size(); ++i) {
                bristles.drawSegment(painter, polygon.at(i - 1), polygon.at(i));
            }
        }
        return;
    }
    // Filling the stroke's area rather than drawing with a pen: Qt draws 1 px pens with a thin-line
    // rasteriser that leaves corners unjoined.
    QPainterPathStroker stroker(QPen(style.outlineColor, style.width, Qt::SolidLine, style.cap, style.join));
    stroker.setMiterLimit(MiterLimit);
    paintStyledArea(painter, stroker.createStroke(path), style.outline, style.outlineColor, grainOrigin);
}

QRectF shapePaintBounds(const QPainterPath &path, const ShapeStyle &style)
{
    // Miter joins can reach past half the width at sharp corners.
    const qreal reach = style.outline == StrokeStyle::None ? 1 : style.width * MiterLimit / 2 + 2;
    return path.controlPointRect().adjusted(-reach, -reach, reach, reach);
}

} // namespace paint
