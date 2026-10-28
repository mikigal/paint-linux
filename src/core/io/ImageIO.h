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
    QByteArray format;    // QImageWriter format name; "bmp" and "gif" use our own encoders
    QString description;  // e.g. "PNG picture"
    QStringList suffixes; // first one is the default
    bool supportsAlpha;
    int bitsPerPixel = 0; // bitmaps and GIF: the depth colours are reduced to

    QString nameFilter() const;
    // Saving in palette formats loses colours; Paint warns first.
    bool reducesColors() const { return bitsPerPixel > 0 && bitsPerPixel <= 8; }
};

// Formats Paint offers when saving, in its order: PNG, JPEG, the four bitmap depths, GIF, then TIFF
// and HEIC where Qt can write them. Bitmaps share their suffixes; .bmp means 24-bit.
const QList<SaveFormat> &saveFormats();
// Paint's "Save as project": layered OpenRaster, not in the Save as list.
const SaveFormat &projectFormat();
// Image formats and the project format.
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
