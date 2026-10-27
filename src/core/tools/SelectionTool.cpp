#include "tools/SelectionTool.h"

#include "document/Document.h"
#include "document/History.h"
#include "tools/ToolSettings.h"

#include <QCoreApplication>
#include <QPainter>

#include <cmath>

namespace paint {

namespace {

QPainterPath rectPath(const QRectF &rect)
{
    QPainterPath path;
    path.addRect(rect);
    return path;
}

// Makes everything outside `shape` transparent (fillPath only touches pixels inside the path,
// so the complement is cleared instead).
void clearOutside(QImage &image, const QPainterPath &shape)
{
    QPainterPath outside;
    outside.addRect(QRectF(image.rect()));
    outside = outside.subtracted(shape);
    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.fillPath(outside, Qt::black);
}

// Pixels of `color` become transparent: Paint's "transparent selection" drops the background colour.
QImage withoutColor(const QImage &image, const QColor &color)
{
    QImage result = image.convertToFormat(QImage::Format_ARGB32);
    const QRgb key = color.rgb();
    for (int y = 0; y < result.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            if (qAlpha(line[x]) == 255 && (line[x] & RGB_MASK) == (key & RGB_MASK)) {
                line[x] = 0;
            }
        }
    }
    return result.convertToFormat(Document::LayerFormat);
}

} // namespace

SelectionTool::SelectionTool(ToolContext &context)
    : InteractiveTool(context)
{}

bool SelectionTool::isTransforming() const
{
    return m_drag == Drag::Move || m_drag == Drag::Resize || m_drag == Drag::Rotate;
}

QRectF SelectionTool::frame() const
{
    return m_floating ? m_target : m_path.boundingRect();
}

QTransform SelectionTool::contentTransform() const
{
    // Maps content pixels onto the target box, rotated around its centre.
    QTransform transform;
    const QPointF centre = m_target.center();
    transform.translate(centre.x(), centre.y());
    transform.rotate(m_angle);
    transform.scale(m_target.width() / m_content.width(), m_target.height() / m_content.height());
    transform.translate(-m_content.width() / 2.0, -m_content.height() / 2.0);
    return transform;
}

QPainterPath SelectionTool::outline() const
{
    return m_floating ? contentTransform().map(m_contentShape) : m_path;
}

QRect SelectionTool::bounds() const
{
    return hasSelection() ? outline().boundingRect().toAlignedRect() : QRect();
}

QList<QPointF> SelectionTool::handlePositions(qreal screenPixel) const
{
    if (!hasSelection()) {
        return {};
    }
    return box::handles(frame(), m_floating ? m_angle : 0, screenPixel);
}

SelectionHandle SelectionTool::hitTest(QPointF position, qreal screenPixel) const
{
    if (!hasSelection()) {
        return SelectionHandle::None;
    }
    return box::hitTest(handlePositions(screenPixel), outline(), position, screenPixel);
}

void SelectionTool::press(const PointerEvent &event)
{
    // The right button opens Paint's context menu instead.
    if (event.button != Qt::LeftButton || m_drag != Drag::None) {
        return;
    }
    m_pressPosition = event.position;
    const SelectionHandle handle = hitTest(event.position, event.screenPixel);
    if (handle != SelectionHandle::None) {
        // Ctrl+drag moves a copy and leaves the original in place.
        const bool duplicate = (event.modifiers & Qt::ControlModifier) && handle == SelectionHandle::Inside;
        if (!m_floating) {
            lift(duplicate);
        } else if (duplicate) {
            m_base = m_context.document().layer(m_layer).image.copy();
            m_shown = {};
        }
        if (!m_floating) {
            return;
        }
        m_handle = handle;
        m_drag = handle == SelectionHandle::Inside   ? Drag::Move
                 : handle == SelectionHandle::Rotate ? Drag::Rotate
                                                     : Drag::Resize;
        m_startTarget = m_target;
        m_startAngle = m_angle;
        notify();
        return;
    }
    commit();
    m_drag = Drag::Define;
    m_lasso = {event.position};
    notify();
}

void SelectionTool::move(const PointerEvent &event)
{
    switch (m_drag) {
    case Drag::None:
        return;
    case Drag::Define:
        defineTo(event.position, event.modifiers);
        return;
    case Drag::Move: {
        // Whole pixels keep unscaled content crisp.
        const QPointF delta = event.position - m_pressPosition;
        m_target = m_startTarget.translated(std::round(delta.x()), std::round(delta.y()));
        render();
        return;
    }
    case Drag::Resize:
        resizeTo(event.position, event.modifiers);
        return;
    case Drag::Rotate:
        rotateTo(event.position, event.modifiers);
        return;
    }
}

