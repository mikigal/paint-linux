#pragma once

#include "tools/BoxGeometry.h"
#include "tools/Tool.h"
#include "tools/ToolSettings.h"

#include <QImage>
#include <QLineF>
#include <QPainterPath>
#include <QRectF>
#include <QTextCursor>

#include <memory>
#include <optional>

class QTextCharFormat;
class QTextDocument;

namespace paint {

// Paint's text tool. Dragging (or clicking) opens a text box; typing goes into it with the
// format from the format bar, which applies to the selected text or, with nothing selected, to
// what is typed next. The text is drawn in colour 1, over colour 2 with "Fill background". The
// box wraps its lines and grows to fit them; its frame moves (by the border), resizes and turns
// it. Like a shape, the text is drawn onto the layer live and becomes one undo step when
// committed — by clicking outside the box, switching tools or Escape.
class TextTool : public InteractiveTool
{
public:
    explicit TextTool(ToolContext &context);
    ~TextTool() override;

    void press(const PointerEvent &event) override;
    void move(const PointerEvent &event) override;
    void release(const PointerEvent &event) override;
    void cancel() override;
    void commit() override;
    bool keyPress(const KeyEvent &event) override;
    bool wantsKey(const KeyEvent &event) const override;
    bool undo() override;
    bool redo() override;
    bool isActive() const override { return m_mode != Mode::Idle; }

    // A text box is open for typing.
    bool isEditing() const { return m_mode == Mode::Editing; }
    // A text box is being dragged out.
    bool isDefining() const { return m_mode == Mode::Defining; }
    // The box in document coordinates.
    QPainterPath frameOutline() const;
    // The eight resize handles, then the rotate handle.
    QList<QPointF> handlePositions(qreal screenPixel) const;
    // A handle, Inside for the border (which moves the box), or None. The text area itself is
    // None; see isOverText().
    BoxHandle hitTest(QPointF position, qreal screenPixel) const;
    bool isOverText(QPointF position, qreal screenPixel) const;
    qreal angle() const { return m_angle; }
    // The text cursor in document coordinates.
    std::optional<QLineF> caret() const;
    // The box the text spans, for the status bar.
    QRectF extent() const;
    QString text() const;

    // Text from an input method; `preedit` is shown at the cursor until it is committed.
    void inputMethod(const QString &commit, const QString &preedit);
    // The format bar changed: apply what differs from the format last reported to it.
    void applyFormat(const TextFormat &format);
    void applyAlignment(Qt::Alignment alignment);
    // Repaints with the current colours and background setting.
    void refresh();

private:
    enum class Mode {
        Idle,
        Defining,
        Editing,
    };
    enum class Drag {
        None,
        Define,
        Select,
        Move,
        Resize,
        Rotate,
    };

    void open(QRectF frame);
    QTransform toDocument() const;
    QPointF toBox(QPointF documentPoint) const;
    int positionAt(QPointF documentPoint) const;
    // Re-wraps the text, grows the box to fit it, repaints and reports the format.
    void changed();
    void fitFrame();
    void render(bool showSelection = true);
    void syncFormat();
    void finish();
    void reset();
    void notify();
    void copySelection() const;
    void paste();

    Mode m_mode = Mode::Idle;
    QImage m_resolution; // fixes the layout at 96 DPI, so point sizes match Paint's on any screen
    std::unique_ptr<QTextDocument> m_document;
    QTextCursor m_cursor;
    QRectF m_frame;    // the box (unrotated), in document coordinates
    qreal m_angle = 0; // degrees, around the box's centre
    qreal m_minimumHeight = 0;
    qsizetype m_layer = 0;
    QImage m_before; // the layer before the text
    QRect m_dirty;   // layer area touched by the text
    QRect m_shown;   // layer area the text covers now
    // The format and alignment last reported to the settings.
    TextFormat m_syncedFormat;
    Qt::Alignment m_syncedAlignment = Qt::AlignLeft;

    // Current pointer interaction.
    Drag m_drag = Drag::None;
    BoxHandle m_handle = BoxHandle::None;
    QPointF m_pressPosition;
    QRectF m_startFrame;
    qreal m_startAngle = 0;
};

} // namespace paint
