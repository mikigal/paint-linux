#pragma once

#include <QAbstractScrollArea>
#include <QImage>
#include <QList>

#include <optional>

namespace paint {
class Document;
}

// The workspace: the document's canvas centred on a grey background, with Paint's eight
// resize handles, zooming and scrolling.
class CanvasView : public QAbstractScrollArea
{
    Q_OBJECT

public:
    static constexpr double MinZoom = 0.125;
    static constexpr double MaxZoom = 8.0;
    // Paint's zoom presets, ascending.
    static const QList<double> &zoomPresets();

    explicit CanvasView(paint::Document *document, QWidget *parent = nullptr);

    double zoom() const { return m_zoom; }
    // Zooms around `anchor` (viewport coordinates), or the viewport centre when it is null.
    void setZoom(double zoom, QPointF anchor = {});
    void zoomIn();
    void zoomOut();
    void zoomToActualSize() { setZoom(1.0); }
    void zoomToFit();

    // The canvas rectangle in viewport coordinates.
    QRectF canvasRect() const;
    QPointF viewportToDocument(QPointF point) const;

    // Re-reads the document after it changed (size or pixels).
    void documentChanged();

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
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    QSizeF scaledCanvasSize() const;
    void updateScrollBars();
    QList<QRectF> handleRects() const;

    paint::Document *m_document;
    QImage m_composite;
    double m_zoom = 1.0;
};
