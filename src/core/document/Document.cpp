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

void Document::resizeCanvas(QSize size)
{
    Q_ASSERT(!size.isEmpty());
    m_size = size;
    for (Layer &layer : m_layers) {
        QImage resized = makeTransparentImage();
        QPainter painter(&resized);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(0, 0, layer.image);
        painter.end();
        layer.image = std::move(resized);
    }
}

void Document::crop(const QRect &area, const QPainterPath &shape)
{
    const QRect clipped = area.intersected(rect());
    Q_ASSERT(!clipped.isEmpty());
    m_size = clipped.size();
    for (Layer &layer : m_layers) {
        QImage cropped = layer.image.copy(clipped);
        if (!shape.isEmpty()) {
            // Clear what lies outside the shape (fillPath only reaches pixels inside a path).
            QPainterPath outside;
            outside.addRect(QRectF(cropped.rect()));
            QPainter painter(&cropped);
            painter.setCompositionMode(QPainter::CompositionMode_Clear);
            painter.fillPath(outside.subtracted(shape.translated(-clipped.topLeft())), Qt::black);
        }
        layer.image = std::move(cropped);
    }
}

QImage Document::makeTransparentImage() const
{
    QImage image(m_size, LayerFormat);
    image.fill(Qt::transparent);
    return image;
}

} // namespace paint
