#include "shapes/ShapeGeometry.h"

#include <QPolygonF>
#include <QTransform>

#include <cmath>
#include <initializer_list>
#include <numbers>

namespace paint {

namespace {

// Shapes are defined in a unit square and mapped onto the requested bounds.
QPolygonF unitPolygon(std::initializer_list<QPointF> points)
{
    return QPolygonF(QList<QPointF>(points));
}

QPainterPath closedPolygon(const QPolygonF &polygon)
{
    QPainterPath path;
    path.addPolygon(polygon);
    path.closeSubpath();
    return path;
}

// A regular polygon or star with its first point at the top, fitted to the unit square.
QPolygonF radialPolygon(int points, double innerRatio)
{
    const bool star = innerRatio > 0;
    const int vertices = star ? points * 2 : points;
    QPolygonF polygon;
    for (int i = 0; i < vertices; ++i) {
        const double angle = -std::numbers::pi / 2 + i * 2 * std::numbers::pi / vertices;
        const double radius = star && (i % 2) ? innerRatio : 1.0;
        polygon.append(QPointF(radius * std::cos(angle), radius * std::sin(angle)));
    }
    const QRectF box = polygon.boundingRect();
    QTransform fit;
    fit.scale(1.0 / box.width(), 1.0 / box.height());
    fit.translate(-box.left(), -box.top());
    return fit.map(polygon);
}

QPainterPath fitToUnitSquare(const QPainterPath &path)
{
    const QRectF box = path.boundingRect();
    QTransform fit;
    fit.scale(1.0 / box.width(), 1.0 / box.height());
    fit.translate(-box.left(), -box.top());
    return fit.map(path);
}

// Boolean path operations flatten curves with an absolute tolerance, so shapes built with them
// are constructed on a 100-unit grid and scaled down afterwards to keep their curves smooth.
QPainterPath fromHundredths(const QPainterPath &path)
{
    return QTransform::fromScale(0.01, 0.01).map(path);
}

QPolygonF rightArrow()
{
    return unitPolygon({{0, 0.25}, {0.5, 0.25}, {0.5, 0}, {1, 0.5}, {0.5, 1}, {0.5, 0.75}, {0, 0.75}});
}

QPainterPath unitShape(ShapeType type)
{
    QPainterPath path;
    switch (type) {
    case ShapeType::Line:
        path.moveTo(0, 0);
        path.lineTo(1, 1);
        return path;
    case ShapeType::Curve:
        path.moveTo(0, 0.85);
        path.cubicTo(0.2, -0.3, 0.55, 1.3, 1, 0.15);
        return path;
    case ShapeType::Oval:
        path.addEllipse(QRectF(0, 0, 1, 1));
        return path;
    case ShapeType::Rectangle:
        path.addRect(QRectF(0, 0, 1, 1));
        return path;
    case ShapeType::RoundedRectangle:
        path.addRoundedRect(QRectF(0, 0, 1, 1), 0.2, 0.2);
        return path;
    case ShapeType::Polygon:
        return closedPolygon(unitPolygon({{0.3, 0}, {1, 0}, {0.8, 0.45}, {1, 1}, {0, 1}}));
    case ShapeType::Triangle:
        return closedPolygon(unitPolygon({{0.5, 0}, {1, 1}, {0, 1}}));
    case ShapeType::RightTriangle:
        return closedPolygon(unitPolygon({{0, 0}, {1, 1}, {0, 1}}));
    case ShapeType::Diamond:
        return closedPolygon(unitPolygon({{0.5, 0}, {1, 0.5}, {0.5, 1}, {0, 0.5}}));
    case ShapeType::Pentagon:
        return closedPolygon(radialPolygon(5, 0));
    case ShapeType::Hexagon:
        return closedPolygon(radialPolygon(6, 0));
    case ShapeType::RightArrow:
        return closedPolygon(rightArrow());
    case ShapeType::LeftArrow:
        return closedPolygon(QTransform(-1, 0, 0, 1, 1, 0).map(rightArrow()));
    case ShapeType::UpArrow:
        // Rotate the right arrow by -90° around the centre of the unit square.
        return closedPolygon(QTransform(0, -1, 1, 0, 0, 1).map(rightArrow()));
    case ShapeType::DownArrow:
        return closedPolygon(QTransform(0, 1, -1, 0, 1, 0).map(rightArrow()));
    case ShapeType::FourPointStar:
        return closedPolygon(radialPolygon(4, 0.3));
    case ShapeType::FivePointStar:
        return closedPolygon(radialPolygon(5, 0.382));
    case ShapeType::SixPointStar:
        return closedPolygon(radialPolygon(6, 0.577));
    case ShapeType::RoundedRectangularCallout: {
        QPainterPath body;
        body.addRoundedRect(QRectF(0, 0, 100, 75), 15, 20);
        return fromHundredths(body.united(closedPolygon(unitPolygon({{15, 70}, {35, 70}, {10, 100}}))));
    }
    case ShapeType::OvalCallout: {
        QPainterPath body;
        body.addEllipse(QRectF(0, 0, 100, 78));
        return fromHundredths(body.united(closedPolygon(unitPolygon({{18, 60}, {38, 72}, {8, 100}}))));
    }
    case ShapeType::CloudCallout: {
        QPainterPath cloud;
        struct Puff
        {
            double x, y, r;
        };
        for (const Puff &puff : {Puff{50, 42, 28}, Puff{22, 42, 20}, Puff{38, 22, 20}, Puff{62, 20, 20},
                                 Puff{80, 40, 19}, Puff{66, 58, 18}, Puff{40, 60, 18}}) {
            QPainterPath circle;
            circle.addEllipse(QPointF(puff.x, puff.y), puff.r, puff.r * 0.9);
            cloud = cloud.united(circle);
        }
        // The trail of small bubbles towards the speaker.
        cloud.addEllipse(QPointF(17, 84), 6, 5.5);
        cloud.addEllipse(QPointF(8, 96), 4, 3.5);
        return fitToUnitSquare(cloud);
    }
    case ShapeType::Heart:
        path.moveTo(0.5, 0.22);
        path.cubicTo(0.5, 0.08, 0.37, 0, 0.24, 0);
        path.cubicTo(0.09, 0, 0, 0.13, 0, 0.29);
        path.cubicTo(0, 0.56, 0.3, 0.76, 0.5, 1);
        path.cubicTo(0.7, 0.76, 1, 0.56, 1, 0.29);
        path.cubicTo(1, 0.13, 0.91, 0, 0.76, 0);
        path.cubicTo(0.63, 0, 0.5, 0.08, 0.5, 0.22);
        path.closeSubpath();
        return path;
    case ShapeType::Lightning:
        // The classic lightning bolt outline (21600-unit grid of the Office shape).
        return closedPolygon(QTransform::fromScale(1.0 / 21600, 1.0 / 21600)
                                 .map(unitPolygon({{8458, 0},
                                                   {0, 3923},
                                                   {7564, 8416},
                                                   {4993, 9720},
                                                   {12197, 13904},
                                                   {9987, 14934},
                                                   {21600, 21600},
                                                   {14768, 12911},
                                                   {16558, 12016},
                                                   {11197, 7902},
                                                   {13819, 6799}})));
    }
    Q_UNREACHABLE_RETURN(path);
}

} // namespace

QPainterPath shapePath(ShapeType type, const QRectF &bounds)
{
    QTransform toBounds;
    toBounds.translate(bounds.left(), bounds.top());
    toBounds.scale(bounds.width(), bounds.height());
    return toBounds.map(unitShape(type));
}

} // namespace paint
