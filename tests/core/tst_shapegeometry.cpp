#include "GoldenImage.h"
#include "shapes/ShapeGeometry.h"

#include <QPainter>
#include <QTest>

using namespace paint;

class TestShapeGeometry : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void shapesFillTheirBounds()
    {
        const QRectF bounds(10, 20, 200, 100);
        for (ShapeType type : AllShapeTypes) {
            const QRectF box = shapePath(type, bounds).boundingRect();
            // Every shape spans the full width and height of the dragged rectangle.
            QVERIFY2(qAbs(box.left() - bounds.left()) < 0.5 && qAbs(box.right() - bounds.right()) < 0.5,
                     qPrintable(QString::number(int(type))));
            if (type != ShapeType::Curve) { // the curve's control points overshoot vertically
                QVERIFY2(qAbs(box.top() - bounds.top()) < 0.5 && qAbs(box.bottom() - bounds.bottom()) < 0.5,
                         qPrintable(QString::number(int(type))));
            }
        }
    }

    void closedShapesContainTheirCentre()
    {
        const QRectF bounds(0, 0, 100, 100);
        for (ShapeType type : AllShapeTypes) {
            if (!shapeIsClosed(type) || type == ShapeType::Polygon || type == ShapeType::Lightning ||
                type == ShapeType::RightTriangle) {
                continue;
            }
            QVERIFY2(shapePath(type, bounds).contains(QPointF(50, 45)),
                     qPrintable(QString::number(int(type))));
        }
    }

    void shapeSheetMatchesGolden()
    {
        constexpr int Cell = 64;
        constexpr int Columns = 7; // as in Paint's gallery
        const int rows = int((AllShapeTypes.size() + Columns - 1) / Columns);
        QImage sheet(Columns * Cell, rows * Cell, QImage::Format_ARGB32_Premultiplied);
        sheet.fill(Qt::white);
        QPainter painter(&sheet);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(Qt::black, 1.5));
        for (qsizetype i = 0; i < qsizetype(AllShapeTypes.size()); ++i) {
            const QRectF cell(int(i % Columns) * Cell + 8, int(i / Columns) * Cell + 8, Cell - 16, Cell - 16);
            painter.drawPath(shapePath(AllShapeTypes.at(i), cell));
        }
        painter.end();
        PAINT_COMPARE_GOLDEN(sheet, QStringLiteral("shapes/sheet"), {.channelTolerance = 8});
    }
};

QTEST_GUILESS_MAIN(TestShapeGeometry)
#include "tst_shapegeometry.moc"
