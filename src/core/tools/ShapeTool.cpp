#include "tools/ShapeTool.h"

#include "document/Document.h"
#include "document/History.h"
#include "shapes/ShapeGeometry.h"
#include "tools/ToolSettings.h"

#include <QCoreApplication>
#include <QLineF>
#include <QPainter>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace paint {

namespace {

// How close to its first corner (in screen pixels) a click closes a polygon.
constexpr qreal CloseDistance = 6;

// Shapes drawn by dragging out a box, as opposed to lines, curves and polygons.
constexpr bool fillsBox(ShapeType type)
{
    return type != ShapeType::Line && type != ShapeType::Curve && type != ShapeType::Polygon;
}

QPointF pixelCentre(QPointF point)
{
    return QPointF(std::floor(point.x()) + 0.5, std::floor(point.y()) + 0.5);
}

// Shift keeps a segment horizontal, vertical or at 45°.
QPointF constrained(QPointF from, QPointF to)
{
    QPointF delta = to - from;
    const qreal dx = std::abs(delta.x());
    const qreal dy = std::abs(delta.y());
    const qreal tan22 = std::tan(22.5 * std::numbers::pi / 180);
    if (dy < dx * tan22) {
        delta.setY(0);
    } else if (dx < dy * tan22) {
        delta.setX(0);
    } else {
        const qreal side = std::max(dx, dy);
        delta = QPointF(std::copysign(side, delta.x()), std::copysign(side, delta.y()));
    }
    return from + delta;
}

// The box covering the pixels under `a` and `b`; Shift makes it a square.
QRectF pixelBox(QPointF a, QPointF b, bool square)
{
    const QPointF from(std::floor(a.x()), std::floor(a.y()));
    QPointF to(std::floor(b.x()), std::floor(b.y()));
    if (square) {
        const qreal side = std::max(std::abs(to.x() - from.x()), std::abs(to.y() - from.y()));
        to = from + QPointF(to.x() < from.x() ? -side : side, to.y() < from.y() ? -side : side);
    }
    return QRectF(QPointF(std::min(from.x(), to.x()), std::min(from.y(), to.y())),
                  QPointF(std::max(from.x(), to.x()) + 1, std::max(from.y(), to.y()) + 1));
}

QTransform boxMapping(const QRectF &from, const QRectF &to)
{
    QTransform transform;
    transform.translate(to.left(), to.top());
    transform.scale(to.width() / from.width(), to.height() / from.height());
    transform.translate(-from.left(), -from.top());
    return transform;
}

qreal distanceToSegment(QPointF point, QPointF a, QPointF b)
{
    const QPointF ab = b - a;
    const qreal lengthSquared = QPointF::dotProduct(ab, ab);
    const qreal t =
        lengthSquared > 0 ? std::clamp(QPointF::dotProduct(point - a, ab) / lengthSquared, 0.0, 1.0) : 0.0;
    return QLineF(point, a + ab * t).length();
}

} // namespace

ShapeStyle ShapeTool::style() const
{
    const ToolSettings &settings = m_context.settings();
    ShapeStyle style;
    style.outline = settings.outlineStyle();
    style.fill = settings.fillStyle();
    style.outlineColor = settings.color(m_swapColors ? ColorSlot::Secondary : ColorSlot::Primary);
    style.fillColor = settings.color(m_swapColors ? ColorSlot::Primary : ColorSlot::Secondary);
    style.width = settings.sizeFor(Tool::Shape);
    // Thin lines end on whole pixels; thicker ones get round ends.
    style.cap = style.width <= 2 ? Qt::SquareCap : Qt::RoundCap;
    style.join = m_type == ShapeType::Curve ? Qt::RoundJoin : Qt::SvgMiterJoin;
    return style;
}

QRectF ShapeTool::insetFrame(const ShapeStyle &style) const
{
    // The outline runs inside the dragged box, so the finished shape fills exactly that box.
    const qreal half = style.outline == StrokeStyle::None
                           ? 0
                           : std::min(style.width, std::min(m_frame.width(), m_frame.height())) / 2;
    return m_frame.adjusted(half, half, -half, -half);
}