void SelectionTool::release(const PointerEvent &event)
{
    if (event.button != Qt::LeftButton || m_drag == Drag::None) {
        return;
    }
    if (m_drag == Drag::Define) {
        defineTo(event.position, event.modifiers);
        const QRectF box = m_path.boundingRect();
        if (box.width() < 1 || box.height() < 1) {
            m_path = {}; // a click selects nothing
        }
    }
    m_drag = Drag::None;
    m_handle = SelectionHandle::None;
    notify();
}

void SelectionTool::defineTo(QPointF position, Qt::KeyboardModifiers modifiers)
{
    const QRectF canvas = m_context.document().rect();
    if (m_context.settings().tool() == Tool::FreeformSelect) {
        m_lasso.append(position);
        QPainterPath path;
        path.addPolygon(m_lasso);
        path.closeSubpath();
        m_path = path.intersected(rectPath(canvas)).simplified();
    } else {
        QPointF end = position;
        if (modifiers & Qt::ShiftModifier) {
            // Shift draws a square selection.
            const qreal side =
                std::max(std::abs(end.x() - m_pressPosition.x()), std::abs(end.y() - m_pressPosition.y()));
            end = m_pressPosition + QPointF(end.x() < m_pressPosition.x() ? -side : side,
                                            end.y() < m_pressPosition.y() ? -side : side);
        }
        const QRectF box = QRectF(m_pressPosition, end).normalized();
        // Rectangle selections cover whole pixels.
        const QRectF pixels(QPointF(std::floor(box.left()), std::floor(box.top())),
                            QPointF(std::ceil(box.right()), std::ceil(box.bottom())));
        m_path = rectPath(pixels.intersected(canvas));
    }
    notify();
}

void SelectionTool::resizeTo(QPointF position, Qt::KeyboardModifiers modifiers)
{
    m_target = box::resized(m_startTarget, m_angle, m_handle, position, modifiers & Qt::ShiftModifier);
    render();
}

void SelectionTool::rotateTo(QPointF position, Qt::KeyboardModifiers modifiers)
{
    m_angle = box::rotated(m_startTarget.center(), m_startAngle, m_pressPosition, position,
                           modifiers & Qt::ShiftModifier);
    render();
}

void SelectionTool::lift(bool keepOriginal)
{
    Document &document = m_context.document();
    const QRect box = m_path.boundingRect().toAlignedRect().intersected(document.rect());
    if (box.isEmpty()) {
        m_path = {};
        return;
    }
    m_layer = document.activeLayerIndex();
    QImage &layer = document.layer(m_layer).image;
    m_before = layer;
    m_contentShape = m_path.translated(-box.topLeft());
    m_content = layer.copy(box);
    // Only pixels inside a free-form outline belong to the selection.
    clearOutside(m_content, m_contentShape);
    m_base = layer.copy();
    if (!keepOriginal) {
        QPainter painter(&m_base);
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.fillPath(m_path, Qt::black);
    }
    m_target = QRectF(box);
    m_angle = 0;
    m_floating = true;
    m_path = {};
    m_dirty = box;
    m_shown = box;
    render();
}

void SelectionTool::paste(const QImage &image, QPointF topLeft)
{
    commit();
    if (image.isNull()) {
        return;
    }
    Document &document = m_context.document();
    m_layer = document.activeLayerIndex();
    m_before = document.layer(m_layer).image;
    m_base = m_before.copy();
    m_content = image.convertToFormat(Document::LayerFormat);
    m_contentShape = rectPath(QRectF(m_content.rect()));
    m_target = QRectF(topLeft, QSizeF(m_content.size()));
    m_angle = 0;
    m_floating = true;
    m_dirty = {};
    m_shown = {};
    render();
    notify();
}

void SelectionTool::refresh()
{
    if (m_floating) {
        render();
    }
}

void SelectionTool::render()
{
    Document &document = m_context.document();
    QImage &layer = document.layer(m_layer).image;
    const QTransform transform = contentTransform();
    const QRect area = transform.mapRect(QRectF(m_content.rect()))
                           .toAlignedRect()
                           .adjusted(-1, -1, 1, 1)
                           .intersected(document.rect());
    const bool transformed = m_angle != 0 || m_target.size() != QSizeF(m_content.size());

    QPainter painter(&layer);
    // Put back what the content covered last time, then draw it at its new place.
    if (!m_shown.isEmpty()) {
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(m_shown.topLeft(), m_base, m_shown);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform, transformed);
    painter.setTransform(transform);
    const ToolSettings &settings = m_context.settings();
    painter.drawImage(0, 0,
                      settings.transparentSelection()
                          ? withoutColor(m_content, settings.color(ColorSlot::Secondary))
                          : m_content);
    painter.end();

    const QRect changed = m_shown | area;
    m_shown = area;
    m_dirty |= area;
    m_context.documentChanged(changed);
}

