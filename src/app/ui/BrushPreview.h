#pragma once

#include "tools/ToolTypes.h"

#include <QColor>
#include <QImage>
#include <QSize>

namespace paint::ui {

// A wavy sample stroke for the brush picker, like the previews in Paint's Brushes menu.
// Placeholder rendering until the brush engine (M6) draws the real strokes.
QImage brushPreview(BrushType type, QSize size, qreal devicePixelRatio, const QColor &color);

} // namespace paint::ui