QPainterPath ShapeTool::path(const ShapeStyle &style) const
{
    QPainterPath path;
    if (m_type == ShapeType::Line || (m_mode == Mode::Drawing && !fillsBox(m_type))) {
        path.moveTo(m_start);
        path.lineTo(m_end);
        return path;
    }
    if (m_mode == Mode::Bending) {
        path.moveTo(m_start);
        path.cubicTo(m_control1, m_control2, m_end);
        return path;
    }
    if (m_mode == Mode::Polygon) {
        path.addPolygon(m_corners);
        return path;
    }
    path = fillsBox(m_type) ? shapePath(m_type, insetFrame(style))
                            : boxMapping(m_baseFrame, m_frame).map(m_basePath);
    return m_angle == 0 ? path : box::rotationAround(m_frame.center(), m_angle).map(path);
}

bool ShapeTool::isClosed() const
{
    if (m_type == ShapeType::Polygon) {
        return m_mode == Mode::Editing;
    }
    return shapeIsClosed(m_type);
}

void ShapeTool::press(const PointerEvent &event)
{
    if (event.button != Qt::LeftButton && event.button != Qt::RightButton) {
        return;
    }
    m_screenPixel = event.screenPixel;
    switch (m_mode) {
    case Mode::Idle:
        begin(event);
        return;
    case Mode::Drawing:
        return;
    case Mode::Bending:
        m_drag = Drag::Bend;
        bendTo(event.position);
        return;
    case Mode::Polygon:
        if (event.doubleClick) {
            closePolygon();
            return;
        }
        m_drag = Drag::Corner;
        m_corners.append(m_corners.isEmpty() || !(event.modifiers & Qt::ShiftModifier)
                             ? pixelCentre(event.position)
                             : constrained(m_corners.last(), pixelCentre(event.position)));
        render();
        notify();
        return;
    case Mode::Editing:
        break;
    }
    // The right button always starts a new shape, in swapped colours.
    const BoxHandle handle =
        event.button == Qt::LeftButton ? hitTest(event.position, event.screenPixel) : BoxHandle::None;
    if (handle == BoxHandle::None) {
        finish();
        begin(event);
        return;
    }
    m_handle = handle;
    m_drag = handle == BoxHandle::Inside   ? Drag::Move
             : handle == BoxHandle::Rotate ? Drag::Rotate
             : m_type == ShapeType::Line   ? Drag::Endpoint
                                           : Drag::Resize;
    m_pressPosition = event.position;
    m_startFrame = m_frame;
    m_startAngle = m_angle;
    m_startStart = m_start;
    m_startEnd = m_end;
    notify();
}

void ShapeTool::move(const PointerEvent &event)
{
    switch (m_drag) {
    case Drag::None:
        return;
    case Drag::Draw:
        drawTo(event.position, event.modifiers);
        return;
    case Drag::Bend:
        bendTo(event.position);
        return;
    case Drag::Corner: {
        const QPointF corner = pixelCentre(event.position);
        m_corners.last() = m_corners.size() > 1 && (event.modifiers & Qt::ShiftModifier)
                               ? constrained(m_corners.at(m_corners.size() - 2), corner)
                               : corner;
        render();
        notify();
        return;
    }
    case Drag::Move:
    case Drag::Resize:
    case Drag::Rotate:
    case Drag::Endpoint:
        dragTo(event.position, event.modifiers);
        return;
    }
}

