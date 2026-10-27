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
    // Abandons an unfinished action and restores the document (Escape, tool switch, undo).
    virtual void cancel() {}
    // Whether an action (e.g. a stroke) is in progress.
    virtual bool isActive() const { return false; }

protected:
    ToolContext &m_context;
};

} // namespace paint
