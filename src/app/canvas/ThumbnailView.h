#pragma once

#include <QImage>
#include <QPointer>
#include <QTimer>
#include <QWidget>

class CanvasView;

// Paint's thumbnail: the whole picture small in the canvas's top-right corner, with the part the
// canvas shows outlined. Clicking or dragging in it moves the view there. (Paint shows it in a
// window of its own; here it is a card over the canvas, which Wayland lets us place.)
class ThumbnailView : public QWidget
{
public:
    static constexpr QSize MaximumPicture{200, 150};

    explicit ThumbnailView(CanvasView *canvas);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void rebuild();
    void reposition();
    // Where the picture is drawn in this widget.
    QRectF pictureRect() const;
    void moveViewTo(QPointF position);

    CanvasView *m_canvas;
    // Kept apart from the canvas, whose viewport may still send events while the canvas goes.
    QPointer<QWidget> m_viewport;
    QImage m_picture;
    QTimer m_rebuild;
};
