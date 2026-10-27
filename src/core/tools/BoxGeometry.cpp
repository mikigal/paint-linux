#include "tools/BoxGeometry.h"

#include <QLineF>
#include <QPolygonF>

#include <cmath>
#include <numbers>

namespace paint::box {

namespace {

qreal degreesTowards(QPointF centre, QPointF point)
{
    return std::atan2(point.y() - centre.y(), point.x() - centre.x()) * 180 / std::numbers::pi;
}

} // namespace

QTransform rotationAround(QPointF centre, qreal degrees)
{
    QTransform transform;
    transform.translate(centre.x(), centre.y());
    transform.rotate(degrees);
    transform.translate(-centre.x(), -centre.y());
    return transform;
}

QList<QPointF> handles(const QRectF &frame, qreal angle, qreal screenPixel)
{
    const qreal midX = frame.center().x();
    const qreal midY = frame.center().y();
    return rotationAround(frame.center(), angle)
        .map(QPolygonF{
            frame.topLeft(),
            {midX, frame.top()},
            frame.topRight(),
            {frame.right(), midY},
            frame.bottomRight(),
            {midX, frame.bottom()},
            frame.bottomLeft(),
            {frame.left(), midY},
            {midX, frame.top() - RotateHandleOffset * screenPixel},
        });
}

BoxHandle hitTest(const QList<QPointF> &handles, const QPainterPath &inside, QPointF position,
                  qreal screenPixel)
{
    qsizetype nearest = -1;
    qreal nearestDistance = 0;
    for (qsizetype i = 0; i < handles.size(); ++i) {
        const qreal distance = QLineF(position, handles.at(i)).length();
        if (nearest < 0 || distance < nearestDistance) {
            nearest = i;
            nearestDistance = distance;
        }
    }
    const bool isInside = !inside.isEmpty() && inside.contains(position);
    const qreal reach = (isInside ? HandleRadius - 2 : HandleRadius + 2) * screenPixel;
    if (nearest >= 0 && nearestDistance <= reach) {
        return static_cast<BoxHandle>(int(BoxHandle::TopLeft) + int(nearest));
    }
    return isInside ? BoxHandle::Inside : BoxHandle::None;
}

QRectF resized(const QRectF &start, qreal angle, BoxHandle handle, QPointF position, bool keepAspect)
{
    const QPointF centre = start.center();
    const QPointF local = rotationAround(centre, angle).inverted().map(position);
    QRectF frame = start;
    QPointF anchor;
    switch (handle) {
    case BoxHandle::TopLeft:
        frame.setTopLeft(local);
        anchor = start.bottomRight();
        break;
    case BoxHandle::Top:
        frame.setTop(local.y());
        anchor = QPointF(centre.x(), start.bottom());
        break;
    case BoxHandle::TopRight:
        frame.setTopRight(local);
        anchor = start.bottomLeft();
        break;
    case BoxHandle::Right:
        frame.setRight(local.x());
        anchor = QPointF(start.left(), centre.y());
        break;
    case BoxHandle::BottomRight:
        frame.setBottomRight(local);
        anchor = start.topLeft();
        break;
    case BoxHandle::Bottom:
        frame.setBottom(local.y());
        anchor = QPointF(centre.x(), start.top());
        break;
    case BoxHandle::BottomLeft:
        frame.setBottomLeft(local);
        anchor = start.topRight();
        break;
    case BoxHandle::Left:
        frame.setLeft(local.x());
        anchor = QPointF(start.right(), centre.y());
        break;
    default:
        return start;
    }
    const bool corner = handle == BoxHandle::TopLeft || handle == BoxHandle::TopRight ||
                        handle == BoxHandle::BottomRight || handle == BoxHandle::BottomLeft;
    if (corner && keepAspect && start.width() > 0 && start.height() > 0) {
        // Scale both sides by the larger change, away from the anchor.
        const qreal scale = std::max(frame.width() / start.width(), frame.height() / start.height());
        const QSizeF size = start.size() * scale;
        frame = QRectF(QPointF(anchor.x() < frame.center().x() ? anchor.x() : anchor.x() - size.width(),
                               anchor.y() < frame.center().y() ? anchor.y() : anchor.y() - size.height()),
                       size);
    }
    frame = frame.normalized();
    frame.setWidth(std::max<qreal>(1, frame.width()));
    frame.setHeight(std::max<qreal>(1, frame.height()));
    if (angle != 0) {
        const QPointF before = rotationAround(centre, angle).map(anchor);
        const QPointF after = rotationAround(frame.center(), angle).map(anchor);
        frame.translate(before - after);
    }
    return frame;
}

qreal rotated(QPointF centre, qreal startAngle, QPointF pressPosition, QPointF position, bool snap)
{
    qreal angle = startAngle + degreesTowards(centre, position) - degreesTowards(centre, pressPosition);
    if (snap) {
        angle = std::round(angle / 15) * 15;
    }
    angle = std::remainder(angle, 360.0);
    return std::abs(angle) < 1e-9 ? 0 : angle;
}

} // namespace paint::box
