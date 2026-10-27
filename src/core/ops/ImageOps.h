#pragma once

#include <QImage>
#include <QPainterPath>
#include <QTransform>

namespace paint {

// Paint's whole-image and selection operations as pure functions on one image. Every operation
// also has a geometry mapping (old image coordinates → new ones), so outlines can follow the
// pixels. New areas are transparent; results keep Format_ARGB32_Premultiplied.

enum class RotateBy {
    Right90,
    Left90,
    Half,
};

struct ImageOp
{
    QImage image;
    QTransform mapping; // from the old image's coordinates to the new image's
};

ImageOp rotate(const QImage &image, RotateBy rotation);
// Rotation by any angle (degrees, clockwise); the result grows to hold the whole rotated image.
ImageOp rotate(const QImage &image, qreal degrees);
ImageOp flip(const QImage &image, Qt::Orientation orientation);
ImageOp scale(const QImage &image, QSize size);
// Shear by the given angles (degrees) like Paint's skew; the result grows to hold the skewed image.
ImageOp skew(const QImage &image, qreal horizontalDegrees, qreal verticalDegrees);

// Inverts the colours (not the transparency) of the pixels inside `shape` (everywhere if empty).
void invertColors(QImage &image, const QPainterPath &shape = {});
// Paint's "Black and white": every pixel becomes black or white by its brightness.
QImage toBlackAndWhite(const QImage &image);

// The size of `size` after a rotation or skew, as the operations above compute it.
QSize rotatedSize(QSize size, qreal degrees);
QSize skewedSize(QSize size, qreal horizontalDegrees, qreal verticalDegrees);

} // namespace paint
