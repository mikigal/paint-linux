#pragma once

#include "shapes/ShapeStyles.h"
#include "tools/BoxGeometry.h"
#include "tools/Tool.h"
#include "tools/ToolTypes.h"

#include <QImage>
#include <QList>
#include <QPainterPath>
#include <QPolygonF>
#include <QRectF>

namespace paint {

// Paint's shapes. Dragging draws a shape stretched over the dragged box (Shift: a square, or a
// line in 45° steps). The shape then stays editable until it is committed: its frame moves,
// resizes and turns it, and changing the colours, size or outline and fill styles restyles it.
// Like a floating selection it is drawn onto the layer live and becomes one undo step when
// committed — by clicking outside it, switching tools or shapes, Enter or Escape.
//
// A curve starts as a line and is bent by two more drags. A polygon gets a corner with every
// click and closes when its first corner is clicked again, on a double click or with Enter.
class ShapeTool : public InteractiveTool
{
public:
    using InteractiveTool::InteractiveTool;

    void press(const PointerEvent &event) override;
    void move(const PointerEvent &event) override;
    void release(const PointerEvent &event) override;
    void cancel() override;
    void commit() override;
    bool keyPress(const KeyEvent &event) override;
    bool isActive() const override { return m_mode != Mode::Idle; }

    // The shape is drawn and shows its frame and handles.
    bool isEditing() const { return m_mode == Mode::Editing; }
    // The edited shape's frame in document coordinates; empty for lines, which only have end handles.
    QPainterPath frameOutline() const;
    // Handles in document coordinates: a line's two ends, or the eight resize handles followed
    // by the rotate handle.
    QList<QPointF> handlePositions(qreal screenPixel) const;
    bool hasRotateHandle() const { return isEditing() && m_type != ShapeType::Line; }
    // A line's ends hit as Left (start) and Right (end).
    BoxHandle hitTest(QPointF position, qreal screenPixel) const;
    // The frame's rotation in degrees; for a line, its direction (for cursor shapes).
    qreal angle() const;
    // The box the shape spans, for the status bar.
    QRectF extent() const;
    // Repaints the shape with the current colours, size and styles.
    void refresh();

private:
    enum class Mode {
        Idle,
        Drawing,
        Bending, // a curve waiting for its bends
        Polygon, // a polygon taking corners
        Editing,
    };
    enum class Drag {
        None,
        Draw,
        Bend,
        Corner,
        Move,
        Resize,
        Rotate,
        Endpoint,
    };

    void begin(const PointerEvent &event);
    void drawTo(QPointF position, Qt::KeyboardModifiers modifiers);
    void finishDrawing();
    void bendTo(QPointF position);
    void closePolygon();
    void startEditing();
    void dragTo(QPointF position, Qt::KeyboardModifiers modifiers);
    void nudge(QPointF step);

    ShapeStyle style() const;
    QRectF insetFrame(const ShapeStyle &style) const;
    QPainterPath path(const ShapeStyle &style) const;
    bool isClosed() const;
    void render();
    void finish();
    void reset();
    void notify();

    Mode m_mode = Mode::Idle;
    ShapeType m_type = ShapeType::Line;
    bool m_swapColors = false; // drawn with the right button: outline in colour 2, fill in colour 1
    qsizetype m_layer = 0;
    QImage m_before; // the layer before the shape
    QRect m_dirty;   // layer area touched by the shape
    QRect m_shown;   // layer area the shape covers now

    // Geometry. Lines and curves run from m_start to m_end; curves bend through two controls.
    QPointF m_start;
    QPointF m_end;
    QPointF m_control1;
    QPointF m_control2;
    int m_bends = 0;
    QPolygonF m_corners;
    // Shapes in a box are drawn into m_frame. Curves and polygons keep their drawn outline in
    // m_basePath and stretch it from m_baseFrame onto m_frame.
    QRectF m_frame;
    qreal m_angle = 0;
    QPainterPath m_basePath;
    QRectF m_baseFrame;

    // Current pointer interaction.
    Drag m_drag = Drag::None;
    BoxHandle m_handle = BoxHandle::None;
    QPointF m_pressPosition;
    qreal m_screenPixel = 1;
    QRectF m_startFrame;
    qreal m_startAngle = 0;
    QPointF m_startStart;
    QPointF m_startEnd;
};

} // namespace paint
