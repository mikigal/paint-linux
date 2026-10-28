#include "editor/Editor.h"
#include "io/ImageIO.h"
#include "tools/ToolSettings.h"

#include <QFile>
#include <QImageReader>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace paint;

class TestImageIO : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        QVERIFY(m_dir.isValid());
        m_settings = std::make_unique<ToolSettings>();
        m_editor = std::make_unique<Editor>(m_settings.get());
    }

    void formatsFollowPaintsOrder()
    {
        QCOMPARE(saveFormats().first().format, QByteArray("png"));
        QCOMPARE(saveFormatForPath(QStringLiteral("/x/photo.JPG"))->format, QByteArray("jpeg"));
        QVERIFY(!saveFormatForPath(QStringLiteral("/x/notes.txt")));
        QVERIFY(openNameFilters().first().contains(QLatin1String("*.png")));
    }

    void pngRoundTripKeepsTransparency()
    {
        m_editor->newDocument(QSize(10, 8));
        Document &document = m_editor->document();
        document.setBackgroundVisible(false);
        document.layer(0).image.setPixelColor(3, 4, QColor(255, 0, 0, 128));
        const QString path = m_dir.filePath(QStringLiteral("a.png"));
        QCOMPARE(m_editor->save(path, *saveFormatForPath(path)), QString());
        QVERIFY(!m_editor->isModified());
        QVERIFY(m_editor->fileSize() > 0);

        Editor reopened(m_settings.get());
        QSignalSpy replaced(&reopened, &Editor::documentReplaced);
        QCOMPARE(reopened.open(path), QString());
        QCOMPARE(replaced.count(), 1);
        QCOMPARE(reopened.document().size(), QSize(10, 8));
        QVERIFY2(!reopened.document().isBackgroundVisible(), "transparent images open without a background");
        QCOMPARE(reopened.document().layer(0).image.pixelColor(3, 4), QColor(255, 0, 0, 128));
        QCOMPARE(reopened.filePath(), path);
    }

    void opaqueImagesKeepTheBackground()
    {
        m_editor->newDocument(QSize(4, 4));
        const QString path = m_dir.filePath(QStringLiteral("opaque.png"));
        QCOMPARE(m_editor->save(path, *saveFormatForPath(path)), QString());
        QCOMPARE(m_editor->open(path), QString());
        QVERIFY(m_editor->document().isBackgroundVisible());
    }

    void jpegAndBmpFlattenOnWhite()
    {
        for (const QString &name : {QStringLiteral("b.jpg"), QStringLiteral("c.bmp")}) {
            m_editor->newDocument(QSize(16, 16));
            m_editor->document().setBackgroundVisible(false);
            m_editor->document().layer(0).image.fill(Qt::transparent);
            const QString path = m_dir.filePath(name);
            QCOMPARE(m_editor->save(path, *saveFormatForPath(path)), QString());
            const QImage saved(path);
            QVERIFY(!saved.hasAlphaChannel());
            QVERIFY2(saved.pixelColor(8, 8).lightness() > 250, qPrintable(name));
        }
    }

    void everyFormatSavesAndReadsBack()
    {
        const QImage picture = testPicture(QSize(37, 23));
        for (const SaveFormat &format : saveFormats()) {
            const QString path = m_dir.filePath(QStringLiteral("all.") + format.suffixes.first());
            QFile::remove(path);
            QCOMPARE(saveImage(picture, path, format), QString());
            QImageReader reader(path);
            const QImage read = reader.read();
            QVERIFY2(!read.isNull(),
                     qPrintable(format.description + QLatin1Char(' ') + reader.errorString()));
            QCOMPARE(read.size(), picture.size());
        }
    }

    void bitmapDepthsReduceColoursLikePaint()
    {
        const QImage picture = testPicture(QSize(40, 30));
        const auto saveAs = [&](int bits) {
            for (const SaveFormat &format : saveFormats()) {
                if (format.format == "bmp" && format.bitsPerPixel == bits) {
                    const QString path = m_dir.filePath(QStringLiteral("depth%1.bmp").arg(bits));
                    [&] { QCOMPARE(saveImage(picture, path, format), QString()); }();
                    return QImage(path).convertToFormat(QImage::Format_RGB32);
                }
            }
            return QImage();
        };
        // 24-bit is exact, with the resolution.
        const QImage full = saveAs(24);
        QCOMPARE(full, picture.convertToFormat(QImage::Format_RGB32));
        QCOMPARE(QImage(m_dir.filePath(QStringLiteral("depth24.bmp"))).dotsPerMeterX(), 11811);
        // 16 colours come from Windows' palette.
        const QImage sixteen = saveAs(4);
        QSet<QRgb> used;
        for (int y = 0; y < sixteen.height(); ++y) {
            for (int x = 0; x < sixteen.width(); ++x) {
                used.insert(sixteen.pixel(x, y) & RGB_MASK);
            }
        }
        QVERIFY(used.size() <= 16);
        QVERIFY(used.contains(qRgb(255, 0, 0) & RGB_MASK));
        // Monochrome splits at half brightness.
        const QImage mono = saveAs(1);
        QCOMPARE(mono.pixel(0, 0) & RGB_MASK, qRgb(0, 0, 0) & RGB_MASK);         // dark corner
        QCOMPARE(mono.pixel(39, 29) & RGB_MASK, qRgb(255, 255, 255) & RGB_MASK); // light corner
        QVERIFY(saveAs(8).width() == 40);
        QCOMPARE(saveFormatForSuffix(QStringLiteral("bmp"))->bitsPerPixel, 24);
        QVERIFY(saveFormatForSuffix(QStringLiteral("gif"))->reducesColors());
        QVERIFY(!saveFormatForSuffix(QStringLiteral("png"))->reducesColors());
    }

    void gifKeepsUpToTwoHundredFiftySixColoursExactly()
    {
        // Enough pixels and colours to fill the LZW table and start it over several times.
        QImage picture(300, 200, QImage::Format_RGB32);
        quint32 seed = 7;
        for (int y = 0; y < picture.height(); ++y) {
            for (int x = 0; x < picture.width(); ++x) {
                seed = seed * 1664525u + 1013904223u;
                const int index = int(seed >> 24) % 200;
                picture.setPixel(x, y, qRgb(index, 255 - index, (index * 7) % 256));
            }
        }
        const QString path = m_dir.filePath(QStringLiteral("noise.gif"));
        QCOMPARE(saveImage(picture, path, *saveFormatForSuffix(QStringLiteral("gif"))), QString());
        QCOMPARE(QImage(path).convertToFormat(QImage::Format_RGB32), picture);

        // A plain image, a single pixel and a two-colour one decode as well.
        for (const QSize size : {QSize(1, 1), QSize(64, 64), QSize(3, 900)}) {
            QImage plain(size, QImage::Format_RGB32);
            plain.fill(qRgb(10, 200, 30));
            if (size.width() > 1) {
                plain.setPixel(0, 0, qRgb(255, 255, 255));
            }
            QCOMPARE(saveImage(plain, path, *saveFormatForSuffix(QStringLiteral("gif"))), QString());
            QCOMPARE(QImage(path).convertToFormat(QImage::Format_RGB32), plain);
        }
    }

    void brokenFilesReportAnError()
    {
        const QString path = m_dir.filePath(QStringLiteral("broken.png"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("definitely not a PNG");
        file.close();
        m_editor->newDocument(QSize(5, 5));
        const QString error = m_editor->open(path);
        QVERIFY(error.contains(QLatin1String("can't be opened")));
        QCOMPARE(m_editor->document().size(), QSize(5, 5)); // the current image is untouched
    }

    void savingToAnUnwritablePlaceFails()
    {
        const QString error = m_editor->save(QStringLiteral("/proc/nope/x.png"), saveFormats().first());
        QVERIFY(!error.isEmpty());
        QVERIFY(m_editor->filePath().isEmpty());
    }

private:
    // A gradient from dark (top-left) to light (bottom-right) with a red block, at 300 DPI.
    static QImage testPicture(QSize size)
    {
        QImage image(size, QImage::Format_ARGB32);
        for (int y = 0; y < size.height(); ++y) {
            for (int x = 0; x < size.width(); ++x) {
                const int value = 255 * (x + y) / (size.width() + size.height() - 2);
                image.setPixel(x, y, qRgb(value, value, value));
            }
        }
        for (int y = 2; y < 8; ++y) {
            for (int x = 10; x < 18; ++x) {
                image.setPixel(x, y, qRgb(255, 0, 0));
            }
        }
        image.setDotsPerMeterX(11811);
        image.setDotsPerMeterY(11811);
        return image;
    }

    QTemporaryDir m_dir;
    std::unique_ptr<ToolSettings> m_settings;
    std::unique_ptr<Editor> m_editor;
};

QTEST_GUILESS_MAIN(TestImageIO)
#include "tst_imageio.moc"
