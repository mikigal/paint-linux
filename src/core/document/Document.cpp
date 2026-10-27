#include "document/Document.h"

#include <QPainter>

#include <algorithm>

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

void Document::insertLayer(qsizetype index, Layer layer)
{
    Q_ASSERT(index >= 0 && index <= m_layers.size());
    Q_ASSERT(layer.image.size() == m_size);
    layer.image = layer.image.convertToFormat(LayerFormat);
    m_layers.insert(index, std::move(layer));
    if (index <= m_activeLayer && m_layers.size() > 1) {
        ++m_activeLayer;
    }
}

void Document::removeLayer(qsizetype index)
{
    Q_ASSERT(m_layers.size() > 1 && index >= 0 && index < m_layers.size());
    m_layers.removeAt(index);
    // Layers above shift down; a removed active layer hands over to the one below it.
    if (m_activeLayer > index || (m_activeLayer == index && index > 0)) {
        --m_activeLayer;
    }
}

void Document::moveLayer(qsizetype from, qsizetype to)
{
    Q_ASSERT(from >= 0 && from < m_layers.size() && to >= 0 && to < m_layers.size());
    if (from == to) {
        return;
    }
    m_layers.move(from, to);
    if (m_activeLayer == from) {
        m_activeLayer = to;
    } else if (from < m_activeLayer && to >= m_activeLayer) {
        --m_activeLayer;
    } else if (from > m_activeLayer && to <= m_activeLayer) {
        ++m_activeLayer;
    }
}

void Document::setLayers(QList<Layer> layers, qsizetype active)
{
    Q_ASSERT(!layers.isEmpty() && active >= 0 && active < layers.size());
    for (Layer &layer : layers) {
        Q_ASSERT(layer.image.size() == m_size);
        layer.image = layer.image.convertToFormat(LayerFormat);
    }
    m_layers = std::move(layers);
    m_activeLayer = active;
}

QString Document::nextLayerName() const
{
    static const QString prefix = QStringLiteral("Layer ");
    int highest = 0;
    for (const Layer &layer : m_layers) {
        if (layer.name.startsWith(prefix)) {
            highest = std::max(highest, layer.name.mid(prefix.size()).toInt());
        }
    }
    return prefix + QString::number(highest + 1);
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
    result.setDotsPerMeterX(m_dotsPerMeter);
    result.setDotsPerMeterY(m_dotsPerMeter);
    result.fill(m_backgroundVisible ? m_backgroundColor : QColor(Qt::transparent));

    QPainter painter(&result);
    for (const Layer &layer : m_layers) {
        if (layer.visible) {
            painter.drawImage(QPoint(0, 0), layer.image, clipped);
        }
    }
    return result;
}

void Document::resizeCanvas(const QRect &area)
{
    Q_ASSERT(!area.isEmpty());
    m_size = area.size();
    for (Layer &layer : m_layers) {
        QImage resized = makeTransparentImage();
        QPainter painter(&resized);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(-area.topLeft(), layer.image);
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
