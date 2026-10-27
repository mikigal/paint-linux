#include "tools/StrokeTools.h"

#include "brushes/Textures.h"
#include "document/Document.h"
#include "document/History.h"
#include "tools/ToolSettings.h"

#include <QCoreApplication>
#include <QPainter>

#include <cmath>
#include <numbers>

namespace paint {

namespace {

QRect segmentBounds(QPointF from, QPointF to, qreal radius)
{
    const QRectF box = QRectF(from, to).normalized().adjusted(-radius, -radius, radius, radius);
    return box.toAlignedRect().adjusted(-1, -1, 1, 1);
}

// The centre of the pixel under a point, for hard-edged drawing.
QPointF pixelCentre(QPointF point)
{
    return QPointF(std::floor(point.x()) + 0.5, std::floor(point.y()) + 0.5);
}

} // namespace

void StrokeTool::press(const PointerEvent &event)
{
    if (isActive() || (event.button != Qt::LeftButton && event.button != Qt::RightButton)) {
        return;
    }
    Document &document = m_context.document();
    ToolSettings &settings = m_context.settings();
    m_button = event.button;
    m_layer = document.activeLayerIndex();
    m_color = settings.color(event.button == Qt::RightButton ? ColorSlot::Secondary : ColorSlot::Primary);
    m_size = settings.size();
    m_opacity = settings.opacity() / 100.0;
    m_before = document.layer(m_layer).image; // shared until the first change detaches the layer
    m_stroke = QImage(document.size(), Document::LayerFormat);
    m_stroke.fill(Qt::transparent);
    m_changed = {};
    m_last = event.position;
    beginStroke(event.position);
    addSegment(event.position, event.position);
}

void StrokeTool::move(const PointerEvent &event)
{
    if (!isActive()) {
        return;
    }
    addSegment(m_last, event.position);
    m_last = event.position;
}

void StrokeTool::release(const PointerEvent &event)
{
    if (!isActive() || event.button != m_button) {
        return;
    }
    addSegment(m_last, event.position);
    finish();
}

void StrokeTool::cancel()
{
    if (!isActive()) {
        return;
    }
    const QRect changed = m_changed;
    if (!changed.isEmpty()) {
        QPainter painter(&m_context.document().layer(m_layer).image);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(changed.topLeft(), m_before, changed);
    }
    m_button = Qt::NoButton;
    m_before = {};
    m_stroke = {};
    m_changed = {};
    if (!changed.isEmpty()) {
        m_context.documentChanged(changed);
    }
}

void StrokeTool::commit()
{
    if (isActive()) {
        finish();
    }
}

void StrokeTool::addSegment(QPointF from, QPointF to)
{
    QPainter painter(&m_stroke);
    const QRect area = drawSegment(painter, from, to).intersected(m_stroke.rect());
    painter.end();
    if (!area.isEmpty()) {
        composite(area);
    }
}

void StrokeTool::composite(const QRect &area)
{
    QPainter painter(&m_context.document().layer(m_layer).image);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(area.topLeft(), m_before, area);
    painter.setCompositionMode(erases() ? QPainter::CompositionMode_DestinationOut
                                        : QPainter::CompositionMode_SourceOver);
    painter.setOpacity(m_opacity);
    drawStroke(painter, area);
    painter.end();
    m_changed |= area;
    m_context.documentChanged(area);
}

void StrokeTool::drawStroke(QPainter &painter, const QRect &area) const
{
    painter.drawImage(area.topLeft(), m_stroke, area);
}

void StrokeTool::finish()
{
    if (!m_changed.isEmpty()) {
        m_context.history().recordPixelEdit(actionName(), m_layer, m_changed, m_before.copy(m_changed));
    }
    m_button = Qt::NoButton;
    m_before = {};
    m_stroke = {};
    m_changed = {};
}

QRect PencilTool::drawSegment(QPainter &painter, QPointF from, QPointF to)
{
    from = pixelCentre(from);
    to = pixelCentre(to);
    painter.setRenderHint(QPainter::Antialiasing, false);
    if (m_size <= 1) {
        // Exactly one pixel per step, like Paint's 1 px pencil.
        painter.setPen(QPen(m_color, 1, Qt::SolidLine, Qt::SquareCap));
        painter.drawLine(from, to);
        painter.drawPoint(to);
        return segmentBounds(from, to, 1);
    }
    painter.setPen(QPen(m_color, m_size, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(from, to);
    return segmentBounds(from, to, m_size / 2.0 + 1);
}

QString PencilTool::actionName() const
{
    return QCoreApplication::translate("paint::Tool", "Pencil");
}

void BrushTool::beginStroke(QPointF point)
{
    m_type = m_context.settings().brushType();
    m_random.seed(quint32(std::floor(point.x())) * 73856093u ^ quint32(std::floor(point.y())) * 19349663u);
    m_bristles.reset();
    if (m_type == BrushType::OilBrush) {
        m_bristles.emplace(m_color, m_size, m_random.generate());
    }
}

StrokeStyle BrushTool::texture() const
{
    switch (m_type) {
    case BrushType::Crayon:
        return StrokeStyle::Crayon;
    case BrushType::Marker:
        return StrokeStyle::Marker;
    case BrushType::NaturalPencil:
        return StrokeStyle::NaturalPencil;
    case BrushType::Watercolor:
        return StrokeStyle::Watercolor;
    default:
        return StrokeStyle::Solid;
    }
}

QRect BrushTool::drawSegment(QPainter &painter, QPointF from, QPointF to)
{
    switch (m_type) {
    case BrushType::CalligraphyBrush:
        return drawNib(painter, from, to, 45);
    case BrushType::CalligraphyPen:
        return drawNib(painter, from, to, 135);
    case BrushType::Airbrush:
        return spray(painter, from, to);
    case BrushType::OilBrush:
        if (m_bristles) {
            return m_bristles->drawSegment(painter, from, to);
        }
        break;
    default:
        break;
    }
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(m_color, m_size, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (from == to) {
        // A zero-length line with a round cap paints a dot.
        painter.drawLine(from, to + QPointF(0.01, 0));
    } else {
        painter.drawLine(from, to);
    }
    // Watercolour finds its edges a few pixels away, so recolour that far around the new part.
    const qreal reach = m_size / 2.0 + 1 + (m_type == BrushType::Watercolor ? WatercolorEdge + 1 : 0);
    return segmentBounds(from, to, reach);
}

QRect BrushTool::drawNib(QPainter &painter, QPointF from, QPointF to, qreal degrees) const
{
    // A flat nib held at a fixed slant: wide strokes across it, hairlines along it.
    const qreal radians = degrees * std::numbers::pi / 180;
    const QPointF half = QPointF(std::cos(radians), -std::sin(radians)) * (m_size / 2.0);
    const qreal thickness = std::max(1.0, m_size * 0.2);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(m_color, thickness, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(m_color);
    painter.drawPolygon(QPolygonF{from + half, to + half, to - half, from - half});
    return segmentBounds(from, to, m_size / 2.0 + thickness);
}

QRect BrushTool::spray(QPainter &painter, QPointF from, QPointF to)
{
    // Single-pixel dots scattered evenly over the nozzle's circle for every pixel moved.
    const qreal radius = m_size / 2.0;
    const int dots = std::max(1, int(std::lround(std::numbers::pi * radius * radius * 0.02)));
    const int steps = std::max(1, int(std::ceil(std::hypot(to.x() - from.x(), to.y() - from.y()))));
    painter.setRenderHint(QPainter::Antialiasing, false);
    for (int step = from == to ? 0 : 1; step <= steps; ++step) {
        const QPointF centre = from + (to - from) * (qreal(step) / steps);
        for (int i = 0; i < dots; ++i) {
            const qreal angle = m_random.generateDouble() * 2 * std::numbers::pi;
            const qreal distance = std::sqrt(m_random.generateDouble()) * radius;
            painter.fillRect(QRect(int(std::floor(centre.x() + distance * std::cos(angle))),
                                   int(std::floor(centre.y() + distance * std::sin(angle))), 1, 1),
                             m_color);
        }
    }
    return segmentBounds(from, to, radius + 1);
}

void BrushTool::drawStroke(QPainter &painter, const QRect &area) const
{
    const StrokeStyle style = texture();
    if (style == StrokeStyle::Solid) {
        StrokeTool::drawStroke(painter, area);
        return;
    }
    // The buffer holds where the stroke went; colour that with the paper grain.
    const int margin = style == StrokeStyle::Watercolor ? WatercolorEdge + 1 : 0;
    const QRect around = area.adjusted(-margin, -margin, margin, margin).intersected(strokeBuffer().rect());
    const QImage coverage = strokeBuffer().copy(around).convertToFormat(QImage::Format_Alpha8);
    painter.drawImage(area.topLeft(), textured(coverage, style, m_color, around.topLeft()),
                      area.translated(-around.topLeft()));
}

QString BrushTool::actionName() const
{
    return QCoreApplication::translate("paint::Tool", "Brush");
}

QRect EraserTool::drawSegment(QPainter &painter, QPointF from, QPointF to)
{
    // Axis-aligned square stamps every pixel along the segment, so the eraser keeps its shape.
    painter.setRenderHint(QPainter::Antialiasing, false);
    const qreal half = m_size / 2.0;
    const qreal length = std::hypot(to.x() - from.x(), to.y() - from.y());
    const int steps = std::max(1, int(std::ceil(length)));
    for (int i = 0; i <= steps; ++i) {
        const QPointF centre = from + (to - from) * (qreal(i) / steps);
        painter.fillRect(QRectF(std::round(centre.x() - half), std::round(centre.y() - half), m_size, m_size),
                         Qt::black);
    }
    return segmentBounds(from, to, half + 1);
}

QString EraserTool::actionName() const
{
    return QCoreApplication::translate("paint::Tool", "Eraser");
}

} // namespace paint
