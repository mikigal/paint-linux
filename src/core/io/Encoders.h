#pragma once

#include <QByteArray>
#include <QImage>

namespace paint {

// Windows bitmaps in the depths Paint saves: 1 (monochrome), 4 (16 colours), 8 (256 colours)
// and 24 bits per pixel, uncompressed, bottom-up, with the image's resolution.
QByteArray encodeBmp(const QImage &image, int bitsPerPixel);

// A GIF89a picture with a palette of up to 256 colours.
QByteArray encodeGif(const QImage &image);

// The picture as a palette format stores it: black and white by threshold (1 bit), the Windows
// 16-colour palette (4 bits), or up to 256 colours with dithering (8 bits). Returns Format_Mono
// or Format_Indexed8.
QImage reduceColors(const QImage &image, int bitsPerPixel);

} // namespace paint
