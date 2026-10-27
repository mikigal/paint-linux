#pragma once

#include <QColor>

#include <array>

namespace paint {

// Paint's two rows of preset colours (measured from Windows 11 Paint).
inline constexpr std::array<QRgb, 20> DefaultPalette{
    0x000000, 0x7f7f7f, 0x880015, 0xed1c24, 0xff7f27, 0xfff200, 0x22b14c, 0x00a2e8, 0x3f48cc, 0xa349a4,
    0xffffff, 0xc3c3c3, 0xb97a57, 0xffaec9, 0xffc90e, 0xefe4b0, 0xb5e61d, 0x99d9ea, 0x7092be, 0xc8bfe7,
};

inline constexpr int DefaultPaletteColumns = 10;

} // namespace paint
