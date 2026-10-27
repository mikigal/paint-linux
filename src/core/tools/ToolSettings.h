#pragma once

#include "tools/ToolTypes.h"

#include <QColor>
#include <QHash>
#include <QList>
#include <QObject>

namespace paint {

// The user's current tool choices: active tool, brush and shape types, colours and the
// per-tool slider values. Shared by the ribbon, the side panel and (later) the tools.
class ToolSettings : public QObject
{
    Q_OBJECT

public:
    static constexpr int MinSize = 1;
    static constexpr int MaxSize = 100;
    static constexpr int CustomColorCount = 10;

    explicit ToolSettings(QObject *parent = nullptr);

    Tool tool() const { return m_tool; }
    void setTool(Tool tool);
    // The last selection tool used, shown on the Selection button.
    Tool selectionTool() const { return m_selectionTool; }
    // The tool that was active before the current one (the colour picker returns to it).
    Tool previousTool() const { return m_previousTool; }

    BrushType brushType() const { return m_brushType; }
    void setBrushType(BrushType type);
    ShapeType shapeType() const { return m_shapeType; }
    void setShapeType(ShapeType type);
    StrokeStyle outlineStyle() const { return m_outlineStyle; }
    void setOutlineStyle(StrokeStyle style);
    StrokeStyle fillStyle() const { return m_fillStyle; }
    void setFillStyle(StrokeStyle style);

    QColor color(ColorSlot slot) const { return slot == ColorSlot::Primary ? m_primary : m_secondary; }
    void setColor(ColorSlot slot, const QColor &color);
    ColorSlot activeColorSlot() const { return m_activeSlot; }
    void setActiveColorSlot(ColorSlot slot);

    // Empty slots hold invalid colours.
    QList<QColor> customColors() const { return m_customColors; }
    void setCustomColor(int index, const QColor &color);
    // Puts a colour into the first empty custom slot (or shifts the row when it is full).
    void addCustomColor(const QColor &color);

    // Size and opacity of the active tool; tools without a slider report their last value.
    int size() const { return sizeFor(m_tool); }
    void setSize(int size);
    int sizeFor(Tool tool) const;
    int opacity() const { return opacityFor(m_tool); }
    void setOpacity(int percent);
    int opacityFor(Tool tool) const;
    int fillTolerance() const { return m_fillTolerance; }
    void setFillTolerance(int percent);

    bool transparentSelection() const { return m_transparentSelection; }
    void setTransparentSelection(bool enabled);

Q_SIGNALS:
    void toolChanged(paint::Tool tool);
    void brushTypeChanged(paint::BrushType type);
    void shapeTypeChanged(paint::ShapeType type);
    void outlineStyleChanged(paint::StrokeStyle style);
    void fillStyleChanged(paint::StrokeStyle style);
    void colorChanged(paint::ColorSlot slot, const QColor &color);
    void activeColorSlotChanged(paint::ColorSlot slot);
    void customColorsChanged();
    void sizeChanged(int size);
    void opacityChanged(int percent);
    void fillToleranceChanged(int percent);
    void transparentSelectionChanged(bool enabled);

private:
    Tool m_tool = Tool::Pencil;
    Tool m_selectionTool = Tool::RectangleSelect;
    Tool m_previousTool = Tool::Pencil;
    BrushType m_brushType = BrushType::Brush;
    ShapeType m_shapeType = ShapeType::Line;
    StrokeStyle m_outlineStyle = StrokeStyle::Solid;
    StrokeStyle m_fillStyle = StrokeStyle::None;
    QColor m_primary = Qt::black;
    QColor m_secondary = Qt::white;
    ColorSlot m_activeSlot = ColorSlot::Primary;
    QList<QColor> m_customColors;
    QHash<Tool, int> m_sizes;
    QHash<Tool, int> m_opacities;
    int m_fillTolerance = 0;
    bool m_transparentSelection = false;
};

} // namespace paint
