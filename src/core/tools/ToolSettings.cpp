#include "tools/ToolSettings.h"

namespace paint {

ToolSettings::ToolSettings(QObject *parent)
    : QObject(parent)
    , m_customColors(CustomColorCount)
{
    // Provisional defaults; the pencil starts at 1 px like Paint's.
    m_sizes = {{Tool::Pencil, 1}, {Tool::Brush, 5}, {Tool::Eraser, 10}, {Tool::Shape, 3}};
    m_opacities = {{Tool::Pencil, 100}, {Tool::Brush, 100}, {Tool::Eraser, 100}};
}

void ToolSettings::setTool(Tool tool)
{
    if (tool == m_tool) {
        return;
    }
    m_tool = tool;
    if (isSelectionTool(tool)) {
        m_selectionTool = tool;
    }
    Q_EMIT toolChanged(tool);
    // The sliders show the values of the newly active tool.
    Q_EMIT sizeChanged(size());
    Q_EMIT opacityChanged(opacity());
}

void ToolSettings::setBrushType(BrushType type)
{
    if (type != m_brushType) {
        m_brushType = type;
        Q_EMIT brushTypeChanged(type);
    }
}

void ToolSettings::setShapeType(ShapeType type)
{
    if (type != m_shapeType) {
        m_shapeType = type;
        Q_EMIT shapeTypeChanged(type);
    }
}

void ToolSettings::setOutlineStyle(StrokeStyle style)
{
    if (style != m_outlineStyle) {
        m_outlineStyle = style;
        Q_EMIT outlineStyleChanged(style);
    }
}

void ToolSettings::setFillStyle(StrokeStyle style)
{
    if (style != m_fillStyle) {
        m_fillStyle = style;
        Q_EMIT fillStyleChanged(style);
    }
}

void ToolSettings::setColor(ColorSlot slot, const QColor &color)
{
    QColor &target = slot == ColorSlot::Primary ? m_primary : m_secondary;
    if (color.isValid() && color != target) {
        target = color;
        Q_EMIT colorChanged(slot, color);
    }
}

void ToolSettings::setActiveColorSlot(ColorSlot slot)
{
    if (slot != m_activeSlot) {
        m_activeSlot = slot;
        Q_EMIT activeColorSlotChanged(slot);
    }
}

void ToolSettings::setCustomColor(int index, const QColor &color)
{
    Q_ASSERT(index >= 0 && index < CustomColorCount);
    if (m_customColors.at(index) != color) {
        m_customColors[index] = color;
        Q_EMIT customColorsChanged();
    }
}

void ToolSettings::addCustomColor(const QColor &color)
{
    for (QColor &slot : m_customColors) {
        if (!slot.isValid()) {
            slot = color;
            Q_EMIT customColorsChanged();
            return;
        }
    }
    m_customColors.removeFirst();
    m_customColors.append(color);
    Q_EMIT customColorsChanged();
}

int ToolSettings::sizeFor(Tool tool) const
{
    return m_sizes.value(tool, 1);
}

void ToolSettings::setSize(int size)
{
    if (!toolUsesSize(m_tool)) {
        return;
    }
    size = qBound(MinSize, size, MaxSize);
    if (size != sizeFor(m_tool)) {
        m_sizes[m_tool] = size;
        Q_EMIT sizeChanged(size);
    }
}

int ToolSettings::opacityFor(Tool tool) const
{
    return m_opacities.value(tool, 100);
}

void ToolSettings::setOpacity(int percent)
{
    if (!toolUsesOpacity(m_tool)) {
        return;
    }
    percent = qBound(1, percent, 100);
    if (percent != opacityFor(m_tool)) {
        m_opacities[m_tool] = percent;
        Q_EMIT opacityChanged(percent);
    }
}

void ToolSettings::setFillTolerance(int percent)
{
    percent = qBound(0, percent, 100);
    if (percent != m_fillTolerance) {
        m_fillTolerance = percent;
        Q_EMIT fillToleranceChanged(percent);
    }
}

void ToolSettings::setTransparentSelection(bool enabled)
{
    if (enabled != m_transparentSelection) {
        m_transparentSelection = enabled;
        Q_EMIT transparentSelectionChanged(enabled);
    }
}

} // namespace paint