void ShapeTool::release(const PointerEvent &event)
{
    if (m_drag == Drag::None) {
        return;
    }
    move(event);
    const Drag drag = m_drag;
    m_drag = Drag::None;
    m_handle = BoxHandle::None;
    switch (drag) {
    case Drag::Draw:
        finishDrawing();
        return;
    case Drag::Bend:
        if (++m_bends == 2) {
            startEditing();
        }
        return;
    case Drag::Corner:
        // Clicking the first corner again closes the polygon.
        if (m_corners.size() > 2 &&
            QLineF(m_corners.last(), m_corners.first()).length() <= CloseDistance * m_screenPixel) {
            m_corners.removeLast();
            closePolygon();
        }
        return;
    default:
        notify();
        return;
    }
}

void ShapeTool::begin(const PointerEvent &event)
{
    Document &document = m_context.document();
    m_type = m_context.settings().shapeType();
    m_swapColors = event.button == Qt::RightButton;
    m_layer = document.activeLayerIndex();
    m_before = document.layer(m_layer).image; // shared until the shape is drawn
    m_dirty = {};
    m_shown = {};
    m_angle = 0;
    m_bends = 0;
    m_corners.clear();
    m_basePath = {};
    m_pressPosition = event.position;
    m_start = m_end = pixelCentre(event.position);
    m_frame = pixelBox(event.position, event.position, false);
    m_mode = Mode::Drawing;
    m_drag = Drag::Draw;
    notify();
}

void ShapeTool::drawTo(QPointF position, Qt::KeyboardModifiers modifiers)
{
    const bool shift = modifiers & Qt::ShiftModifier;
    if (fillsBox(m_type)) {
        m_frame = pixelBox(m_pressPosition, position, shift);
    } else {
        m_end = shift ? constrained(m_start, pixelCentre(position)) : pixelCentre(position);
    }
    render();
    notify();
}

void ShapeTool::finishDrawing()
{
    const bool clicked = fillsBox(m_type) ? m_frame.width() <= 1 && m_frame.height() <= 1 : m_start == m_end;
    if (m_type == ShapeType::Polygon) {
        // The first drag is the polygon's first side; a click just places its first corner.
        m_corners = {m_start};
        if (!clicked) {
            m_corners.append(m_end);
        }
        m_mode = Mode::Polygon;
        render();
        notify();
        return;
    }
    if (clicked) {
        // A click without a drag draws nothing.
        cancel();
        return;
    }
    if (m_type == ShapeType::Curve) {
        m_control1 = m_start + (m_end - m_start) / 3;
        m_control2 = m_start + (m_end - m_start) * 2 / 3;
        m_bends = 0;
        m_mode = Mode::Bending;
        notify();
        return;
    }
    startEditing();
}

void ShapeTool::bendTo(QPointF position)
{
    // As in Paint, the pointer pulls the curve towards it: the first bend moves both control
    // points there, the second only the one nearer the end.
    if (m_bends == 0) {
        m_control1 = position;
    }
    m_control2 = position;
    render();
    notify();
}

void ShapeTool::closePolygon()
{
    // Double clicks leave repeated corners behind.
    QPolygonF corners;
    for (const QPointF &corner : std::as_const(m_corners)) {
        if (corners.isEmpty() || corners.last() != corner) {
            corners.append(corner);
        }
    }
    if (corners.size() > 1 && corners.first() == corners.last()) {
        corners.removeLast();
    }
    if (corners.size() < 2) {
        cancel();
        return;
    }
    m_corners = std::move(corners);
    startEditing();
}

void ShapeTool::startEditing()
{
    if (!fillsBox(m_type) && m_type != ShapeType::Line) {
        // Curves and polygons keep their drawn outline and stretch it with the frame.
        QPainterPath outline;
        if (m_type == ShapeType::Curve) {
            outline.moveTo(m_start);
            outline.cubicTo(m_control1, m_control2, m_end);
        } else {
            outline.addPolygon(m_corners);
            outline.closeSubpath();
        }
        const ShapeStyle current = style();
        const qreal half = current.outline == StrokeStyle::None ? 0 : current.width / 2;
        QRectF frame = outline.boundingRect().adjusted(-half, -half, half, half);
        if (frame.width() < 1) {
            frame.adjust(-(1 - frame.width()) / 2, 0, (1 - frame.width()) / 2, 0);
        }
        if (frame.height() < 1) {
            frame.adjust(0, -(1 - frame.height()) / 2, 0, (1 - frame.height()) / 2);
        }
        m_basePath = std::move(outline);
        m_baseFrame = frame;
        m_frame = frame;
    }
    m_angle = 0;
    m_mode = Mode::Editing;
    m_drag = Drag::None;
    render();
    notify();
}

