#pragma once

#include "ops/ImageOps.h"
#include "tools/BoxGeometry.h"
#include "tools/Tool.h"

#include <QImage>
#include <QList>
#include <QPainterPath>
#include <QPolygonF>
#include <QRectF>
#include <QTransform>

#include <functional>

namespace paint {

using SelectionHandle = BoxHandle;

// Paint's rectangle and free-form selection. A selection is an outline on the active layer;
// dragging it, its handles or its rotate handle lifts the pixels into floating content that is
// drawn onto the layer live (so the document always shows what you see). The content is placed
// for good — one undo step — when the selection is committed: by clicking elsewhere, switching
// tools, Enter or Escape.
class SelectionTool : public InteractiveTool
{
public:
    explicit SelectionTool(ToolContext &context);

    void press(const PointerEvent &event) override;
    void move(const PointerEvent &event) override;
    void release(const PointerEvent &event) override;
    void cancel() override;
    void commit() override;
    bool isActive() const override { return m_floating || m_drag != Drag::None; }
    bool keyPress(const KeyEvent &event) override;

    bool hasSelection() const { return m_floating || !m_path.isEmpty(); }
    bool isFloating() const { return m_floating; }
    // Moving, resizing or rotating: Paint hides the outline meanwhile.
    bool isTransforming() const;
    // Dragging out a new selection.
    bool isDefining() const { return m_drag == Drag::Define; }
    // The selection outline in document coordinates.
    QPainterPath outline() const;
    QRect bounds() const;
    // Handle positions in document coordinates, Paint's eight resize handles then the rotate handle.
    QList<QPointF> handlePositions(qreal screenPixel) const;
    SelectionHandle hitTest(QPointF position, qreal screenPixel) const;
    // Rotation of the floating content in degrees (for cursor shapes).
    qreal angle() const { return m_angle; }

    // Places floating content but keeps the outline selected.
    void place();
    void selectAll();
    void invert();
    void deselect() { commit(); }
    // Clears the selected pixels to transparency and drops the selection.
    void deleteSelected();
    // The pixels inside the outline, transparent elsewhere: from the active layer, or flattened
    // from everything visible. Without a selection: the whole layer or image.
    QImage copy(bool visibleLayers) const;
    // Places `image` as floating content with its top-left corner at `topLeft`.
    void paste(const QImage &image, QPointF topLeft);
    // Re-renders floating content (e.g. after the transparent selection option changed).
    void refresh();
    // Applies an image operation (rotate, flip, resize, skew, invert…) to the selected pixels,
    // lifting them first; the result stays floating, centred where the selection was.
    void transformSelection(const std::function<ImageOp(const QImage &)> &operation);
    // Turns the floating content by `degrees` more (Paint's custom rotate with a selection).
    void rotateBy(qreal degrees);

private:
    enum class Drag {
        None,
        Define,
        Move,
        Resize,
        Rotate,
    };

    QRectF frame() const; // the box the handles sit on, before rotation
    QTransform contentTransform() const;
    void lift(bool keepOriginal);
    void render();
    void placeContent(bool keepSelection);
    void clearSelection();
    void notify();
    void defineTo(QPointF position, Qt::KeyboardModifiers modifiers);
    void resizeTo(QPointF position, Qt::KeyboardModifiers modifiers);
    void rotateTo(QPointF position, Qt::KeyboardModifiers modifiers);

    // Not floating: the selected area in document coordinates.
    QPainterPath m_path;
    // Floating content and where it goes.
    bool m_floating = false;
    QImage m_content;
    QPainterPath m_contentShape; // outline in content coordinates
    QRectF m_target;             // the content's (unrotated) box in document coordinates
    qreal m_angle = 0;           // degrees, around the target's centre
    qsizetype m_layer = 0;
    QImage m_before; // the layer before the content was lifted or pasted
    QImage m_base;   // the layer without the floating content
    QRect m_dirty;   // layer area touched since lifting
    QRect m_shown;   // layer area the content covers now

    // Current pointer interaction.
    Drag m_drag = Drag::None;
    SelectionHandle m_handle = SelectionHandle::None;
    QPointF m_pressPosition;
    QRectF m_startTarget;
    qreal m_startAngle = 0;
    QPolygonF m_lasso;
};

} // namespace paint
