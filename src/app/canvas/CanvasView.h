#pragma once

#include "tools/Tool.h"

#include <QAbstractScrollArea>
#include <QImage>
#include <QList>

#include <optional>

namespace paint {
class Editor;
}

// The workspace: the document's canvas centred on a grey background, with Paint's eight
// resize handles, zooming and scrolling. Pointer input on the canvas drives the active tool.
class CanvasView : public QAbstractScrollArea
{
    Q_OBJECT

public:
    static constexpr double MinZoom = 0.125;
    static constexpr double MaxZoom = 8.0;
    // Paint's zoom presets, ascending.
    static const QList<double> &zoomPresets();

    explicit CanvasView(paint::Editor *editor, QWidget *parent = nullptr);

    double zoom() const { return m_zoom; }
    // Zooms around `anchor` (viewport coordinates), or the viewport centre when it is null.
    void setZoom(double zoom, QPointF anchor = {});
    void zoomIn(QPointF anchor = {});
    void zoomOut(QPointF anchor = {});
    void zoomToActualSize() { setZoom(1.0); }
    void zoomToFit();

    // The canvas rectangle in viewport coordinates.
    QRectF canvasRect() const;
    QPointF viewportToDocument(QPointF point) const;
    QRectF documentToViewport(const QRectF &rect) const;

Q_SIGNALS:
    void zoomChanged(double zoom);
    // The document pixel under the pointer, or no value when the pointer is off the canvas.
    void cursorPositionChanged(std::optional<QPoint> position);
    void contextMenuRequested(QPoint globalPosition);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void handlePixelsChanged(const QRect &area);
    void handleDocumentReplaced();
    QSizeF scaledCanvasSize() const;
    void updateScrollBars();
    QList<QRectF> handleRects() const;
    paint::PointerEvent pointerEvent(const QMouseEvent *event, Qt::MouseButton button) const;
    // The brush size outline around the pointer (viewport coordinates), if the tool shows one.
    std::optional<QRectF> brushOutline() const;
    void setHoverPosition(std::optional<QPointF> position);
    void updateCursor();

    paint::Editor *m_editor;
    QImage m_composite;
    double m_zoom = 1.0;
    std::optional<QPointF> m_hover; // pointer position in document coordinates
    Qt::MouseButton m_drawingButton = Qt::NoButton;
};
