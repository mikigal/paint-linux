#include "document/History.h"

#include "document/Document.h"

#include <QPainter>
#include <QSet>

namespace paint {

namespace {

// Memory a structural change holds: the layer images that differ between the two states. Layers
// it left alone are shared with the document and the neighbouring steps.
qint64 changedBytes(const Document &before, const Document &after)
{
    QSet<qint64> beforeKeys;
    QSet<qint64> afterKeys;
    for (qsizetype i = 0; i < before.layerCount(); ++i) {
        beforeKeys.insert(before.layer(i).image.cacheKey());
    }
    for (qsizetype i = 0; i < after.layerCount(); ++i) {
        afterKeys.insert(after.layer(i).image.cacheKey());
    }
    qint64 bytes = 0;
    for (qsizetype i = 0; i < before.layerCount(); ++i) {
        bytes +=
            afterKeys.contains(before.layer(i).image.cacheKey()) ? 0 : before.layer(i).image.sizeInBytes();
    }
    for (qsizetype i = 0; i < after.layerCount(); ++i) {
        bytes +=
            beforeKeys.contains(after.layer(i).image.cacheKey()) ? 0 : after.layer(i).image.sizeInBytes();
    }
    return bytes;
}

} // namespace

class PixelEditCommand : public UndoCommand
{
public:
    PixelEditCommand(History *history, const QString &text, qsizetype layer, const QRect &area, QImage before,
                     QImage after)
        : UndoCommand(text)
        , m_history(history)
        , m_layer(layer)
        , m_area(area)
        , m_before(std::move(before))
        , m_after(std::move(after))
    {}

    void undo() override { apply(m_before); }
    void redo() override { apply(m_after); }
    qint64 cost() const override { return m_before.sizeInBytes() + m_after.sizeInBytes(); }

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
};

class DocumentChangeCommand : public UndoCommand
{
public:
    DocumentChangeCommand(History *history, const QString &text, Document before, Document after)
        : UndoCommand(text)
        , m_history(history)
        , m_before(std::move(before))
        , m_after(std::move(after))
        , m_cost(changedBytes(m_before, m_after))
    {}

    void undo() override { apply(m_before); }
    void redo() override { apply(m_after); }
    qint64 cost() const override { return m_cost; }

private:
    void apply(const Document &state)
    {
        *m_history->m_document = state;
        Q_EMIT m_history->documentRestructured();
    }

    History *m_history;
    Document m_before;
    Document m_after;
    qint64 m_cost;
};

History::History(Document *document, QObject *parent)
    : QObject(parent)
    , m_document(document)
{
    m_stack.setLimits(UndoLimit, MemoryLimit);
}

void History::recordPixelEdit(const QString &text, qsizetype layer, const QRect &area, const QImage &before)
{
    const QRect clipped = area.intersected(m_document->rect());
    if (clipped.isEmpty()) {
        return;
    }
    const QPoint offset = clipped.topLeft() - area.topLeft();
    m_stack.push(std::make_unique<PixelEditCommand>(this, text, layer, clipped,
                                                    before.copy(QRect(offset, clipped.size())),
                                                    m_document->layer(layer).image.copy(clipped)));
}

void History::recordDocumentChange(const QString &text, const Document &before)
{
    m_stack.push(std::make_unique<DocumentChangeCommand>(this, text, before, *m_document));
}

} // namespace paint
