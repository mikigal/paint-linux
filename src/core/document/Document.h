#pragma once

#include "document/Layer.h"

#include <QColor>
#include <QImage>
#include <QList>
#include <QPainterPath>
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
    // 96 DPI, Paint's default resolution.
    static constexpr int DefaultDotsPerMeter = 3780;

    explicit Document(QSize size, QColor background = Qt::white);

    QSize size() const { return m_size; }
    QRect rect() const { return QRect(QPoint(0, 0), m_size); }

    QColor backgroundColor() const { return m_backgroundColor; }
    void setBackgroundColor(const QColor &color) { m_backgroundColor = color; }
    bool isBackgroundVisible() const { return m_backgroundVisible; }
    void setBackgroundVisible(bool visible) { m_backgroundVisible = visible; }

    // Print resolution, saved with the image (Paint shows it in Image properties as DPI).
    int dotsPerMeter() const { return m_dotsPerMeter; }
    void setDotsPerMeter(int dotsPerMeter)
    {
        m_dotsPerMeter = dotsPerMeter > 0 ? dotsPerMeter : DefaultDotsPerMeter;
    }

    // Layers are ordered bottom to top.
    qsizetype layerCount() const { return m_layers.size(); }
    const Layer &layer(qsizetype index) const { return m_layers.at(index); }
    Layer &layer(qsizetype index) { return m_layers[index]; }
    qsizetype addLayer(const QString &name);
    // Inserts `layer` (document-sized) at `index`; the active layer stays the same layer.
    void insertLayer(qsizetype index, Layer layer);
    // Removes a layer (never the last one); the layer below becomes active if it was.
    void removeLayer(qsizetype index);
    // Moves a layer to `to`; the active layer stays the same layer.
    void moveLayer(qsizetype from, qsizetype to);
    // Replaces all layers (each document-sized, at least one) and picks the active one.
    void setLayers(QList<Layer> layers, qsizetype active);
    // "Layer N" with N one past the highest number in use.
    QString nextLayerName() const;

    qsizetype activeLayerIndex() const { return m_activeLayer; }
    void setActiveLayerIndex(qsizetype index);
    Layer &activeLayer() { return m_layers[m_activeLayer]; }

    // Changes the canvas size keeping the top-left corner; new areas are transparent on every
    // layer (the background colour shows through).
    void resizeCanvas(QSize size) { resizeCanvas(QRect(QPoint(0, 0), size)); }
    // The new canvas covers `area` of the current one: e.g. a negative x adds room on the left.
    void resizeCanvas(const QRect &area);
    // Replaces every layer by `transform(layer)`; all results must have the same size, which
    // becomes the canvas size.
    template<typename Transform>
    void transformLayers(Transform transform)
    {
        for (Layer &layer : m_layers) {
            layer.image = transform(std::as_const(layer.image));
        }
        m_size = m_layers.first().image.size();
    }
    // Cuts the canvas down to `area`; pixels outside `shape` (if given) become transparent.
    void crop(const QRect &area, const QPainterPath &shape = {});

    // Flattens the visible background and layers within `area` (document
    // coordinates). The result has the size of the clipped area.
    QImage composite(const QRect &area) const;
    QImage composite() const { return composite(rect()); }

private:
    QImage makeTransparentImage() const;

    QSize m_size;
    QColor m_backgroundColor;
    bool m_backgroundVisible = true;
    int m_dotsPerMeter = DefaultDotsPerMeter;
    QList<Layer> m_layers;
    qsizetype m_activeLayer = 0;
};

} // namespace paint
