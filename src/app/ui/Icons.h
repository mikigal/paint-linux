#pragma once

#include "tools/ToolTypes.h"

#include <QColor>
#include <QCursor>
#include <QIcon>
#include <QPalette>

namespace paint::ui {

// Colours an icon is drawn with: the line colour and Paint's blue accent.
struct IconColors
{
    QColor foreground;
    QColor accent;
};

bool isDarkPalette(const QPalette &palette);
IconColors iconColors(const QPalette &palette);

// A Paint-style icon from resources/icons (":/paint/icons/<name>.svg"). The icon is recoloured
// for the application palette whenever it is drawn, so it follows light/dark theme switches;
// the disabled state is drawn in the palette's disabled text colour.
QIcon icon(const QString &name);

// The gallery icon of a shape, drawn from its geometry.
QIcon shapeIcon(ShapeType type);

// A mouse cursor made from a Paint icon, with a white halo so it shows on any colour.
// `hotspot` is in the icon's 20 px design grid.
QCursor toolCursor(const QString &name, QPointF hotspot);

} // namespace paint::ui
