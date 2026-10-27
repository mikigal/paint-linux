#include "document/Document.h"
#include "ops/FloodFill.h"

#include <QElapsedTimer>
#include <QPainter>
#include <QTest>

using namespace paint;

class TestFloodFill : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fillsEnclosedRegionOnly()
    {
        QImage image = canvas(20, 20, Qt::white);
        QPainter painter(&image);
        painter.setPen(Qt::black);
        painter.drawRect(5, 5, 10, 10); // pixels 5..15
        painter.end();

        const QRect changed = floodFill(image, QPoint(10, 10), Qt::red);
        QCOMPARE(changed, QRect(QPoint(6, 6), QPoint(14, 14)));
        QCOMPARE(image.pixelColor(10, 10), QColor(Qt::red));
        QCOMPARE(image.pixelColor(5, 10), QColor(Qt::black));
        QCOMPARE(image.pixelColor(2, 2), QColor(Qt::white));
    }

    void fillsAroundObstaclesWithFourConnectivity()
    {
        QImage image = canvas(10, 10, Qt::white);
        // A diagonal line does not separate regions for a 4-connected fill... it does: pixels on
        // either side only touch diagonally.
        for (int i = 0; i < 10; ++i) {
            image.setPixelColor(i, i, Qt::black);
        }
        floodFill(image, QPoint(9, 0), Qt::blue);
        QCOMPARE(image.pixelColor(9, 0), QColor(Qt::blue));
        QCOMPARE(image.pixelColor(0, 9), QColor(Qt::white));
    }

    void toleranceGrowsTheRegion()
    {
        QImage image = canvas(3, 1, QColor(100, 100, 100));
        image.setPixelColor(1, 0, QColor(110, 100, 100));
        image.setPixelColor(2, 0, QColor(140, 100, 100));

        QImage exact = image;
        floodFill(exact, QPoint(0, 0), Qt::green, 0);
        QCOMPARE(exact.pixelColor(1, 0), QColor(110, 100, 100));

        QImage loose = image;
        floodFill(loose, QPoint(0, 0), Qt::green, 5); // 5 % ≈ 12 levels
        QCOMPARE(loose.pixelColor(1, 0), QColor(Qt::green));
        QCOMPARE(loose.pixelColor(2, 0), QColor(140, 100, 100));
    }

    void fillsTransparentAreaOfALayer()
    {
        Document document(QSize(8, 8));
        QImage &layer = document.layer(0).image;
        layer.setPixelColor(4, 0, Qt::black);
        layer.setPixelColor(4, 1, Qt::black);
        floodFill(layer, QPoint(0, 0), Qt::yellow);
        QCOMPARE(layer.pixelColor(0, 7), QColor(Qt::yellow));
        QCOMPARE(layer.pixelColor(4, 0), QColor(Qt::black));
    }

    void sameColourIsNoOp()
    {
        QImage image = canvas(5, 5, Qt::red);
        QVERIFY(floodFill(image, QPoint(2, 2), Qt::red).isEmpty());
        QVERIFY(floodFill(image, QPoint(-1, 2), Qt::blue).isEmpty());
    }

    void semiTransparentFillIsPremultiplied()
    {
        QImage image = canvas(2, 2, Qt::transparent);
        floodFill(image, QPoint(0, 0), QColor(255, 0, 0, 128));
        QCOMPARE(image.pixelColor(1, 1), QColor(255, 0, 0, 128));
    }

    void largeImagesFillQuickly()
    {
        QImage image = canvas(4000, 3000, Qt::white);
        QElapsedTimer timer;
        timer.start();
        floodFill(image, QPoint(0, 0), Qt::black);
        const qint64 elapsed = timer.elapsed();
        QCOMPARE(image.pixelColor(3999, 2999), QColor(Qt::black));
        qInfo("4000x3000 fill: %lld ms", elapsed);
        QVERIFY2(elapsed < 2000, "flood fill of a 12 MP image must stay interactive");
    }

private:
    static QImage canvas(int width, int height, const QColor &color)
    {
        QImage image(width, height, Document::LayerFormat);
        image.fill(color);
        return image;
    }
};

QTEST_GUILESS_MAIN(TestFloodFill)
#include "tst_floodfill.moc"
