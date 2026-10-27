#include "io/MimeImport.h"

#include "io/ImageIO.h"

#include <QFileInfo>
#include <QImageReader>
#include <QMimeData>
#include <QRegularExpression>

namespace paint {

namespace {

QImage premultiplied(const QImage &image)
{
    return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

// Image sources referenced by <img src="…"> in pasted HTML.
QStringList imageSources(const QString &html)
{
    static const QRegularExpression img(QStringLiteral(R"(<img\b[^>]*\bsrc\s*=\s*["']([^"']+)["'])"),
                                        QRegularExpression::CaseInsensitiveOption);
    QStringList sources;
    for (auto it = img.globalMatch(html); it.hasNext();) {
        // Attribute values escape '&' in URLs.
        sources.append(it.next().captured(1).replace(QStringLiteral("&amp;"), QStringLiteral("&")));
    }
    return sources;
}

QImage imageFromDataUri(const QString &uri)
{
    // data:image/png;base64,....
    static const QRegularExpression pattern(QStringLiteral(R"(^data:image/[\w.+-]+;base64,(.+)$)"),
                                            QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch match = pattern.match(uri.trimmed());
    if (!match.hasMatch()) {
        return {};
    }
    return QImage::fromData(QByteArray::fromBase64(match.captured(1).toLatin1()));
}

bool isRemote(const QUrl &url)
{
    return url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https");
}

} // namespace

std::optional<ImportedImage> imageFromMimeData(const QMimeData *mime)
{
    if (!mime) {
        return std::nullopt;
    }
    if (mime->hasImage()) {
        const QImage image = qvariant_cast<QImage>(mime->imageData());
        if (!image.isNull()) {
            return ImportedImage{premultiplied(image), {}};
        }
    }
    // Raw encoded images that Qt did not decode into imageData() (e.g. image/webp).
    for (const QString &format : mime->formats()) {
        if (format.startsWith(QLatin1String("image/"))) {
            const QImage image = QImage::fromData(mime->data(format));
            if (!image.isNull()) {
                return ImportedImage{premultiplied(image), {}};
            }
        }
    }
    for (const QUrl &url : mime->urls()) {
        if (url.isLocalFile() && QFileInfo(url.toLocalFile()).isFile()) {
            LoadedImage loaded = loadImage(url.toLocalFile());
            if (loaded.error.isEmpty()) {
                return ImportedImage{std::move(loaded.image), url.toLocalFile()};
            }
        }
    }
    if (mime->hasHtml()) {
        for (const QString &source : imageSources(mime->html())) {
            const QImage image = imageFromDataUri(source);
            if (!image.isNull()) {
                return ImportedImage{premultiplied(image), {}};
            }
        }
    }
    return std::nullopt;
}

QUrl remoteImageUrl(const QMimeData *mime)
{
    if (!mime) {
        return {};
    }
    // An <img> tag names the image itself; a plain link may be a page, so it comes second.
    if (mime->hasHtml()) {
        for (const QString &source : imageSources(mime->html())) {
            const QUrl url(source);
            if (isRemote(url)) {
                return url;
            }
        }
    }
    for (const QUrl &url : mime->urls()) {
        if (isRemote(url)) {
            return url;
        }
    }
    return {};
}

bool mimeDataHasImage(const QMimeData *mime, bool allowRemote)
{
    if (!mime) {
        return false;
    }
    if (mime->hasImage()) {
        return true;
    }
    for (const QString &format : mime->formats()) {
        if (format.startsWith(QLatin1String("image/"))) {
            return true;
        }
    }
    for (const QUrl &url : mime->urls()) {
        if (url.isLocalFile() && !QImageReader::imageFormat(url.toLocalFile()).isEmpty()) {
            return true;
        }
    }
    if (mime->hasHtml()) {
        for (const QString &source : imageSources(mime->html())) {
            if (source.startsWith(QLatin1String("data:image/"))) {
                return true;
            }
        }
    }
    return allowRemote && remoteImageUrl(mime).isValid();
}

} // namespace paint
