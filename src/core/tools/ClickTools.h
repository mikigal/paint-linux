#pragma once

#include "tools/Tool.h"

namespace paint {

// Fills the clicked area of the active layer, with the fill tolerance from the side panel.
class FillTool : public InteractiveTool
{
public:
    using InteractiveTool::InteractiveTool;
    void press(const PointerEvent &event) override;
};

// Picks a colour from the visible image into colour 1 (left) or colour 2 (right), then returns
// to the previously used tool, as Paint does.
class ColorPickerTool : public InteractiveTool
{
public:
    using InteractiveTool::InteractiveTool;
    void press(const PointerEvent &event) override;
};

// Zooms in (left) or out (right) around the clicked point.
class MagnifierTool : public InteractiveTool
{
public:
    using InteractiveTool::InteractiveTool;
    void press(const PointerEvent &event) override;
};

} // namespace paint
