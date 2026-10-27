#include "GoldenImage.h"

#include <QTemporaryDir>
#include <QTest>

using namespace paint::test;

class TestGoldenImage : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        qunsetenv("PAINT_UPDATE_GOLDEN");
        QVERIFY(m_dir.isValid());
        m_options.goldenDir = m_dir.filePath(QStringLiteral("golden"));
        m_options.outputDir = m_dir.filePath(QStringLiteral("output"));
    }

    void missingGoldenFails()
    {
        const GoldenResult result = compareWithGolden(solid(Qt::red), QStringLiteral("missing"), m_options);
        QVERIFY(!result.passed);
        QVERIFY(result.message.contains(QLatin1String("PAINT_UPDATE_GOLDEN")));
        QVERIFY(QFile::exists(m_dir.filePath(QStringLiteral("output/missing.actual.png"))));
    }

    void updateThenMatch()
    {
        qputenv("PAINT_UPDATE_GOLDEN", "1");
        QVERIFY(compareWithGolden(solid(Qt::blue), QStringLiteral("blue"), m_options).passed);
        qunsetenv("PAINT_UPDATE_GOLDEN");
        QVERIFY(compareWithGolden(solid(Qt::blue), QStringLiteral("blue"), m_options).passed);
    }

    void mismatchWritesDiff()
    {
        qputenv("PAINT_UPDATE_GOLDEN", "1");
        QVERIFY(compareWithGolden(solid(Qt::white), QStringLiteral("white"), m_options).passed);
        qunsetenv("PAINT_UPDATE_GOLDEN");

        QImage changed = solid(Qt::white);
        changed.setPixelColor(3, 4, Qt::black);
        const GoldenResult result = compareWithGolden(changed, QStringLiteral("white"), m_options);
        QVERIFY(!result.passed);
        QVERIFY(result.message.contains(QLatin1String("1 of 100 pixels differ")));

        const QImage diff(m_dir.filePath(QStringLiteral("output/white.diff.png")));
        QCOMPARE(diff.pixelColor(3, 4), QColor(Qt::red));
        QVERIFY(diff.pixelColor(0, 0) != QColor(Qt::red));
    }

    void toleranceAndRatio()
    {
        QImage a = solid(QColor(100, 100, 100));
        QImage b = solid(QColor(103, 100, 100));
        QCOMPARE(countDifferentPixels(a, b, 2), qsizetype(100));
        QCOMPARE(countDifferentPixels(a, b, 3), qsizetype(0));

        qputenv("PAINT_UPDATE_GOLDEN", "1");
        QVERIFY(compareWithGolden(a, QStringLiteral("grey"), m_options).passed);
        qunsetenv("PAINT_UPDATE_GOLDEN");
        b = a;
        b.setPixelColor(0, 0, Qt::black);
        GoldenOptions lenient = m_options;
        lenient.allowedMismatchRatio = 0.01;
        QVERIFY(compareWithGolden(b, QStringLiteral("grey"), lenient).passed);
    }

private:
    static QImage solid(const QColor &color)
    {
        QImage image(10, 10, QImage::Format_ARGB32);
        image.fill(color);
        return image;
    }

    QTemporaryDir m_dir;
    GoldenOptions m_options;
};

QTEST_GUILESS_MAIN(TestGoldenImage)
#include "tst_goldenimage.moc"
