#include "brushes/Bristles.h"

#include <QLineF>
#include <QPainter>
#include <QRandomGenerator>

#include <algorithm>
#include <cmath>

namespace paint {

namespace {

// `color` moved towards white (amount > 0) or black (amount < 0).
QColor shaded(const QColor &color, qreal amount)
{
    const qreal target = amount > 0 ? 1.0 : 0.0;
    const qreal t = std::abs(amount);
    QColor result = color;
    result.setRgbF(float(color.redF() + (target - color.redF()) * t),
                   float(color.greenF() + (target - color.greenF()) * t),
                   float(color.blueF() + (target - color.blueF()) * t), color.alphaF());
    return result;
}

} // namespace

Bristles::Bristles(const QColor &color, qreal width, quint32 seed)
    : m_color(color)
    , m_width(std::max<qreal>(1, width))
{
    QRandomGenerator random(seed);
    const int count = std::clamp(int(std::lround(m_width)), 4, 48);
    const qreal spread = m_width * 0.45;
    // Neighbouring bristles overlap a little, leaving only the odd dry streak.
    const qreal bristleWidth = std::max(0.8, 2 * spread / std::max(1, count - 1) * 1.3);
    for (int i = 0; i < count; ++i) {
        const qreal position = count == 1 ? 0 : -spread + 2 * spread * i / (count - 1);
        const qreal jitter = (random.generateDouble() - 0.5) * bristleWidth * 0.6;
        const qreal shade = (random.generateDouble() - 0.4) * 0.7;
        m_bristles.append(
            {position + jitter, bristleWidth * (0.7 + 0.6 * random.generateDouble()), shaded(color, shade)});
    }
}

void Bristles::lift(QPointF direction)
{
    const qreal length = std::hypot(direction.x(), direction.y());
    m_hasDirection = length > 0.01;
    if (m_hasDirection) {
        m_direction = direction / length;
    }
}

QRect Bristles::drawSegment(QPainter &painter, QPointF from, QPointF to)
{
    const QPointF delta = to - from;
    const qreal length = std::hypot(delta.x(), delta.y());
    if (length > 0.01) {
        // The brush turns smoothly with the stroke, so mouse jitter does not scatter the bristles.
        const QPointF direction = delta / length;
        m_direction = m_hasDirection ? m_direction * 0.6 + direction * 0.4 : direction;
        const qreal norm = std::hypot(m_direction.x(), m_direction.y());
        m_direction = norm > 0.01 ? m_direction / norm : direction;
        m_hasDirection = true;
    }
    const QPointF across(-m_direction.y(), m_direction.x());
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const qreal reach = m_width / 2 + 2;
    if (!m_hasDirection) {
        // A click, or the start before the brush has moved: a round dab of paint.
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_color);
        painter.drawEllipse(to, m_width * 0.45, m_width * 0.45);
        painter.restore();
        return QRectF(to, to).adjusted(-reach, -reach, reach, reach).toAlignedRect();
    }
    for (const Bristle &bristle : std::as_const(m_bristles)) {
        painter.setPen(QPen(bristle.color, bristle.width, Qt::SolidLine, Qt::RoundCap));
        const QPointF offset = across * bristle.offset;
        painter.drawLine(from + offset, (length > 0.01 ? to : to + m_direction * 0.01) + offset);
    }
    painter.restore();
    return QRectF(from, to).normalized().adjusted(-reach, -reach, reach, reach).toAlignedRect();
}

} // namespace paint
