#pragma once

#include <QByteArray>
#include <QImage>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace paint {

struct SaveFormat
{
    QByteArray format;    // QImageWriter format name
    QString description;  // e.g. "PNG picture"
    QStringList suffixes; // first one is the default
    bool supportsAlpha;

    QString nameFilter() const;
};

// Formats Paint offers when saving, in its order. More arrive with M8 (GIF, TIFF, HEIC, BMP depths).
const QList<SaveFormat> &saveFormats();
std::optional<SaveFormat> saveFormatForSuffix(const QString &suffix);
std::optional<SaveFormat> saveFormatForPath(const QString &path);

// "Images (*.png *.jpg …)" plus one filter per readable format, for open dialogs.
QStringList openNameFilters();

struct LoadedImage
{
    QImage image; // Format_ARGB32_Premultiplied, EXIF orientation applied
    QString error;
};

LoadedImage loadImage(const QString &path);

// Writes a flattened image. Formats without transparency get transparent areas on white, as
// Paint does. Returns an error message, or an empty string on success.
QString saveImage(const QImage &image, const QString &path, const SaveFormat &format);

// Whether any pixel is not fully opaque.
bool hasTransparency(const QImage &image);

} // namespace paint
