#pragma once

#include <QPointF>
#include <QRect>
#include <Qt>

namespace paint {

class Document;
class History;
class ToolSettings;

struct PointerEvent
{
    QPointF position; // document coordinates; pixel (x, y) covers [x, x+1) × [y, y+1)
    Qt::MouseButton button = Qt::NoButton;
    Qt::KeyboardModifiers modifiers;
    // Size of one screen pixel in document units (1 / zoom), for zoom-independent hit testing.
    qreal screenPixel = 1.0;
    // The press is the second click of a double click.
    bool doubleClick = false;
};

// What a tool may touch. Implemented by Editor; tests can provide their own.
class ToolContext
{
public:
    virtual ~ToolContext() = default;

    virtual Document &document() = 0;
    virtual ToolSettings &settings() = 0;
    virtual History &history() = 0;
    // The tool changed these document pixels.
    virtual void documentChanged(const QRect &area) = 0;
    // The magnifier asks the view to zoom around a document point.
    virtual void requestZoom(QPointF documentPosition, bool zoomIn) = 0;
    // The selection or an edited shape changed (outline, frame, handles or floating state).
    virtual void selectionChanged() {}
};

// An interactive tool driven by pointer events on the canvas. Drawing with the left button uses
// colour 1 and with the right button colour 2, as in Paint.
class InteractiveTool
{
public:
    explicit InteractiveTool(ToolContext &context)
        : m_context(context)
    {}
    virtual ~InteractiveTool() = default;
    InteractiveTool(const InteractiveTool &) = delete;
    InteractiveTool &operator=(const InteractiveTool &) = delete;

    virtual void press(const PointerEvent &event) = 0;
    virtual void move(const PointerEvent &) {}
    virtual void release(const PointerEvent &) {}
    // Abandons an unfinished action and restores the document (undo, a second mouse button).
    virtual void cancel() {}
    // Finishes pending work, e.g. places a moved selection (tool switch, saving).
    virtual void commit() {}
    // Keyboard input while the canvas has focus; returns whether the key was used.
    virtual bool keyPress(int key, Qt::KeyboardModifiers modifiers)
    {
        Q_UNUSED(key)
        Q_UNUSED(modifiers)
        return false;
    }
    // Whether an action (e.g. a stroke) is in progress.
    virtual bool isActive() const { return false; }

protected:
    ToolContext &m_context;
};

} // namespace paint
