#include "io/ImageIO.h"

#include "io/Encoders.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QPainter>
#include <QSaveFile>

#include <algorithm>

namespace paint {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("paint::ImageIO", text);
}

constexpr int JpegQuality = 92;
constexpr int HeicQuality = 90;
// QImageWriter's compression value for LZW in TIFF files.
constexpr int TiffLzw = 1;
constexpr int MaxGifSide = 65535;

} // namespace

QString SaveFormat::nameFilter() const
{
    QStringList patterns;
    for (const QString &suffix : suffixes) {
        patterns.append(QStringLiteral("*.") + suffix);
    }
    return QStringLiteral("%1 (%2)").arg(description, patterns.join(QLatin1Char(' ')));
}

const QList<SaveFormat> &saveFormats()
{
    static const QList<SaveFormat> formats = [] {
        const QStringList bitmap{QStringLiteral("bmp"), QStringLiteral("dib")};
        QList<SaveFormat> list{
            {"png", tr("PNG picture"), {QStringLiteral("png")}, true},
            {"jpeg",
             tr("JPEG picture"),
             {QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("jpe"), QStringLiteral("jfif")},
             false},
            {"bmp", tr("24-bit Bitmap"), bitmap, false, 24},
            {"bmp", tr("256 Color Bitmap"), bitmap, false, 8},
            {"bmp", tr("16 Color Bitmap"), bitmap, false, 4},
            {"bmp", tr("Monochrome Bitmap"), bitmap, false, 1},
            {"gif", tr("GIF picture"), {QStringLiteral("gif")}, false, 8},
            {"tiff", tr("TIFF picture"), {QStringLiteral("tif"), QStringLiteral("tiff")}, true},
        };
        // HEIC needs an encoder (kimageformats with libheif).
        if (QImageWriter::supportedImageFormats().contains("heic")) {
            list.append({"heic", tr("HEIC picture"), {QStringLiteral("heic"), QStringLiteral("heif")}, true});
        }
        return list;
    }();
    return formats;
}

const SaveFormat &projectFormat()
{
    static const SaveFormat format{"ora", tr("OpenRaster project"), {QStringLiteral("ora")}, true};
    return format;
}

std::optional<SaveFormat> saveFormatForSuffix(const QString &suffix)
{
    const QString lower = suffix.toLower();
    if (projectFormat().suffixes.contains(lower)) {
        return projectFormat();
    }
    for (const SaveFormat &format : saveFormats()) {
        if (format.suffixes.contains(lower)) {
            return format;
        }
    }
    return std::nullopt;
}

std::optional<SaveFormat> saveFormatForPath(const QString &path)
{
    return saveFormatForSuffix(QFileInfo(path).suffix());
}

QStringList openNameFilters()
{
    QStringList patterns;
    for (const QByteArray &format : QImageReader::supportedImageFormats()) {
        patterns.append(QStringLiteral("*.") + QString::fromLatin1(format));
    }
    // Layered projects open as layers.
    patterns.append(QStringLiteral("*.ora"));
    patterns.removeDuplicates();
    std::ranges::sort(patterns);
    return {tr("Images (%1)").arg(patterns.join(QLatin1Char(' '))), tr("All files (*)")};
}

LoadedImage loadImage(const QString &path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull()) {
        return {{}, tr("This file can't be opened: %1").arg(reader.errorString())};
    }
    return {image.convertToFormat(QImage::Format_ARGB32_Premultiplied), {}};
}

bool hasTransparency(const QImage &image)
{
    if (!image.hasAlphaChannel()) {
        return false;
    }
    const QImage argb = image.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < argb.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(argb.constScanLine(y));
        if (std::any_of(line, line + argb.width(), [](QRgb pixel) { return qAlpha(pixel) != 255; })) {
            return true;
        }
    }
    return false;
}

QString saveImage(const QImage &image, const QString &path, const SaveFormat &format)
{
    QImage output = image;
    if (!format.supportsAlpha) {
        output = QImage(image.size(), QImage::Format_RGB32);
        output.setDotsPerMeterX(image.dotsPerMeterX());
        output.setDotsPerMeterY(image.dotsPerMeterY());
        output.fill(Qt::white);
        QPainter painter(&output);
        painter.drawImage(0, 0, image);
    } else {
        output = image.convertToFormat(QImage::Format_ARGB32);
    }

    // Written to a temporary file that replaces the old one only when complete.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return tr("This file can't be saved: %1").arg(file.errorString());
    }
    if (format.format == "bmp" || format.format == "gif") {
        if (format.format == "gif" && (output.width() > MaxGifSide || output.height() > MaxGifSide)) {
            file.cancelWriting();
            return tr("This picture is too large for the GIF format.");
        }
        const QByteArray data =
            format.format == "gif" ? encodeGif(output) : encodeBmp(output, format.bitsPerPixel);
        if (file.write(data) != data.size()) {
            file.cancelWriting();
            return tr("This file can't be saved: %1").arg(file.errorString());
        }
    } else {
        QImageWriter writer(&file, format.format);
        if (format.format == "jpeg") {
            writer.setQuality(JpegQuality);
        } else if (format.format == "heic") {
            writer.setQuality(HeicQuality);
        } else if (format.format == "tiff") {
            writer.setCompression(TiffLzw);
        }
        if (!writer.write(output)) {
            file.cancelWriting();
            return tr("This file can't be saved: %1").arg(writer.errorString());
        }
    }
    if (!file.commit()) {
        return tr("This file can't be saved: %1").arg(file.errorString());
    }
    return {};
}

} // namespace paint
