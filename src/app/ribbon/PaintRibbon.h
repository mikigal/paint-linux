#pragma once

class ActionRegistry;
class Ribbon;
class QWidget;

namespace paint {
class ToolSettings;
}

// Builds Paint's ribbon: Quick access, Selection, Image, Tools, Brushes, Shapes, Colors, Layers.
Ribbon *createPaintRibbon(ActionRegistry *actions, paint::ToolSettings *settings, QWidget *parent);
