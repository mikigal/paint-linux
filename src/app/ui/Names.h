#pragma once

#include "tools/ToolTypes.h"

#include <QString>

// User-visible names of tools, brushes, shapes and styles, as Paint labels them.
namespace paint::ui {

QString toolName(Tool tool);
QString brushName(BrushType type);
QString shapeName(ShapeType type);
QString outlineStyleName(StrokeStyle style);
QString fillStyleName(StrokeStyle style);
// Paint's names for its preset palette colours (tool tips), in palette order.
QString paletteColorName(int index);

} // namespace paint::ui
