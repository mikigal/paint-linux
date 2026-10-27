#include "canvas/CanvasView.h"

#include "document/Document.h"
#include "editor/Editor.h"
#include "io/MimeImport.h"
#include "tools/SelectionTool.h"
#include "tools/ToolSettings.h"
#include "ui/Icons.h"

#include <QContextMenuEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <ranges>

using namespace paint;

namespace {

// Room around the canvas for its resize handles.
constexpr int CanvasMargin = 24;
constexpr qreal HandleSize = 6;
// Below this on-screen diameter the brush outline is replaced by a crosshair.
constexpr qreal MinOutlineDiameter = 5;

QColor workspaceColor(const QPalette &palette)
{
    return palette.color(QPalette::Window).darker(ui::isDarkPalette(palette) ? 125 : 104);
}

// The grey and white squares shown behind transparent pixels.
QBrush checkerboardBrush()
{
    static const QBrush brush = [] {
        constexpr int Cell = 8;
        QPixmap tile(2 * Cell, 2 * Cell);
        tile.fill(Qt::white);
        QPainter painter(&tile);
        painter.fillRect(0, 0, Cell, Cell, QColor(0xcc, 0xcc, 0xcc));
        painter.fillRect(Cell, Cell, Cell, Cell, QColor(0xcc, 0xcc, 0xcc));
        return QBrush(tile);
    }();
    return brush;
}

} // namespace

const QList<double> &CanvasView::zoomPresets()
{
    static const QList<double> presets{0.125, 0.25, 0.5, 0.75, 1, 2, 3, 4, 5, 6, 7, 8};
    return presets;
}

CanvasView::CanvasView(Editor *editor, QWidget *parent)
    : QAbstractScrollArea(parent)
    , m_editor(editor)
{
    setObjectName(QStringLiteral("canvasView"));
    viewport()->setObjectName(QStringLiteral("canvasView.viewport"));
    setFrameShape(QFrame::NoFrame);
    setFocusPolicy(Qt::StrongFocus);
    viewport()->setMouseTracking(true);
    viewport()->setAttribute(Qt::WA_OpaquePaintEvent);

    connect(editor, &Editor::pixelsChanged, this, &CanvasView::handlePixelsChanged);
    connect(editor, &Editor::documentReplaced, this, &CanvasView::handleDocumentReplaced);
    connect(editor, &Editor::documentRestructured, this, &CanvasView::handleDocumentRestructured);
    connect(editor, &Editor::selectionStateChanged, viewport(), qOverload<>(&QWidget::update));
    setAcceptDrops(true);
    connect(editor, &Editor::zoomRequested, this, [this](QPointF point, bool in) {
        const QPointF anchor = documentToViewport(QRectF(point, QSizeF())).topLeft();
        in ? zoomIn(anchor) : zoomOut(anchor);
    });
    const ToolSettings &settings = editor->settings();
    connect(&settings, &ToolSettings::toolChanged, this, &CanvasView::updateCursor);
    connect(&settings, &ToolSettings::sizeChanged, this, &CanvasView::updateCursor);
    m_composite = editor->document().composite();
    updateScrollBars();
    updateCursor();
}

void CanvasView::handlePixelsChanged(const QRect &area)
{
    const QRect clipped = area.intersected(m_composite.rect());
    if (clipped.isEmpty()) {
        return;
    }
    QPainter painter(&m_composite);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(clipped.topLeft(), m_editor->document().composite(clipped));
    painter.end();
    viewport()->update(documentToViewport(clipped).toAlignedRect().adjusted(-2, -2, 2, 2));
}

void CanvasView::handleDocumentReplaced()
{
    m_composite = m_editor->document().composite();
    updateScrollBars();
    // Like Paint, show a large image whole and a small one at actual size.
    const QSizeF area = QSizeF(viewport()->size()) - QSizeF(2 * CanvasMargin, 2 * CanvasMargin);
    const QSizeF size = m_editor->document().size();
    if (size.width() > area.width() || size.height() > area.height()) {
        zoomToFit();
    } else {
        zoomToActualSize();
    }
    viewport()->update();
}

