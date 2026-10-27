#pragma once

#include "tools/BoxGeometry.h"
#include "tools/Tool.h"

#include <QAbstractScrollArea>
#include <QImage>
#include <QList>
#include <QTimer>

#include <optional>

namespace paint {
class Editor;
}

class QMimeData;

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
    // Something was dropped: `onCanvas` tells whether it landed on the image itself. The payload
    // is only valid during the signal.
    void dropped(const QMimeData *mime, QPointF documentPosition, bool onCanvas);
    // While a canvas handle is dragged: the size the canvas will get (no value when done).
    void canvasResizePreview(std::optional<QSize> size);

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void changeEvent(QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

private:
    void handlePixelsChanged(const QRect &area);
    void handleDocumentReplaced();
    void handleDocumentRestructured();
    // The selection's outline or an edited shape's frame, with their handles.
    void paintSelection(QPainter &painter);
    void paintFrame(QPainter &painter, const QPainterPath &outline, const QList<QPointF> &handles,
                    bool rotateHandle);
    QTransform documentToViewportTransform() const;
    QSizeF scaledCanvasSize() const;
    void updateScrollBars();
    QList<QRectF> handleRects() const;
    paint::PointerEvent pointerEvent(const QMouseEvent *event, Qt::MouseButton button) const;
    // The brush size outline around the pointer (viewport coordinates), if the tool shows one.
    std::optional<QRectF> brushOutline() const;
    void setHoverPosition(std::optional<QPointF> position);
    void updateCursor();
    void updateFrameCursor(paint::BoxHandle handle, qreal angle);
    bool isEditingText() const;
    // The text cursor's bounds in viewport coordinates.
    std::optional<QRectF> caretRect() const;
    void restartCaretBlink();
    void updateTextInput();
    // The canvas resize handle under a viewport point (index into handleRects()), or -1.
    int canvasHandleAt(QPointF viewportPoint) const;
    QRect canvasResizeTarget(QPointF viewportPoint) const;

    paint::Editor *m_editor;
    QImage m_composite;
    double m_zoom = 1.0;
    std::optional<QPointF> m_hover; // pointer position in document coordinates
    Qt::MouseButton m_drawingButton = Qt::NoButton;
    // Dragging a canvas handle: which one, where it started, and the canvas it would give.
    int m_resizeHandle = -1;
    QPointF m_resizeStart;
    std::optional<QRect> m_resizeTarget;
    QTimer m_caretTimer;
    bool m_caretVisible = true;
    // The airbrush keeps spraying while the pointer rests.
    QTimer m_sprayTimer;
    QPointF m_pointer; // last pointer position in document coordinates
};
