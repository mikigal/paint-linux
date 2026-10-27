#include "ui/BrushPreview.h"

#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>

#include <cmath>
#include <numbers>

namespace paint::ui {

namespace {

QPainterPath wave(QSizeF size)
{
    QPainterPath path;
    const qreal h = size.height();
    path.moveTo(3, h * 0.6);
    path.cubicTo(size.width() * 0.3, h * 0.05, size.width() * 0.55, h * 0.95, size.width() - 3, h * 0.35);
    return path;
}

// Stamps `stamp(painter, point, angle)` along the path every `spacing` pixels.
template<typename Stamp>
void stampAlong(QPainter &painter, const QPainterPath &path, qreal spacing, Stamp stamp)
{
    const int steps = int(path.length() / spacing);
    for (int i = 0; i <= steps; ++i) {
        const qreal t = path.percentAtLength(i * spacing);
        stamp(painter, path.pointAtPercent(t), path.angleAtPercent(t));
    }
}

} // namespace

QImage brushPreview(BrushType type, QSize size, qreal devicePixelRatio, const QColor &color)
{
    QImage image(size * devicePixelRatio, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(devicePixelRatio);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPainterPath path = wave(QSizeF(size));
    QRandomGenerator random(42);
    const auto jitter = [&random](qreal amount) { return (random.generateDouble() - 0.5) * 2 * amount; };

    switch (type) {
    case BrushType::Brush:
        painter.strokePath(path, QPen(color, 4.5, Qt::SolidLine, Qt::RoundCap));
        break;
    case BrushType::CalligraphyBrush:
    case BrushType::CalligraphyPen: {
        // A flat nib at 45° (brush) or 135° (pen): width varies with the stroke direction.
        const qreal nib = type == BrushType::CalligraphyBrush ? 45 : 135;
        stampAlong(painter, path, 0.5, [&](QPainter &p, QPointF point, qreal) {
            p.save();
            p.translate(point);
            p.rotate(-nib);
            p.fillRect(QRectF(-3, -0.6, 6, 1.2), color);
            p.restore();
        });
        break;
    }
    case BrushType::Airbrush:
        stampAlong(painter, path, 1.0, [&](QPainter &p, QPointF point, qreal) {
            for (int i = 0; i < 4; ++i) {
                const qreal angle = random.generateDouble() * 2 * std::numbers::pi;
                const qreal radius = std::sqrt(random.generateDouble()) * 4.5;
                p.fillRect(QRectF(point.x() + radius * std::cos(angle), point.y() + radius * std::sin(angle),
                                  0.8, 0.8),
                           color);
            }
        });
        break;
    case BrushType::OilBrush: {
        painter.strokePath(path, QPen(color, 5, Qt::SolidLine, Qt::RoundCap));
        QColor streak = Qt::white;
        streak.setAlphaF(0.35);
        for (qreal offset : {-1.4, 0.6}) {
            painter.strokePath(path.translated(0, offset), QPen(streak, 0.6));
        }
        break;
    }
    case BrushType::Crayon:
        stampAlong(painter, path, 0.7, [&](QPainter &p, QPointF point, qreal) {
            for (int i = 0; i < 5; ++i) {
                p.fillRect(QRectF(point.x() + jitter(2.6), point.y() + jitter(2.6), 1, 1), color);
            }
        });
        break;
    case BrushType::Marker: {
        QColor translucent = color;
        translucent.setAlphaF(0.65);
        painter.strokePath(path, QPen(translucent, 6, Qt::SolidLine, Qt::FlatCap));
        break;
    }
    case BrushType::NaturalPencil:
        stampAlong(painter, path, 0.5, [&](QPainter &p, QPointF point, qreal) {
            QColor grain = color;
            grain.setAlphaF(0.35 + random.generateDouble() * 0.5);
            p.fillRect(QRectF(point.x() + jitter(0.8), point.y() + jitter(0.8), 1.2, 1.2), grain);
        });
        break;
    case BrushType::Watercolor: {
        QColor wash = color;
        wash.setAlphaF(0.3);
        painter.strokePath(path, QPen(wash, 6, Qt::SolidLine, Qt::RoundCap));
        wash.setAlphaF(0.45);
        painter.strokePath(path, QPen(wash, 2.5, Qt::SolidLine, Qt::RoundCap));
        break;
    }
    }
    return image;
}

} // namespace paint::ui