void CanvasView::handleDocumentRestructured()
{
    // Crop, canvas size or their undo: same zoom, new picture.
    m_composite = m_editor->document().composite();
    updateScrollBars();
    viewport()->update();
}

QTransform CanvasView::documentToViewportTransform() const
{
    const QRectF canvas = canvasRect();
    QTransform transform;
    transform.translate(canvas.left(), canvas.top());
    transform.scale(m_zoom, m_zoom);
    return transform;
}

QSizeF CanvasView::scaledCanvasSize() const
{
    return QSizeF(m_editor->document().size()) * m_zoom;
}

QRectF CanvasView::canvasRect() const
{
    const QSizeF size = scaledCanvasSize();
    const QSize area = viewport()->size();
    // Centred when it fits, otherwise placed by the scroll bars.
    const qreal x = size.width() + 2 * CanvasMargin <= area.width()
                        ? (area.width() - size.width()) / 2.0
                        : CanvasMargin - horizontalScrollBar()->value();
    const qreal y = size.height() + 2 * CanvasMargin <= area.height()
                        ? (area.height() - size.height()) / 2.0
                        : CanvasMargin - verticalScrollBar()->value();
    return QRectF(QPointF(std::round(x), std::round(y)), size);
}

QPointF CanvasView::viewportToDocument(QPointF point) const
{
    return (point - canvasRect().topLeft()) / m_zoom;
}

QRectF CanvasView::documentToViewport(const QRectF &rect) const
{
    return QRectF(canvasRect().topLeft() + rect.topLeft() * m_zoom, rect.size() * m_zoom);
}

void CanvasView::updateScrollBars()
{
    const QSizeF size = scaledCanvasSize();
    const QSize area = viewport()->size();
    horizontalScrollBar()->setRange(
        0, std::max(0, int(std::ceil(size.width() + 2 * CanvasMargin - area.width()))));
    verticalScrollBar()->setRange(
        0, std::max(0, int(std::ceil(size.height() + 2 * CanvasMargin - area.height()))));
    horizontalScrollBar()->setPageStep(area.width());
    verticalScrollBar()->setPageStep(area.height());
    horizontalScrollBar()->setSingleStep(20);
    verticalScrollBar()->setSingleStep(20);
}

void CanvasView::setZoom(double zoom, QPointF anchor)
{
    zoom = std::clamp(zoom, MinZoom, MaxZoom);
    if (qFuzzyCompare(zoom, m_zoom)) {
        return;
    }
    if (anchor.isNull()) {
        anchor = QRectF(viewport()->rect()).center();
    }
    // Keep the document point under the anchor in place.
    const QPointF documentPoint = viewportToDocument(anchor);
    m_zoom = zoom;
    updateScrollBars();
    const QPointF misplaced = canvasRect().topLeft() + documentPoint * m_zoom - anchor;
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + int(std::round(misplaced.x())));
    verticalScrollBar()->setValue(verticalScrollBar()->value() + int(std::round(misplaced.y())));
    viewport()->update();
    updateCursor();
    Q_EMIT zoomChanged(m_zoom);
}

void CanvasView::zoomIn(QPointF anchor)
{
    for (double preset : zoomPresets()) {
        if (preset > m_zoom + 1e-6) {
            setZoom(preset, anchor);
            return;
        }
    }
}

void CanvasView::zoomOut(QPointF anchor)
{
    for (double preset : zoomPresets() | std::views::reverse) {
        if (preset < m_zoom - 1e-6) {
            setZoom(preset, anchor);
            return;
        }
    }
}

void CanvasView::zoomToFit()
{
    const QSizeF area = QSizeF(viewport()->size()) - QSizeF(2 * CanvasMargin, 2 * CanvasMargin);
    const QSizeF document = m_editor->document().size();
    setZoom(std::min(area.width() / document.width(), area.height() / document.height()));
}

