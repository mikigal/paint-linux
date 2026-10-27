#include "tools/StrokeTools.h"

#include "document/Document.h"
#include "document/History.h"
#include "tools/ToolSettings.h"

#include <QCoreApplication>
#include <QPainter>

#include <cmath>

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
    painter.drawImage(area.topLeft(), m_stroke, area);
    painter.end();
    m_changed |= area;
    m_context.documentChanged(area);
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

QRect BrushTool::drawSegment(QPainter &painter, QPointF from, QPointF to)
{
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(m_color, m_size, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (from == to) {
        // A zero-length line with a round cap paints a dot.
        painter.drawLine(from, to + QPointF(0.01, 0));
    } else {
        painter.drawLine(from, to);
    }
    return segmentBounds(from, to, m_size / 2.0 + 1);
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
