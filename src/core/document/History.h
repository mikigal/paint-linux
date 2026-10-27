#pragma once

#include <QImage>
#include <QObject>
#include <QRect>
#include <QUndoStack>

namespace paint {

class Document;

// The undo history of the open document. Pixel edits are stored as before/after copies of the
// changed rectangle of one layer, so memory grows with what was edited, not with the image size.
class History : public QObject
{
    Q_OBJECT

public:
    // Paint keeps a long but finite history.
    static constexpr int UndoLimit = 100;

    History(Document *document, QObject *parent = nullptr);

    QUndoStack *stack() { return &m_stack; }
    bool canUndo() const { return m_stack.canUndo(); }
    bool canRedo() const { return m_stack.canRedo(); }
    void undo() { m_stack.undo(); }
    void redo() { m_stack.redo(); }
    bool isClean() const { return m_stack.isClean(); }
    void setClean() { m_stack.setClean(); }
    void clear() { m_stack.clear(); }

    // Records an edit that has already been applied to `layer`: `before` holds the layer's
    // pixels in `area` as they were before the edit.
    void recordPixelEdit(const QString &text, qsizetype layer, const QRect &area, const QImage &before);

Q_SIGNALS:
    // Pixels in `area` changed because of undo or redo.
    void documentChanged(const QRect &area);

private:
    friend class PixelEditCommand;

    Document *m_document;
    QUndoStack m_stack;
};

} // namespace paint
