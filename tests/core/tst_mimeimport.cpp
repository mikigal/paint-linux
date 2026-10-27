#include "io/MimeImport.h"

#include <QBuffer>
#include <QMimeData>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using namespace paint;

// Payloads as other applications put them on the clipboard or into drags.
class TestMimeImport : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void decodedImageData()
    {
        // Spectacle, Qt and GTK apps: Qt hands the image over decoded.
        QMimeData mime;
        mime.setImageData(sample(Qt::red));
        const auto imported = imageFromMimeData(&mime);
        QVERIFY(imported);
        QCOMPARE(imported->image.size(), QSize(6, 4));
        QCOMPARE(imported->image.format(), QImage::Format_ARGB32_Premultiplied);
        QCOMPARE(imported->image.pixelColor(1, 1), QColor(Qt::red));
        QVERIFY(mimeDataHasImage(&mime, false));
    }

    void rawPngBytes()
    {
        // Browsers' "Copy image" offers image/png bytes.
        QMimeData mime;
        mime.setData(QStringLiteral("image/png"), encoded(sample(Qt::blue), "PNG"));
        const auto imported = imageFromMimeData(&mime);
        QVERIFY(imported);
        QCOMPARE(imported->image.pixelColor(0, 0), QColor(Qt::blue));
    }

    void localFileFromFileManager()
    {
        // Dolphin copies and drags files as text/uri-list.
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("photo.png"));
        QVERIFY(sample(Qt::green).save(path));
        QMimeData mime;
        mime.setUrls(
            {QUrl::fromLocalFile(dir.filePath(QStringLiteral("notes.txt"))), QUrl::fromLocalFile(path)});
        const auto imported = imageFromMimeData(&mime);
        QVERIFY(imported);
        QCOMPARE(imported->path, path);
        QCOMPARE(imported->image.pixelColor(2, 2), QColor(Qt::green));
        QVERIFY(mimeDataHasImage(&mime, false));
    }

    void htmlWithDataUri()
    {
        const QByteArray base64 = encoded(sample(Qt::yellow), "PNG").toBase64();
        QMimeData mime;
        // (Plain string literals: moc does not parse raw strings.)
        mime.setHtml(QStringLiteral("<p>hi</p><img alt=\"x\" src=\"data:image/png;base64,%1\">")
                         .arg(QString::fromLatin1(base64)));
        const auto imported = imageFromMimeData(&mime);
        QVERIFY(imported);
        QCOMPARE(imported->image.pixelColor(0, 0), QColor(Qt::yellow));
    }

    void remoteImagesNeedADownload()
    {
        QMimeData dragged;
        dragged.setUrls({QUrl(QStringLiteral("https://example.com/page"))});
        dragged.setHtml(QStringLiteral("<img src=\"https://example.com/a.png?x=1&amp;y=2\">"));
        QVERIFY(!imageFromMimeData(&dragged));
        QCOMPARE(remoteImageUrl(&dragged), QUrl(QStringLiteral("https://example.com/a.png?x=1&y=2")));
        QVERIFY(!mimeDataHasImage(&dragged, false));
        QVERIFY(mimeDataHasImage(&dragged, true));
    }

    void unrelatedPayloadsAreRejected()
    {
        QMimeData text;
        text.setText(QStringLiteral("just words"));
        QVERIFY(!imageFromMimeData(&text));
        QVERIFY(!mimeDataHasImage(&text, true));
        QVERIFY(!imageFromMimeData(nullptr));

        QMimeData brokenPng;
        brokenPng.setData(QStringLiteral("image/png"), "not a png");
        QVERIFY(!imageFromMimeData(&brokenPng));
    }

private:
    static QImage sample(const QColor &color)
    {
        QImage image(6, 4, QImage::Format_ARGB32);
        image.fill(color);
        return image;
    }

    static QByteArray encoded(const QImage &image, const char *format)
    {
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, format);
        return bytes;
    }
};

QTEST_GUILESS_MAIN(TestMimeImport)
#include "tst_mimeimport.moc"
