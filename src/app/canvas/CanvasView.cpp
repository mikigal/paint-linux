#include "canvas/CanvasView.h"

#include "document/Document.h"
#include "ui/Icons.h"

#include <QContextMenuEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <ranges>

namespace {

// Room around the canvas for its resize handles.
constexpr int CanvasMargin = 24;
constexpr qreal HandleSize = 6;

QColor workspaceColor(const QPalette &palette)
{
    return palette.color(QPalette::Window).darker(paint::ui::isDarkPalette(palette) ? 125 : 104);
}

} // namespace

const QList<double> &CanvasView::zoomPresets()
{
    static const QList<double> presets{0.125, 0.25, 0.5, 0.75, 1, 2, 3, 4, 5, 6, 7, 8};
    return presets;
}

CanvasView::CanvasView(paint::Document *document, QWidget *parent)
    : QAbstractScrollArea(parent)
    , m_document(document)
{
    setObjectName(QStringLiteral("canvasView"));
    viewport()->setObjectName(QStringLiteral("canvasView.viewport"));
    setFrameShape(QFrame::NoFrame);
    viewport()->setMouseTracking(true);
    viewport()->setAttribute(Qt::WA_OpaquePaintEvent);
    documentChanged();
}

void CanvasView::documentChanged()
{
    m_composite = m_document->composite();
    updateScrollBars();
    viewport()->update();
}

QSizeF CanvasView::scaledCanvasSize() const
{
    return QSizeF(m_document->size()) * m_zoom;
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
    Q_EMIT zoomChanged(m_zoom);
}

void CanvasView::zoomIn()
{
    for (double preset : zoomPresets()) {
        if (preset > m_zoom + 1e-6) {
            setZoom(preset);
            return;
        }
    }
}

void CanvasView::zoomOut()
{
    for (double preset : zoomPresets() | std::views::reverse) {
        if (preset < m_zoom - 1e-6) {
            setZoom(preset);
            return;
        }
    }
}

void CanvasView::zoomToFit()
{
    const QSizeF area = QSizeF(viewport()->size()) - QSizeF(2 * CanvasMargin, 2 * CanvasMargin);
    const QSizeF document = m_document->size();
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

void CanvasView::paintEvent(QPaintEvent *event)
{
    QPainter painter(viewport());
    painter.fillRect(event->rect(), workspaceColor(palette()));

    const QRectF canvas = canvasRect();
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

void CanvasView::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF point = viewportToDocument(event->position());
    const QPoint pixel(int(std::floor(point.x())), int(std::floor(point.y())));
    if (m_document->rect().contains(pixel)) {
        Q_EMIT cursorPositionChanged(pixel);
    } else {
        Q_EMIT cursorPositionChanged(std::nullopt);
    }
    QAbstractScrollArea::mouseMoveEvent(event);
}

void CanvasView::leaveEvent(QEvent *event)
{
    Q_EMIT cursorPositionChanged(std::nullopt);
    QAbstractScrollArea::leaveEvent(event);
}

void CanvasView::contextMenuEvent(QContextMenuEvent *event)
{
    Q_EMIT contextMenuRequested(event->globalPos());
}
