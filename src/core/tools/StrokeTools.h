#pragma once

#include "tools/Tool.h"

#include <QColor>
#include <QImage>
#include <QString>

class QPainter;

namespace paint {

// Freehand tools. A stroke is painted at full strength into a buffer and composited onto the
// layer with the stroke's opacity, so overlapping parts of one stroke do not build up — the
// way Paint's opacity slider behaves.
class StrokeTool : public InteractiveTool
{
public:
    using InteractiveTool::InteractiveTool;

    void press(const PointerEvent &event) override;
    void move(const PointerEvent &event) override;
    void release(const PointerEvent &event) override;
    void cancel() override;
    bool isActive() const override { return m_button != Qt::NoButton; }

protected:
    // Draws the segment from `from` to `to` (a dot when they are equal) into the stroke buffer
    // and returns the touched area.
    virtual QRect drawSegment(QPainter &painter, QPointF from, QPointF to) = 0;
    // Erasing tools remove the stroke's coverage from the layer instead of painting over it.
    virtual bool erases() const { return false; }
    virtual QString actionName() const = 0;

    QColor m_color;
    int m_size = 1;

private:
    void addSegment(QPointF from, QPointF to);
    void composite(const QRect &area);
    void finish();

    Qt::MouseButton m_button = Qt::NoButton;
    qsizetype m_layer = 0;
    qreal m_opacity = 1.0;
    QPointF m_last;
    QImage m_before; // the layer when the stroke started
    QImage m_stroke; // the stroke at full strength
    QRect m_changed;
};

// Hard-edged freehand line; 1 px by default.
class PencilTool : public StrokeTool
{
public:
    using StrokeTool::StrokeTool;

protected:
    QRect drawSegment(QPainter &painter, QPointF from, QPointF to) override;
    QString actionName() const override;
};

// Smooth round brush. The other brush types get their own engines in M6 and use this until then.
class BrushTool : public StrokeTool
{
public:
    using StrokeTool::StrokeTool;

protected:
    QRect drawSegment(QPainter &painter, QPointF from, QPointF to) override;
    QString actionName() const override;
};

// Square eraser that clears pixels to transparency; its opacity is Paint's eraser transparency.
class EraserTool : public StrokeTool
{
public:
    using StrokeTool::StrokeTool;

protected:
    QRect drawSegment(QPainter &painter, QPointF from, QPointF to) override;
    bool erases() const override { return true; }
    QString actionName() const override;
};

} // namespace paint
