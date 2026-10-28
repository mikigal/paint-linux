#pragma once

#include <QWidget>

#include <optional>

class CanvasView;

// Paint's rulers along the top and left of the canvas: image pixels from the picture's corner,
// following zoom and scrolling, with the pointer's position marked.
class Ruler : public QWidget
{
public:
    static constexpr int Thickness = 22;

    // `offset` is how far the viewport starts from this ruler's own start (the corner square the
    // horizontal ruler also covers).
    Ruler(Qt::Orientation orientation, CanvasView *canvas);

    void setOffset(int offset);
    // The pointer's position along this ruler in document coordinates, if it is over the canvas area.
    void setMarker(std::optional<qreal> position);

    // Image pixels between labelled ticks at `zoom`: a 1–2–5 step at least `minimumSpacing` screen
    // pixels apart.
    static int step(double zoom, double minimumSpacing);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Qt::Orientation m_orientation;
    CanvasView *m_canvas;
    int m_offset = 0;
    std::optional<qreal> m_marker;
};
