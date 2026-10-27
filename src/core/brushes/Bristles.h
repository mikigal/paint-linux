#pragma once

#include <QColor>
#include <QList>
#include <QPointF>
#include <QRect>

class QPainter;

namespace paint {

// Paint's oil brush: a row of bristles in slightly different shades of the colour, dragged along
// the stroke so the paint shows streaks. Deterministic for a given seed.
class Bristles
{
public:
    Bristles(const QColor &color, qreal width, quint32 seed);

    // Draws the stroke from `from` to `to` (a dab when they are equal) and returns the touched area.
    QRect drawSegment(QPainter &painter, QPointF from, QPointF to);
    // The next segment starts a new stroke (no direction carried over).
    void lift() { m_hasDirection = false; }
    // The next segment starts a new stroke heading in `direction` (closed outlines start the way
    // they end).
    void lift(QPointF direction);

private:
    struct Bristle
    {
        qreal offset; // across the stroke, from its centre
        qreal width;
        QColor color;
    };

    QColor m_color;
    qreal m_width;
    QList<Bristle> m_bristles;
    QPointF m_direction{1, 0};
    bool m_hasDirection = false;
};

} // namespace paint
