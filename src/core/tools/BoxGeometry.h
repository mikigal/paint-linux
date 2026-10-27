#pragma once

#include <QList>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QTransform>

namespace paint {

// Where the pointer is relative to a transform frame (a selection or an editable shape).
enum class BoxHandle {
    None,
    Inside,
    TopLeft,
    Top,
    TopRight,
    Right,
    BottomRight,
    Bottom,
    BottomLeft,
    Left,
    Rotate,
};

// Geometry of Paint's transform frames: a box rotated around its centre with eight resize
// handles and a rotate handle above the top edge. Shared by selections and shapes.
namespace box {

// How far above the top edge the rotate handle sits, in screen pixels.
inline constexpr qreal RotateHandleOffset = 24;
inline constexpr qreal HandleRadius = 5;

QTransform rotationAround(QPointF centre, qreal degrees);

// The eight resize handles clockwise from the top-left, then the rotate handle.
QList<QPointF> handles(const QRectF &frame, qreal angle, qreal screenPixel);

// The handle (by its index in `handles`, mapped from TopLeft on) nearest to `position`, or
// Inside / None. Inside `inside`, handles only win when the pointer is right on them, so small
// frames can still be dragged by their middle.
BoxHandle hitTest(const QList<QPointF> &handles, const QPainterPath &inside, QPointF position,
                  qreal screenPixel);

// The frame after dragging `handle` to `position` (document coordinates), working in the frame's
// own rotated axes and keeping the opposite handle in place. Corners keep the proportions with
// `keepAspect`.
QRectF resized(const QRectF &start, qreal angle, BoxHandle handle, QPointF position, bool keepAspect);

// The angle after dragging the rotate handle from `pressPosition` to `position`; `snap` rounds to 15°.
qreal rotated(QPointF centre, qreal startAngle, QPointF pressPosition, QPointF position, bool snap);

} // namespace box
} // namespace paint
