#pragma once

#include "brushes/Bristles.h"
#include "tools/Tool.h"
#include "tools/ToolTypes.h"

#include <QColor>
#include <QImage>
#include <QRandomGenerator>
#include <QString>

#include <optional>

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
    void commit() override;
    bool isActive() const override { return m_button != Qt::NoButton; }

protected:
    // A stroke starts at `point`; m_color and m_size are set.
    virtual void beginStroke(QPointF point) { Q_UNUSED(point) }
    // Draws the segment from `from` to `to` (a dot when they are equal) into the stroke buffer
    // and returns the touched area.
    virtual QRect drawSegment(QPainter &painter, QPointF from, QPointF to) = 0;
    // Draws the stroke buffer's `area` onto the layer; textured brushes colour it here.
    virtual void drawStroke(QPainter &painter, const QRect &area) const;
    const QImage &strokeBuffer() const { return m_stroke; }
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

// Paint's nine brushes. The plain brush, crayon, marker, natural pencil and watercolour lay down a
// round stroke that the textured ones colour with paper grain fixed to the document; the
// calligraphy brush and pen drag a slanted nib; the airbrush sprays dots; the oil brush drags
// bristles. Random parts are seeded from where the stroke starts, so strokes are reproducible.
class BrushTool : public StrokeTool
{
public:
    using StrokeTool::StrokeTool;

protected:
    void beginStroke(QPointF point) override;
    QRect drawSegment(QPainter &painter, QPointF from, QPointF to) override;
    void drawStroke(QPainter &painter, const QRect &area) const override;
    QString actionName() const override;

private:
    QRect drawNib(QPainter &painter, QPointF from, QPointF to, qreal degrees) const;
    QRect spray(QPainter &painter, QPointF from, QPointF to);
    // The style the stroke buffer is coloured in; Solid when it already holds the final colours.
    StrokeStyle texture() const;

    BrushType m_type = BrushType::Brush;
    QRandomGenerator m_random;
    std::optional<Bristles> m_bristles;
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