QList<QRectF> CanvasView::handleRects() const
{
    // Handles sit just outside the canvas: corners and the middle of each edge.
    const QRectF canvas = canvasRect().adjusted(-HandleSize / 2 - 1, -HandleSize / 2 - 1, HandleSize / 2 + 1,
                                                HandleSize / 2 + 1);
    QList<QRectF> rects;
    for (qreal fx : {0.0, 0.5, 1.0}) {
        for (qreal fy : {0.0, 0.5, 1.0}) {
            if (fx == 0.5 && fy == 0.5) {
                continue;
            }
            const QPointF centre(canvas.left() + canvas.width() * fx, canvas.top() + canvas.height() * fy);
            rects.append(
                QRectF(centre - QPointF(HandleSize / 2, HandleSize / 2), QSizeF(HandleSize, HandleSize)));
        }
    }
    return rects;
}

int CanvasView::canvasHandleAt(QPointF viewportPoint) const
{
    const QList<QRectF> rects = handleRects();
    for (qsizetype i = 0; i < rects.size(); ++i) {
        if (rects.at(i).adjusted(-4, -4, 4, 4).contains(viewportPoint)) {
            return int(i);
        }
    }
    return -1;
}

namespace {

// handleRects() order: x fractions 0, ½, 1 (outer loop) × y fractions 0, ½, 1, without the centre.
struct HandleEdges
{
    bool left, right, top, bottom;
};

HandleEdges edgesOf(int handle)
{
    static constexpr qreal fractions[] = {0.0, 0.5, 1.0};
    int index = handle >= 4 ? handle + 1 : handle; // skip the centre
    const qreal fx = fractions[index / 3];
    const qreal fy = fractions[index % 3];
    return {fx == 0.0, fx == 1.0, fy == 0.0, fy == 1.0};
}

Qt::CursorShape cursorFor(const HandleEdges &edges)
{
    if ((edges.left && edges.top) || (edges.right && edges.bottom)) {
        return Qt::SizeFDiagCursor;
    }
    if ((edges.right && edges.top) || (edges.left && edges.bottom)) {
        return Qt::SizeBDiagCursor;
    }
    return edges.left || edges.right ? Qt::SizeHorCursor : Qt::SizeVerCursor;
}

} // namespace

QRect CanvasView::canvasResizeTarget(QPointF viewportPoint) const
{
    // Moved edges follow the pointer in whole document pixels; the canvas keeps at least 1×1.
    const QPointF delta = (viewportPoint - m_resizeStart) / m_zoom;
    const HandleEdges edges = edgesOf(m_resizeHandle);
    const QSize size = m_editor->document().size();
    int left = 0, top = 0, right = size.width(), bottom = size.height();
    if (edges.left) {
        left = std::min(right - 1, int(std::round(delta.x())));
    }
    if (edges.right) {
        right = std::max(left + 1, size.width() + int(std::round(delta.x())));
    }
    if (edges.top) {
        top = std::min(bottom - 1, int(std::round(delta.y())));
    }
    if (edges.bottom) {
        bottom = std::max(top + 1, size.height() + int(std::round(delta.y())));
    }
    return QRect(QPoint(left, top), QPoint(right - 1, bottom - 1));
}

std::optional<QRectF> CanvasView::brushOutline() const
{
    const ToolSettings &settings = m_editor->settings();
    const Tool tool = settings.tool();
    if (!m_hover || (tool != Tool::Pencil && tool != Tool::Brush && tool != Tool::Eraser)) {
        return std::nullopt;
    }
    const qreal diameter = settings.size() * m_zoom;
    if (diameter < MinOutlineDiameter) {
        return std::nullopt;
    }
    QPointF centre = *m_hover;
    if (tool != Tool::Brush) {
        // Hard-edged tools paint whole pixels around the pixel under the pointer.
        centre = QPointF(std::floor(centre.x()) + 0.5, std::floor(centre.y()) + 0.5);
    }
    const QPointF viewportCentre = documentToViewport(QRectF(centre, QSizeF())).topLeft();
    return QRectF(viewportCentre - QPointF(diameter / 2, diameter / 2), QSizeF(diameter, diameter));
}