void SelectionTool::placeContent(bool keepSelection)
{
    if (!m_floating) {
        if (!keepSelection) {
            clearSelection();
        }
        return;
    }
    const QImage &layer = m_context.document().layer(m_layer).image;
    // A selection that was lifted and put back unchanged leaves no undo step.
    if (!m_dirty.isEmpty() && m_before.copy(m_dirty) != layer.copy(m_dirty)) {
        m_context.history().recordPixelEdit(QCoreApplication::translate("paint::Tool", "Selection"), m_layer,
                                            m_dirty, m_before.copy(m_dirty));
    }
    const QPainterPath placed = outline();
    clearSelection();
    if (keepSelection) {
        m_path = placed;
    }
    notify();
}

void SelectionTool::clearSelection()
{
    m_floating = false;
    m_path = {};
    m_content = {};
    m_contentShape = {};
    m_before = {};
    m_base = {};
    m_dirty = {};
    m_shown = {};
    m_angle = 0;
}

void SelectionTool::commit()
{
    m_drag = Drag::None;
    m_handle = SelectionHandle::None;
    const bool had = hasSelection();
    placeContent(false);
    if (had) {
        notify();
    }
}

void SelectionTool::cancel()
{
    m_drag = Drag::None;
    m_handle = SelectionHandle::None;
    if (m_floating) {
        // Back to the layer as it was before lifting or pasting.
        QImage &layer = m_context.document().layer(m_layer).image;
        QPainter painter(&layer);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(m_dirty.topLeft(), m_before, m_dirty);
        painter.end();
        const QRect dirty = m_dirty;
        clearSelection();
        m_context.documentChanged(dirty);
    } else {
        clearSelection();
    }
    notify();
}

void SelectionTool::deleteSelected()
{
    if (!hasSelection()) {
        return;
    }
    if (!m_floating) {
        lift(false);
        if (!m_floating) {
            return;
        }
    }
    // Drop the floating content: the layer shows what was under it (transparency when lifted).
    QImage &layer = m_context.document().layer(m_layer).image;
    QPainter painter(&layer);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(m_shown.topLeft(), m_base, m_shown);
    painter.end();
    m_context.documentChanged(m_shown);
    m_shown = {};
    placeContent(false);
}

void SelectionTool::transformSelection(const std::function<ImageOp(const QImage &)> &operation)
{
    if (!hasSelection()) {
        return;
    }
    // Bake any earlier move, scale or rotation into the layer, then work on plain pixels.
    place();
    lift(false);
    if (!m_floating) {
        return;
    }
    const QPointF centre = m_target.center();
    ImageOp result = operation(m_content);
    m_contentShape = result.mapping.map(m_contentShape);
    m_content = result.image.convertToFormat(Document::LayerFormat);
    QRectF target(QPointF(), QSizeF(m_content.size()));
    target.moveCenter(centre);
    target.moveTopLeft(QPointF(std::round(target.left()), std::round(target.top())));
    m_target = target;
    render();
    notify();
}

void SelectionTool::rotateBy(qreal degrees)
{
    if (!hasSelection()) {
        return;
    }
    if (!m_floating) {
        lift(false);
    }
    if (m_floating) {
        m_angle = std::remainder(m_angle + degrees, 360.0);
        render();
        notify();
    }
}

void SelectionTool::place()
{
    m_drag = Drag::None;
    placeContent(true);
}

void SelectionTool::selectAll()
{
    commit();
    m_path = rectPath(QRectF(m_context.document().rect()));
    notify();
}

void SelectionTool::invert()
{
    if (!hasSelection()) {
        return;
    }
    placeContent(true);
    m_path = rectPath(QRectF(m_context.document().rect())).subtracted(m_path).simplified();
    notify();
}

QImage SelectionTool::copy(bool visibleLayers) const
{
    const Document &document = m_context.document();
    const QPainterPath shape = hasSelection() ? outline() : rectPath(QRectF(document.rect()));
    const QRect box = shape.boundingRect().toAlignedRect().intersected(document.rect());
    if (box.isEmpty()) {
        return {};
    }
    QImage image =
        visibleLayers ? document.composite(box) : document.layer(document.activeLayerIndex()).image.copy(box);
    clearOutside(image, shape.translated(-box.topLeft()));
    return image;
}

bool SelectionTool::keyPress(int key, Qt::KeyboardModifiers)
{
    if (!hasSelection() || m_drag != Drag::None) {
        return false;
    }
    QPointF step;
    switch (key) {
    case Qt::Key_Left:
        step = {-1, 0};
        break;
    case Qt::Key_Right:
        step = {1, 0};
        break;
    case Qt::Key_Up:
        step = {0, -1};
        break;
    case Qt::Key_Down:
        step = {0, 1};
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Escape:
        commit();
        return true;
    default:
        return false;
    }
    // Arrow keys nudge the selection by one pixel.
    if (!m_floating) {
        lift(false);
    }
    if (m_floating) {
        m_target.translate(step);
        render();
        notify();
    }
    return true;
}

void SelectionTool::notify()
{
    m_context.selectionChanged();
}

} // namespace paint
