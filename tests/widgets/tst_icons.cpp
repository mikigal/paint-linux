#include "GoldenImage.h"

#include <QDir>
#include <QPainter>
#include <QSvgRenderer>
#include <QTest>

// The icon set is composed with SVG clip paths (tools/icons/build_icons.py); these tests make
// sure QtSvg renders them as intended and catch accidental changes to the generated icons.
class TestIcons : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void allIconsAreValid()
    {
        const QStringList names = iconNames();
        QVERIFY2(names.size() > 40, "icon resources are missing");
        for (const QString &name : names) {
            QSvgRenderer renderer(QStringLiteral(":/paint/icons/") + name);
            QVERIFY2(renderer.isValid(), qPrintable(name));
            QVERIFY2(renderer.defaultSize() == QSize(20, 20), qPrintable(name));
        }
    }

    void accentRegionsAreClipped()
    {
        // Sample points in the 20x20 icon grid, rendered at 10x.
        QCOMPARE(sample(QStringLiteral("eraser.svg"), QPointF(6.5, 13.3)), QColor(0xf6aeac));
        QCOMPARE(sample(QStringLiteral("pencil.svg"), QPointF(10, 10)), QColor(0xeece82));
        QCOMPARE(sample(QStringLiteral("flip.svg"), QPointF(10, 13.5)), QColor(0x005fb8));
        // The upper triangle of the flip icon is only an outline.
        QCOMPARE(sample(QStringLiteral("flip.svg"), QPointF(10, 6.5)).alpha(), 0);
    }

    void iconSheetMatchesGolden()
    {
        const QStringList names = iconNames();
        constexpr int Cell = 40;
        constexpr int Columns = 12;
        const int rows = int((names.size() + Columns - 1) / Columns);
        QImage sheet(Columns * Cell, rows * Cell, QImage::Format_ARGB32_Premultiplied);
        sheet.fill(QColor(0xf9f9f9));
        QPainter painter(&sheet);
        for (qsizetype i = 0; i < names.size(); ++i) {
            QSvgRenderer renderer(QStringLiteral(":/paint/icons/") + names.at(i));
            const QRectF cell(int(i % Columns) * Cell + 10, int(i / Columns) * Cell + 10, 20, 20);
            renderer.render(&painter, cell);
        }
        painter.end();
        PAINT_COMPARE_GOLDEN(sheet, QStringLiteral("icons/sheet-light"));
    }

private:
    static QStringList iconNames()
    {
        return QDir(QStringLiteral(":/paint/icons"))
            .entryList({QStringLiteral("*.svg")}, QDir::Files, QDir::Name);
    }

    static QColor sample(const QString &name, QPointF gridPoint)
    {
        QImage image(200, 200, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        QSvgRenderer(QStringLiteral(":/paint/icons/") + name).render(&painter);
        painter.end();
        return image.pixelColor((gridPoint * 10).toPoint());
    }
};

QTEST_MAIN(TestIcons)
#include "tst_icons.moc"