void ShapeTool::dragTo(QPointF position, Qt::KeyboardModifiers modifiers)
{
    const QPointF delta = position - m_pressPosition;
    const bool shift = modifiers & Qt::ShiftModifier;
    switch (m_drag) {
    case Drag::Move: {
        // Whole pixels keep thin outlines crisp.
        const QPointF step(std::round(delta.x()), std::round(delta.y()));
        m_frame = m_startFrame.translated(step);
        m_start = m_startStart + step;
        m_end = m_startEnd + step;
        break;
    }
    case Drag::Resize: {
        QRectF frame = box::resized(m_startFrame, m_angle, m_handle, position, shift);
        if (m_angle == 0 && fillsBox(m_type)) {
            frame = QRectF(QPointF(std::round(frame.left()), std::round(frame.top())),
                           QPointF(std::round(frame.right()), std::round(frame.bottom())));
            frame.setWidth(std::max<qreal>(1, frame.width()));
            frame.setHeight(std::max<qreal>(1, frame.height()));
        }
        m_frame = frame;
        break;
    }
    case Drag::Rotate:
        m_angle = box::rotated(m_startFrame.center(), m_startAngle, m_pressPosition, position, shift);
        break;
    case Drag::Endpoint: {
        const bool start = m_handle == BoxHandle::Left;
        const QPointF fixed = start ? m_end : m_start;
        const QPointF point = shift ? constrained(fixed, pixelCentre(position)) : pixelCentre(position);
        (start ? m_start : m_end) = point;
        break;
    }
    default:
        return;
    }
    render();
    notify();
}

void ShapeTool::nudge(QPointF step)
{
    m_frame.translate(step);
    m_start += step;
    m_end += step;
    render();
    notify();
}

void ShapeTool::render()
{
    Document &document = m_context.document();
    QImage &layer = document.layer(m_layer).image;
    const ShapeStyle current = style();
    const QPainterPath shape = path(current);
    const QRect area = shapePaintBounds(shape, current).toAlignedRect().intersected(document.rect());

    QPainter painter(&layer);
    if (!m_shown.isEmpty()) {
        // Put back what the shape covered last time.
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(m_shown.topLeft(), m_before, m_shown);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }
    if (!area.isEmpty()) {
        painter.setClipRect(area);
        paintShape(painter, shape, isClosed(), current, fillsBox(m_type) ? m_frame.topLeft() : m_start);
    }
    painter.end();

    const QRect changed = m_shown | area;
    m_shown = area;
    m_dirty |= area;
    if (!changed.isEmpty()) {
        m_context.documentChanged(changed);
    }
}

void ShapeTool::refresh()
{
    if (m_mode != Mode::Idle) {
        render();
    }
}

void ShapeTool::finish()
{
    if (m_mode == Mode::Idle) {
        return;
    }
    const QImage &layer = m_context.document().layer(m_layer).image;
    if (!m_dirty.isEmpty() && m_before.copy(m_dirty) != layer.copy(m_dirty)) {
        m_context.history().recordPixelEdit(QCoreApplication::translate("paint::Tool", "Shape"), m_layer,
                                            m_dirty, m_before.copy(m_dirty));
    }
    reset();
    notify();
}

void ShapeTool::commit()
{
    if (m_mode == Mode::Polygon) {
        closePolygon();
    }
    finish();
}