void CanvasView::paintEvent(QPaintEvent *event)
{
    QPainter painter(viewport());
    painter.fillRect(event->rect(), workspaceColor(palette()));

    const QRectF canvas = canvasRect();
    if (!m_editor->document().isBackgroundVisible()) {
        painter.fillRect(canvas, checkerboardBrush());
    }
    // Magnified pixels stay crisp; reduced images are smoothed.
    painter.setRenderHint(QPainter::SmoothPixmapTransform, m_zoom < 1.0);
    painter.drawImage(canvas, m_composite);

    painter.setRenderHint(QPainter::Antialiasing, false);
    QColor border = palette().color(QPalette::WindowText);
    border.setAlphaF(0.55);
    painter.setPen(QPen(border, 1));
    painter.setBrush(palette().color(QPalette::Base));
    for (const QRectF &handle : handleRects()) {
        painter.drawRect(handle.adjusted(0.5, 0.5, -0.5, -0.5));
    }

    paintSelection(painter);

    if (m_resizeTarget) {
        // The new canvas edge follows the pointer as a dashed outline until the button is released.
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setBrush(Qt::NoBrush);
        QPen dashes(palette().color(QPalette::WindowText), 1, Qt::DashLine);
        painter.setPen(dashes);
        painter.drawRect(documentToViewport(QRectF(*m_resizeTarget)).adjusted(-0.5, -0.5, -0.5, -0.5));
    }

    if (const std::optional<QRectF> outline = brushOutline()) {
        // A dark ring inside a light one stays visible on any colour.
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setBrush(Qt::NoBrush);
        const bool square = m_editor->settings().tool() == Tool::Eraser;
        for (const auto &[colour, inset] :
             {std::pair{QColor(255, 255, 255, 220), 0.5}, {QColor(0, 0, 0, 200), 1.5}}) {
            painter.setPen(QPen(colour, 1));
            const QRectF ring = outline->adjusted(inset, inset, -inset, -inset);
            square ? painter.drawRect(ring) : painter.drawEllipse(ring);
        }
    }
}

void CanvasView::paintSelection(QPainter &painter)
{
    const SelectionTool *selection = m_editor->selection();
    // Paint hides the outline while the selection is being moved, resized or rotated.
    if (!selection->hasSelection() || selection->isTransforming()) {
        return;
    }
    const QTransform toViewport = documentToViewportTransform();
    const QPainterPath outline = toViewport.map(selection->outline());
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(Qt::white, 1));
    painter.drawPath(outline);
    QPen dashes(QColor(0x1a, 0x1a, 0x1a), 1, Qt::CustomDashLine);
    dashes.setDashPattern({4, 4});
    painter.setPen(dashes);
    painter.drawPath(outline);

    if (!selection->isDefining()) {
        const QList<QPointF> handles = selection->handlePositions(1.0 / m_zoom);
        QList<QPointF> points;
        for (const QPointF &handle : handles) {
            points.append(toViewport.map(handle));
        }
        painter.setRenderHint(QPainter::Antialiasing, true);
        QColor line = palette().color(QPalette::Highlight);
        // The rotate handle hangs above the top edge on a short stem.
        painter.setPen(QPen(line, 1));
        painter.drawLine(points.at(1), points.last());
        painter.setBrush(Qt::white);
        const qreal half = SelectionTool::HandleRadius - 1;
        for (qsizetype i = 0; i + 1 < points.size(); ++i) {
            painter.drawRect(QRectF(points.at(i) - QPointF(half, half), QSizeF(2 * half, 2 * half)));
        }
        painter.drawEllipse(points.last(), half + 1, half + 1);
    }
    painter.restore();
}

