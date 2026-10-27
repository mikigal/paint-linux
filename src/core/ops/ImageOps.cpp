#include "ops/ImageOps.h"

#include <QPainter>

#include <cmath>
#include <numbers>

namespace paint {

namespace {

constexpr QImage::Format Format = QImage::Format_ARGB32_Premultiplied;

qreal radians(qreal degrees)
{
    return degrees * std::numbers::pi / 180;
}

// Draws `image` through `transform` onto a transparent canvas that exactly holds the result.
ImageOp transformed(const QImage &image, const QTransform &transform, bool smooth)
{
    const QRectF bounds = transform.mapRect(QRectF(image.rect()));
    const QSize size(std::max(1, int(std::round(bounds.width()))),
                     std::max(1, int(std::round(bounds.height()))));
    QTransform mapping = transform * QTransform::fromTranslate(-bounds.left(), -bounds.top());
    QImage result(size, Format);
    result.fill(Qt::transparent);
    result.setDotsPerMeterX(image.dotsPerMeterX());
    result.setDotsPerMeterY(image.dotsPerMeterY());
    QPainter painter(&result);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, smooth);
    painter.setRenderHint(QPainter::Antialiasing, smooth);
    painter.setTransform(mapping);
    painter.drawImage(0, 0, image);
    painter.end();
    return {result, mapping};
}

} // namespace

ImageOp rotate(const QImage &image, RotateBy rotation)
{
    // Exact pixel moves; QImage::transformed handles 90° steps without resampling.
    const qreal w = image.width();
    const qreal h = image.height();
    switch (rotation) {
    case RotateBy::Right90:
        return {image.transformed(QTransform().rotate(90)).convertToFormat(Format),
                QTransform(0, 1, -1, 0, h, 0)};
    case RotateBy::Left90:
        return {image.transformed(QTransform().rotate(-90)).convertToFormat(Format),
                QTransform(0, -1, 1, 0, 0, w)};
    case RotateBy::Half:
        return {image.flipped(Qt::Horizontal | Qt::Vertical).convertToFormat(Format),
                QTransform(-1, 0, 0, -1, w, h)};
    }
    Q_UNREACHABLE_RETURN({});
}

ImageOp rotate(const QImage &image, qreal degrees)
{
    const QPointF centre = QRectF(image.rect()).center();
    QTransform transform;
    transform.translate(centre.x(), centre.y());
    transform.rotate(degrees);
    transform.translate(-centre.x(), -centre.y());
    return transformed(image, transform, true);
}

ImageOp flip(const QImage &image, Qt::Orientation orientation)
{
    // Paint's "Flip horizontal" mirrors left to right, "Flip vertical" turns it upside down.
    if (orientation == Qt::Horizontal) {
        return {image.flipped(Qt::Horizontal), QTransform(-1, 0, 0, 1, image.width(), 0)};
    }
    return {image.flipped(Qt::Vertical), QTransform(1, 0, 0, -1, 0, image.height())};
}

ImageOp scale(const QImage &image, QSize size)
{
    size = size.expandedTo(QSize(1, 1));
    QImage result =
        image.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(Format);
    result.setDotsPerMeterX(image.dotsPerMeterX());
    result.setDotsPerMeterY(image.dotsPerMeterY());
    return {result, QTransform::fromScale(qreal(size.width()) / image.width(),
                                          qreal(size.height()) / image.height())};
}

ImageOp skew(const QImage &image, qreal horizontalDegrees, qreal verticalDegrees)
{
    if (horizontalDegrees == 0 && verticalDegrees == 0) {
        return {image, QTransform()};
    }
    QTransform shear;
    shear.shear(std::tan(radians(horizontalDegrees)), std::tan(radians(verticalDegrees)));
    return transformed(image, shear, true);
}

QSize rotatedSize(QSize size, qreal degrees)
{
    QTransform rotation;
    rotation.rotate(degrees);
    const QRectF bounds = rotation.mapRect(QRectF(QPointF(), QSizeF(size)));
    return QSize(std::max(1, int(std::round(bounds.width()))), std::max(1, int(std::round(bounds.height()))));
}

QSize skewedSize(QSize size, qreal horizontalDegrees, qreal verticalDegrees)
{
    QTransform shear;
    shear.shear(std::tan(radians(horizontalDegrees)), std::tan(radians(verticalDegrees)));
    const QRectF bounds = shear.mapRect(QRectF(QPointF(), QSizeF(size)));
    return QSize(std::max(1, int(std::round(bounds.width()))), std::max(1, int(std::round(bounds.height()))));
}

void invertColors(QImage &image, const QPainterPath &shape)
{
    Q_ASSERT(image.format() == Format);
    const QRect area =
        shape.isEmpty() ? image.rect() : shape.boundingRect().toAlignedRect().intersected(image.rect());
    for (int y = area.top(); y <= area.bottom(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = area.left(); x <= area.right(); ++x) {
            if (!shape.isEmpty() && !shape.contains(QPointF(x + 0.5, y + 0.5))) {
                continue;
            }
            // In premultiplied form inverting the colour means alpha − channel.
            const QRgb pixel = line[x];
            const int alpha = qAlpha(pixel);
            line[x] = qRgba(alpha - qRed(pixel), alpha - qGreen(pixel), alpha - qBlue(pixel), alpha);
        }
    }
}

QImage toBlackAndWhite(const QImage &image)
{
    QImage result = image.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < result.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            const QRgb pixel = line[x];
            const int value = qGray(pixel) >= 128 ? 255 : 0;
            line[x] = qRgba(value, value, value, qAlpha(pixel));
        }
    }
    return result.convertToFormat(Format);
}

} // namespace paint
