#include "editor/Editor.h"
#include "io/ImageIO.h"
#include "tools/ToolSettings.h"

#include <QFile>
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
    QTemporaryDir m_dir;
    std::unique_ptr<ToolSettings> m_settings;
    std::unique_ptr<Editor> m_editor;
};

QTEST_GUILESS_MAIN(TestImageIO)
#include "tst_imageio.moc"
