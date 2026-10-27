#include "document/History.h"

#include "document/Document.h"

#include <QPainter>
#include <QUndoCommand>

namespace paint {

class PixelEditCommand : public QUndoCommand
{
public:
    PixelEditCommand(History *history, const QString &text, qsizetype layer, const QRect &area, QImage before,
                     QImage after)
        : QUndoCommand(text)
        , m_history(history)
        , m_layer(layer)
        , m_area(area)
        , m_before(std::move(before))
        , m_after(std::move(after))
    {}

    void undo() override { apply(m_before); }

    void redo() override
    {
        // The edit was already applied when it was recorded.
        if (m_firstRedo) {
            m_firstRedo = false;
            return;
        }
        apply(m_after);
    }

private:
    void apply(const QImage &pixels)
    {
        QPainter painter(&m_history->m_document->layer(m_layer).image);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(m_area.topLeft(), pixels);
        painter.end();
        Q_EMIT m_history->documentChanged(m_area);
    }

    History *m_history;
    qsizetype m_layer;
    QRect m_area;
    QImage m_before;
    QImage m_after;
    bool m_firstRedo = true;
};

class DocumentChangeCommand : public QUndoCommand
{
public:
    DocumentChangeCommand(History *history, const QString &text, Document before, Document after)
        : QUndoCommand(text)
        , m_history(history)
        , m_before(std::move(before))
        , m_after(std::move(after))
    {}

    void undo() override { apply(m_before); }

    void redo() override
    {
        if (m_firstRedo) {
            m_firstRedo = false;
            return;
        }
        apply(m_after);
    }

private:
    void apply(const Document &state)
    {
        *m_history->m_document = state;
        Q_EMIT m_history->documentRestructured();
    }

    History *m_history;
    Document m_before;
    Document m_after;
    bool m_firstRedo = true;
};

History::History(Document *document, QObject *parent)
    : QObject(parent)
    , m_document(document)
{
    m_stack.setUndoLimit(UndoLimit);
}

void History::recordPixelEdit(const QString &text, qsizetype layer, const QRect &area, const QImage &before)
{
    const QRect clipped = area.intersected(m_document->rect());
    if (clipped.isEmpty()) {
        return;
    }
    const QPoint offset = clipped.topLeft() - area.topLeft();
    m_stack.push(new PixelEditCommand(this, text, layer, clipped, before.copy(QRect(offset, clipped.size())),
                                      m_document->layer(layer).image.copy(clipped)));
}

void History::recordDocumentChange(const QString &text, const Document &before)
{
    m_stack.push(new DocumentChangeCommand(this, text, before, *m_document));
}

} // namespace paint
