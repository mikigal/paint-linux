#pragma once

#include <QImage>
#include <QSize>

namespace paint {

// Paint's "Set as desktop background" options.
enum class WallpaperMode {
    Fill,   // covers the screen, cropping what overflows
    Tile,   // repeats from the top-left corner
    Center, // at its own size in the middle, on black
};

// The picture laid out on a screen of `screen` pixels. Desktops take a plain image, so the
// layout is done here rather than left to their own settings.
QImage wallpaperImage(const QImage &picture, QSize screen, WallpaperMode mode);

} // namespace paint
