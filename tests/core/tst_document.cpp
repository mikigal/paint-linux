#include "GoldenImage.h"
#include "document/Document.h"

#include <QPainter>
#include <QTest>

using paint::Document;

class TestDocument : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void newDocumentHasOneTransparentLayerOnWhite()
    {
        const Document document(QSize(40, 30));
        QCOMPARE(document.layerCount(), 1);
        QCOMPARE(document.activeLayerIndex(), 0);
        QCOMPARE(document.layer(0).image.size(), QSize(40, 30));
        QCOMPARE(document.layer(0).image.format(), Document::LayerFormat);
        QCOMPARE(document.layer(0).image.pixelColor(5, 5).alpha(), 0);
        QCOMPARE(document.composite().pixelColor(5, 5), QColor(Qt::white));
    }

    void compositeStacksVisibleLayers()
    {
        Document document(QSize(40, 30));
        paintRect(document.layer(0), QRect(0, 0, 20, 30), Qt::red);
        const qsizetype top = document.addLayer(QStringLiteral("Top"));
        paintRect(document.layer(top), QRect(10, 0, 20, 30), QColor(0, 0, 255, 128));

        QCOMPARE(document.composite().pixelColor(5, 5), QColor(Qt::red));
        QCOMPARE(document.composite().pixelColor(35, 5), QColor(Qt::white));
        PAINT_COMPARE_GOLDEN(document.composite(), QStringLiteral("document/composite-two-layers"));

        document.layer(top).visible = false;
        QCOMPARE(document.composite().pixelColor(15, 5), QColor(Qt::red));
    }

    void hiddenBackgroundIsTransparent()
    {
        Document document(QSize(10, 10), Qt::yellow);
        QCOMPARE(document.composite().pixelColor(1, 1), QColor(Qt::yellow));
        document.setBackgroundVisible(false);
        QCOMPARE(document.composite().pixelColor(1, 1).alpha(), 0);
    }

    void compositeOfAreaIsClipped()
    {
        Document document(QSize(10, 10));
        paintRect(document.layer(0), QRect(5, 5, 5, 5), Qt::green);
        const QImage part = document.composite(QRect(4, 4, 20, 20));
        QCOMPARE(part.size(), QSize(6, 6));
        QCOMPARE(part.pixelColor(0, 0), QColor(Qt::white));
        QCOMPARE(part.pixelColor(1, 1), QColor(Qt::green));
    }

private:
    static void paintRect(paint::Layer &layer, const QRect &rect, const QColor &color)
    {
        QPainter painter(&layer.image);
        painter.fillRect(rect, color);
    }
};

QTEST_GUILESS_MAIN(TestDocument)
#include "tst_document.moc"