void ShapeTool::cancel()
{
    if (m_mode == Mode::Idle) {
        return;
    }
    const QRect dirty = m_dirty;
    if (!dirty.isEmpty()) {
        QPainter painter(&m_context.document().layer(m_layer).image);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(dirty.topLeft(), m_before, dirty);
    }
    reset();
    if (!dirty.isEmpty()) {
        m_context.documentChanged(dirty);
    }
    notify();
}

void ShapeTool::reset()
{
    m_mode = Mode::Idle;
    m_drag = Drag::None;
    m_handle = BoxHandle::None;
    m_before = {};
    m_dirty = {};
    m_shown = {};
    m_corners.clear();
    m_basePath = {};
    m_angle = 0;
}

bool ShapeTool::keyPress(int key, Qt::KeyboardModifiers)
{
    if (m_drag != Drag::None) {
        return false;
    }
    const bool enter = key == Qt::Key_Return || key == Qt::Key_Enter;
    switch (m_mode) {
    case Mode::Bending:
        // Enter keeps the curve with the bends it has.
        if (enter) {
            startEditing();
        }
        return enter;
    case Mode::Polygon:
        if (enter) {
            closePolygon();
        }
        return enter;
    case Mode::Editing:
        break;
    default:
        return false;
    }
    switch (key) {
    case Qt::Key_Left:
        nudge({-1, 0});
        return true;
    case Qt::Key_Right:
        nudge({1, 0});
        return true;
    case Qt::Key_Up:
        nudge({0, -1});
        return true;
    case Qt::Key_Down:
        nudge({0, 1});
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Escape:
        commit();
        return true;
    case Qt::Key_Delete:
        cancel();
        return true;
    default:
        return false;
    }
}

QPainterPath ShapeTool::frameOutline() const
{
    if (!isEditing() || m_type == ShapeType::Line) {
        return {};
    }
    QPainterPath frame;
    frame.addRect(m_frame);
    return m_angle == 0 ? frame : box::rotationAround(m_frame.center(), m_angle).map(frame);
}

QList<QPointF> ShapeTool::handlePositions(qreal screenPixel) const
{
    if (!isEditing()) {
        return {};
    }
    if (m_type == ShapeType::Line) {
        return {m_start, m_end};
    }
    return box::handles(m_frame, m_angle, screenPixel);
}

BoxHandle ShapeTool::hitTest(QPointF position, qreal screenPixel) const
{
    if (!isEditing()) {
        return BoxHandle::None;
    }
    if (m_type != ShapeType::Line) {
        return box::hitTest(handlePositions(screenPixel), frameOutline(), position, screenPixel);
    }
    const qreal toStart = QLineF(position, m_start).length();
    const qreal toEnd = QLineF(position, m_end).length();
    if (std::min(toStart, toEnd) <= (box::HandleRadius + 2) * screenPixel) {
        return toStart <= toEnd ? BoxHandle::Left : BoxHandle::Right;
    }
    const qreal reach = std::max<qreal>(m_context.settings().sizeFor(Tool::Shape) / 2.0, 4 * screenPixel);
    return distanceToSegment(position, m_start, m_end) <= reach ? BoxHandle::Inside : BoxHandle::None;
}

qreal ShapeTool::angle() const
{
    if (m_type == ShapeType::Line) {
        return std::atan2(m_end.y() - m_start.y(), m_end.x() - m_start.x()) * 180 / std::numbers::pi;
    }
    return m_angle;
}

QRectF ShapeTool::extent() const
{
    switch (m_mode) {
    case Mode::Idle:
        return {};
    case Mode::Editing:
        if (m_type != ShapeType::Line) {
            return box::rotationAround(m_frame.center(), m_angle).mapRect(m_frame);
        }
        break;
    case Mode::Drawing:
        if (fillsBox(m_type)) {
            return m_frame;
        }
        break;
    default:
        break;
    }
    // Lines, curves and polygons in progress: the pixels their corners sit on.
    return path(style()).boundingRect().adjusted(-0.5, -0.5, 0.5, 0.5);
}

void ShapeTool::notify()
{
    m_context.selectionChanged();
}

} // namespace paint