void CanvasView::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollBars();
}

void CanvasView::scrollContentsBy(int, int)
{
    viewport()->update();
}

void CanvasView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const double steps = event->angleDelta().y() / 120.0;
        setZoom(m_zoom * std::pow(1.25, steps), event->position());
        event->accept();
        return;
    }
    QAbstractScrollArea::wheelEvent(event);
}

PointerEvent CanvasView::pointerEvent(const QMouseEvent *event, Qt::MouseButton button) const
{
    return PointerEvent{viewportToDocument(event->position()), button, event->modifiers(), 1.0 / m_zoom};
}

void CanvasView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_drawingButton == Qt::NoButton) {
        if (const int handle = canvasHandleAt(event->position()); handle >= 0) {
            m_editor->commitTool();
            m_resizeHandle = handle;
            m_resizeStart = event->position();
            m_resizeTarget = m_editor->document().rect();
            Q_EMIT canvasResizePreview(m_resizeTarget->size());
            return;
        }
    }
    if (event->button() != Qt::LeftButton && event->button() != Qt::RightButton) {
        QAbstractScrollArea::mousePressEvent(event);
        return;
    }
    if (m_drawingButton != Qt::NoButton) {
        // Pressing the other button while drawing abandons the stroke, as in Paint.
        m_editor->cancelTool();
        m_drawingButton = Qt::NoButton;
        return;
    }
    m_drawingButton = event->button();
    m_editor->currentTool()->press(pointerEvent(event, event->button()));
}

void CanvasView::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF point = viewportToDocument(event->position());
    const QPoint pixel(int(std::floor(point.x())), int(std::floor(point.y())));
    if (m_editor->document().rect().contains(pixel)) {
        Q_EMIT cursorPositionChanged(pixel);
    } else {
        Q_EMIT cursorPositionChanged(std::nullopt);
    }
    if (m_resizeHandle >= 0) {
        m_resizeTarget = canvasResizeTarget(event->position());
        Q_EMIT canvasResizePreview(m_resizeTarget->size());
        viewport()->update();
        return;
    }
    setHoverPosition(point);
    if (m_drawingButton == Qt::NoButton) {
        if (const int handle = canvasHandleAt(event->position()); handle >= 0) {
            viewport()->setCursor(cursorFor(edgesOf(handle)));
            return;
        }
        updateCursor();
    }
    if (m_drawingButton != Qt::NoButton) {
        m_editor->currentTool()->move(pointerEvent(event, Qt::NoButton));
    } else if (isSelectionTool(m_editor->settings().tool())) {
        updateSelectionCursor(point);
    }
}

void CanvasView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_resizeHandle >= 0 && event->button() == Qt::LeftButton) {
        const QRect target = canvasResizeTarget(event->position());
        m_resizeHandle = -1;
        m_resizeTarget.reset();
        Q_EMIT canvasResizePreview(std::nullopt);
        m_editor->resizeCanvas(target);
        viewport()->update();
        return;
    }
    if (event->button() != m_drawingButton) {
        QAbstractScrollArea::mouseReleaseEvent(event);
        return;
    }
    m_drawingButton = Qt::NoButton;
    m_editor->currentTool()->release(pointerEvent(event, event->button()));
}

void CanvasView::keyPressEvent(QKeyEvent *event)
{
    if (m_drawingButton == Qt::NoButton &&
        m_editor->currentTool()->keyPress(event->key(), event->modifiers())) {
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && m_resizeHandle >= 0) {
        m_resizeHandle = -1;
        m_resizeTarget.reset();
        Q_EMIT canvasResizePreview(std::nullopt);
        viewport()->update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        m_editor->cancelTool();
        m_drawingButton = Qt::NoButton;
        event->accept();
        return;
    }
    QAbstractScrollArea::keyPressEvent(event);
}

