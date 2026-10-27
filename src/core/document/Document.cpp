#include "document/Document.h"

#include <QPainter>

namespace paint {

Document::Document(QSize size, QColor background)
    : m_size(size)
    , m_backgroundColor(background)
{
    Q_ASSERT(!size.isEmpty());
    addLayer(QStringLiteral("Layer 1"));
}

qsizetype Document::addLayer(const QString &name)
{
    m_layers.append(Layer{name, makeTransparentImage(), true});
    return m_layers.size() - 1;
}

void Document::setActiveLayerIndex(qsizetype index)
{
    Q_ASSERT(index >= 0 && index < m_layers.size());
    m_activeLayer = index;
}

QImage Document::composite(const QRect &area) const
{
    const QRect clipped = area.intersected(rect());
    QImage result(clipped.size(), LayerFormat);
    result.fill(m_backgroundVisible ? m_backgroundColor : QColor(Qt::transparent));

    QPainter painter(&result);
    for (const Layer &layer : m_layers) {
        if (layer.visible) {
            painter.drawImage(QPoint(0, 0), layer.image, clipped);
        }
    }
    return result;
}

QImage Document::makeTransparentImage() const
{
    QImage image(m_size, LayerFormat);
    image.fill(Qt::transparent);
    return image;
}

} // namespace paint
