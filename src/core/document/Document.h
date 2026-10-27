#pragma once

#include "document/Layer.h"

#include <QColor>
#include <QImage>
#include <QList>
#include <QRect>
#include <QSize>

namespace paint {

// The image being edited: a solid background plus a stack of raster layers.
// Mirrors Paint's model, where the background is a separate tile that can be
// recoloured or hidden, and every layer above it starts out transparent.
class Document
{
public:
    static constexpr QImage::Format LayerFormat = QImage::Format_ARGB32_Premultiplied;

    explicit Document(QSize size, QColor background = Qt::white);

    QSize size() const { return m_size; }
    QRect rect() const { return QRect(QPoint(0, 0), m_size); }

    QColor backgroundColor() const { return m_backgroundColor; }
    void setBackgroundColor(const QColor &color) { m_backgroundColor = color; }
    bool isBackgroundVisible() const { return m_backgroundVisible; }
    void setBackgroundVisible(bool visible) { m_backgroundVisible = visible; }

    // Layers are ordered bottom to top.
    qsizetype layerCount() const { return m_layers.size(); }
    const Layer &layer(qsizetype index) const { return m_layers.at(index); }
    Layer &layer(qsizetype index) { return m_layers[index]; }
    qsizetype addLayer(const QString &name);

    qsizetype activeLayerIndex() const { return m_activeLayer; }
    void setActiveLayerIndex(qsizetype index);
    Layer &activeLayer() { return m_layers[m_activeLayer]; }

    // Flattens the visible background and layers within `area` (document
    // coordinates). The result has the size of the clipped area.
    QImage composite(const QRect &area) const;
    QImage composite() const { return composite(rect()); }

private:
    QImage makeTransparentImage() const;

    QSize m_size;
    QColor m_backgroundColor;
    bool m_backgroundVisible = true;
    QList<Layer> m_layers;
    qsizetype m_activeLayer = 0;
};

} // namespace paint