void CanvasView::setHoverPosition(std::optional<QPointF> position)
{
    const std::optional<QRectF> before = brushOutline();
    m_hover = position;
    const std::optional<QRectF> after = brushOutline();
    for (const std::optional<QRectF> &outline : {before, after}) {
        if (outline) {
            viewport()->update(outline->toAlignedRect().adjusted(-2, -2, 2, 2));
        }
    }
}

void CanvasView::leaveEvent(QEvent *event)
{
    Q_EMIT cursorPositionChanged(std::nullopt);
    setHoverPosition(std::nullopt);
    QAbstractScrollArea::leaveEvent(event);
}

void CanvasView::contextMenuEvent(QContextMenuEvent *event)
{
    Q_EMIT contextMenuRequested(event->globalPos());
}

void CanvasView::changeEvent(QEvent *event)
{
    QAbstractScrollArea::changeEvent(event);
    if (event->type() == QEvent::PaletteChange) {
        updateCursor();
    }
}

void CanvasView::updateSelectionCursor(QPointF documentPosition)
{
    const SelectionTool *selection = m_editor->selection();
    const SelectionHandle handle = selection->hitTest(documentPosition, 1.0 / m_zoom);
    switch (handle) {
    case SelectionHandle::None:
        viewport()->setCursor(Qt::CrossCursor);
        return;
    case SelectionHandle::Inside:
        viewport()->setCursor(Qt::SizeAllCursor);
        return;
    case SelectionHandle::Rotate:
        viewport()->setCursor(ui::toolCursor(QStringLiteral("rotate-180"), QPointF(10, 10)));
        return;
    default:
        break;
    }
    // Resize cursors follow the handle's direction, turned with the selection.
    const int index = int(handle) - int(SelectionHandle::TopLeft); // 0..7 clockwise from the top-left
    const qreal direction = std::fmod(135 - index * 45 + 360 - selection->angle() + 720, 180.0);
    static const Qt::CursorShape shapes[] = {Qt::SizeHorCursor, Qt::SizeBDiagCursor, Qt::SizeVerCursor,
                                             Qt::SizeFDiagCursor};
    viewport()->setCursor(shapes[int(std::round(direction / 45)) % 4]);
}

void CanvasView::dragEnterEvent(QDragEnterEvent *event)
{
    if (mimeDataHasImage(event->mimeData(), true)) {
        event->acceptProposedAction();
    }
}

void CanvasView::dragMoveEvent(QDragMoveEvent *event)
{
    if (mimeDataHasImage(event->mimeData(), true)) {
        event->acceptProposedAction();
    }
}

void CanvasView::dropEvent(QDropEvent *event)
{
    const QPointF point = viewportToDocument(event->position());
    Q_EMIT dropped(event->mimeData(), point, m_editor->document().rect().contains(point.toPoint()));
    event->acceptProposedAction();
}

void CanvasView::updateCursor()
{
    const Tool tool = m_editor->settings().tool();
    const bool outlined = (tool == Tool::Pencil || tool == Tool::Brush || tool == Tool::Eraser) &&
                          m_editor->settings().size() * m_zoom >= MinOutlineDiameter;
    switch (tool) {
    case Tool::Fill:
        viewport()->setCursor(ui::toolCursor(QStringLiteral("fill"), QPointF(16.5, 18.5)));
        break;
    case Tool::ColorPicker:
        viewport()->setCursor(ui::toolCursor(QStringLiteral("color-picker"), QPointF(2.5, 17.5)));
        break;
    case Tool::Magnifier:
        viewport()->setCursor(ui::toolCursor(QStringLiteral("magnifier"), QPointF(8, 8)));
        break;
    default:
        // The size outline stands in for the pointer, like Paint's circular brush cursor.
        viewport()->setCursor(outlined ? Qt::BlankCursor : Qt::CrossCursor);
        break;
    }
    if (m_hover) {
        setHoverPosition(m_hover);
    }
}
