#pragma once

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QRect>

namespace paint {

// Fills the region of similar colour connected to `seed` (4-connected) with `color`.
// `tolerance` is Paint's 0–100 % slider: a pixel joins the region when none of its channels
// (alpha included) differs from the seed pixel by more than tolerance × 255 / 100.
// Returns the bounding rectangle of the changed pixels (empty if nothing changed).
// `image` must be Format_ARGB32_Premultiplied.
QRect floodFill(QImage &image, QPoint seed, const QColor &color, int tolerance = 0);

} // namespace paint
