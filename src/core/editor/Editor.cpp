#include "editor/Editor.h"

#include "io/OpenRaster.h"
#include "tools/ClickTools.h"
#include "tools/SelectionTool.h"
#include "tools/ShapeTool.h"
#include "tools/StrokeTools.h"
#include "tools/TextTool.h"
#include "tools/ToolSettings.h"

#include <QFileInfo>
#include <QPainter>

#include <cmath>

namespace paint {

Editor::Editor(ToolSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_document(DefaultSize)
    , m_history(&m_document)
{
    m_tools.insert(Tool::Pencil, std::make_shared<PencilTool>(*this));
    m_tools.insert(Tool::Brush, std::make_shared<BrushTool>(*this));
    m_tools.insert(Tool::Eraser, std::make_shared<EraserTool>(*this));
    m_tools.insert(Tool::Fill, std::make_shared<FillTool>(*this));
    m_tools.insert(Tool::ColorPicker, std::make_shared<ColorPickerTool>(*this));
    m_tools.insert(Tool::Magnifier, std::make_shared<MagnifierTool>(*this));
    // Both selection tools share one selection; they differ only in how a new one is drawn.
    m_selection = std::make_shared<SelectionTool>(*this);
    m_tools.insert(Tool::RectangleSelect, m_selection);
    m_tools.insert(Tool::FreeformSelect, m_selection);
    m_shape = std::make_shared<ShapeTool>(*this);
    m_tools.insert(Tool::Shape, m_shape);
    m_text = std::make_shared<TextTool>(*this);
    m_tools.insert(Tool::Text, m_text);
    m_activeTool = currentTool();

    connect(settings, &ToolSettings::transparentSelectionChanged, this, [this] { m_selection->refresh(); });
    connect(settings, &ToolSettings::colorChanged, this, [this](ColorSlot slot) {
        if (slot == ColorSlot::Secondary) {
            m_selection->refresh();
        }
        m_shape->refresh();
        m_text->refresh();
    });
    // A shape being edited follows the size and style settings; picking another shape places it.
    connect(settings, &ToolSettings::sizeChanged, this, [this] { m_shape->refresh(); });
    connect(settings, &ToolSettings::outlineStyleChanged, this, [this] { m_shape->refresh(); });
    connect(settings, &ToolSettings::fillStyleChanged, this, [this] { m_shape->refresh(); });
    connect(settings, &ToolSettings::shapeTypeChanged, this, [this] { m_shape->commit(); });
    // The format bar formats the text being typed.
    connect(settings, &ToolSettings::textFormatChanged, this,
            [this](const TextFormat &format) { m_text->applyFormat(format); });
    connect(settings, &ToolSettings::textAlignmentChanged, this,
            [this](Qt::Alignment alignment) { m_text->applyAlignment(alignment); });
    connect(settings, &ToolSettings::textBackgroundChanged, this, [this] { m_text->refresh(); });
    connect(&m_history, &History::documentChanged, this, &Editor::pixelsChanged);
    connect(&m_history, &History::documentRestructured, this, &Editor::documentRestructured);
    connect(m_history.stack(), &UndoStack::cleanChanged, this,
            [this](bool clean) { Q_EMIT modifiedChanged(!clean); });
    // Switching tools finishes the old tool's work, e.g. places a moved selection.
    connect(settings, &ToolSettings::toolChanged, this, [this] {
        InteractiveTool *next = currentTool();
        if (m_activeTool && m_activeTool != next) {
            m_activeTool->commit();
        }
        m_activeTool = next;
    });
}

Editor::~Editor() = default;

void Editor::documentChanged(const QRect &area)
{
    Q_EMIT pixelsChanged(area);
}

void Editor::requestZoom(QPointF documentPosition, bool zoomIn)
{
    Q_EMIT zoomRequested(documentPosition, zoomIn);
}

InteractiveTool *Editor::currentTool()
{
    return m_tools.value(m_settings->tool()).get();
}

void Editor::cancelTool()
{
    if (InteractiveTool *tool = currentTool()) {
        tool->cancel();
    }
}

void Editor::commitTool()
{
    if (InteractiveTool *tool = currentTool()) {
        tool->commit();
    }
}

void Editor::selectionChanged()
{
    Q_EMIT selectionStateChanged();
}

void Editor::changeLayers(const QString &text, const std::function<void(Document &)> &change)
{
    commitTool();
    m_selection->commit();
    const Document before = m_document;
    change(m_document);
    m_history.recordDocumentChange(text, before);
    Q_EMIT documentRestructured();
}

void Editor::addLayer()
{
    changeLayers(tr("Add layer"), [](Document &document) {
        const qsizetype index = document.activeLayerIndex() + 1;
        QImage image(document.size(), Document::LayerFormat);
        image.fill(Qt::transparent);
        document.insertLayer(index, Layer{document.nextLayerName(), image, true});
        document.setActiveLayerIndex(index);
    });
}

void Editor::duplicateLayer()
{
    changeLayers(tr("Duplicate layer"), [](Document &document) {
        const qsizetype active = document.activeLayerIndex();
        Layer copy = document.layer(active);
        copy.name = document.nextLayerName();
        document.insertLayer(active + 1, copy);
        document.setActiveLayerIndex(active + 1);
    });
}

void Editor::deleteLayer()
{
    if (m_document.layerCount() < 2) {
        return;
    }
    changeLayers(tr("Delete layer"),
                 [](Document &document) { document.removeLayer(document.activeLayerIndex()); });
}

void Editor::mergeLayerDown()
{
    if (m_document.activeLayerIndex() < 1) {
        return;
    }
    changeLayers(tr("Merge down"), [](Document &document) {
        const qsizetype upper = document.activeLayerIndex();
        QPainter painter(&document.layer(upper - 1).image);
        painter.drawImage(QPoint(), document.layer(upper).image);
        painter.end();
        document.removeLayer(upper);
    });
}

void Editor::moveLayer(qsizetype from, qsizetype to)
{
    const qsizetype count = m_document.layerCount();
    if (from == to || from < 0 || to < 0 || from >= count || to >= count) {
        return;
    }
    changeLayers(tr("Move layer"), [from, to](Document &document) { document.moveLayer(from, to); });
}

void Editor::setLayerVisible(qsizetype index, bool visible)
{
    if (index < 0 || index >= m_document.layerCount() || m_document.layer(index).visible == visible) {
        return;
    }
    changeLayers(visible ? tr("Show layer") : tr("Hide layer"),
                 [index, visible](Document &document) { document.layer(index).visible = visible; });
}

void Editor::setActiveLayer(qsizetype index)
{
    if (index < 0 || index >= m_document.layerCount() || index == m_document.activeLayerIndex()) {
        return;
    }
    // A moved selection or a shape belongs to the layer it was made on.
    commitTool();
    m_selection->commit();
    m_document.setActiveLayerIndex(index);
    Q_EMIT activeLayerChanged();
}

void Editor::setBackgroundVisible(bool visible)
{
    if (visible == m_document.isBackgroundVisible()) {
        return;
    }
    changeLayers(visible ? tr("Show background") : tr("Hide background"),
                 [visible](Document &document) { document.setBackgroundVisible(visible); });
}

void Editor::setBackgroundColor(const QColor &color)
{
    if (!color.isValid() || (color == m_document.backgroundColor() && m_document.isBackgroundVisible())) {
        return;
    }
    // A new background colour is meant to be seen.
    changeLayers(tr("Background color"), [color](Document &document) {
        document.setBackgroundColor(color);
        document.setBackgroundVisible(true);
    });
}

void Editor::undo()
{
    // Undo while drawing or moving a selection drops that unfinished work, not the previous step;
    // while typing it undoes the typing first.
    if (InteractiveTool *tool = currentTool(); tool && tool->isActive()) {
        if (!tool->undo()) {
            tool->cancel();
        }
        return;
    }
    m_selection->cancel();
    m_history.undo();
}

void Editor::redo()
{
    if (InteractiveTool *tool = currentTool(); tool && tool->redo()) {
        return;
    }
    cancelTool();
    m_selection->cancel();
    m_history.redo();
}

void Editor::transformDocument(const QString &text, const std::function<ImageOp(const QImage &)> &operation)
{
    commitTool();
    m_selection->commit();
    const Document before = m_document;
    m_document.transformLayers([&operation](const QImage &layer) { return operation(layer).image; });
    m_history.recordDocumentChange(text, before);
    Q_EMIT documentRestructured();
}

QSize Editor::operationSize() const
{
    return m_selection->hasSelection() ? m_selection->bounds().size() : m_document.size();
}

void Editor::rotate(RotateBy rotation)
{
    const auto operation = [rotation](const QImage &image) { return paint::rotate(image, rotation); };
    if (m_selection->hasSelection()) {
        m_selection->transformSelection(operation);
    } else {
        transformDocument(tr("Rotate"), operation);
    }
}

void Editor::rotate(qreal degrees)
{
    if (m_selection->hasSelection()) {
        m_selection->rotateBy(degrees);
    } else {
        transformDocument(tr("Rotate"),
                          [degrees](const QImage &image) { return paint::rotate(image, degrees); });
    }
}

void Editor::flip(Qt::Orientation orientation)
{
    const auto operation = [orientation](const QImage &image) { return paint::flip(image, orientation); };
    if (m_selection->hasSelection()) {
        m_selection->transformSelection(operation);
    } else {
        transformDocument(tr("Flip"), operation);
    }
}

void Editor::resizeAndSkew(QSize size, qreal skewHorizontal, qreal skewVertical)
{
    const auto operation = [size, skewHorizontal, skewVertical](const QImage &image) {
        const ImageOp scaled = paint::scale(image, size);
        ImageOp skewed = paint::skew(scaled.image, skewHorizontal, skewVertical);
        skewed.mapping = scaled.mapping * skewed.mapping;
        return skewed;
    };
    if (m_selection->hasSelection()) {
        m_selection->transformSelection(operation);
    } else {
        transformDocument(tr("Resize and skew"), operation);
    }
}

void Editor::invertColors()
{
    if (m_selection->hasSelection()) {
        m_selection->transformSelection([](const QImage &image) {
            QImage inverted = image;
            paint::invertColors(inverted);
            return ImageOp{inverted, QTransform()};
        });
        return;
    }
    // Without a selection Paint inverts the current layer.
    commitTool();
    const qsizetype layer = m_document.activeLayerIndex();
    const QImage before = m_document.layer(layer).image;
    paint::invertColors(m_document.layer(layer).image);
    m_history.recordPixelEdit(tr("Invert colors"), layer, m_document.rect(), before);
    Q_EMIT pixelsChanged(m_document.rect());
}

void Editor::resizeCanvas(const QRect &area)
{
    if (area.isEmpty() || area == m_document.rect()) {
        return;
    }
    commitTool();
    m_selection->commit();
    const Document before = m_document;
    m_document.resizeCanvas(area);
    m_history.recordDocumentChange(tr("Canvas size"), before);
    Q_EMIT documentRestructured();
}

void Editor::convertToBlackAndWhite()
{
    transformDocument(tr("Black and white"),
                      [](const QImage &image) { return ImageOp{toBlackAndWhite(image), QTransform()}; });
}

void Editor::setResolution(int dotsPerMeter)
{
    if (dotsPerMeter == m_document.dotsPerMeter()) {
        return;
    }
    const Document before = m_document;
    m_document.setDotsPerMeter(dotsPerMeter);
    m_history.recordDocumentChange(tr("Resolution"), before);
    Q_EMIT documentRestructured();
}

void Editor::useSelectionTool()
{
    if (!isSelectionTool(m_settings->tool())) {
        m_settings->setTool(m_settings->selectionTool());
    }
}

void Editor::selectAll()
{
    useSelectionTool();
    m_selection->selectAll();
}

void Editor::invertSelection()
{
    m_selection->invert();
}

void Editor::deleteSelection()
{
    m_selection->deleteSelected();
}

QImage Editor::copySelection(bool visibleLayers) const
{
    return m_selection->copy(visibleLayers);
}

QImage Editor::cutSelection()
{
    const QImage image = m_selection->copy(false);
    m_selection->deleteSelected();
    return image;
}

void Editor::paste(const QImage &image, QPointF topLeft, bool enlargeCanvas)
{
    if (image.isNull()) {
        return;
    }
    commitTool();
    m_selection->commit();
    if (enlargeCanvas) {
        // Grow the canvas so the pasted image fits, as Paint offers.
        const QSize needed = m_document.size().expandedTo(
            QSize(int(std::ceil(topLeft.x())) + image.width(), int(std::ceil(topLeft.y())) + image.height()));
        if (needed != m_document.size()) {
            const Document before = m_document;
            m_document.resizeCanvas(needed);
            m_history.recordDocumentChange(tr("Enlarge canvas"), before);
            Q_EMIT documentRestructured();
        }
    }
    m_settings->setTool(Tool::RectangleSelect);
    m_activeTool = currentTool();
    m_selection->paste(image, topLeft);
}

void Editor::cropToSelection()
{
    if (!m_selection->hasSelection()) {
        return;
    }
    m_selection->place();
    const QPainterPath shape = m_selection->outline();
    const QRect area = shape.boundingRect().toAlignedRect().intersected(m_document.rect());
    m_selection->deselect();
    if (area.isEmpty()) {
        return;
    }
    const Document before = m_document;
    // Rectangular selections crop exactly; free-form ones also clear what lies outside them.
    QPainterPath box;
    box.addRect(QRectF(area));
    m_document.crop(area, shape.subtracted(box).isEmpty() && box.subtracted(shape).isEmpty() ? QPainterPath()
                                                                                             : shape);
    m_history.recordDocumentChange(tr("Crop"), before);
    Q_EMIT documentRestructured();
}

void Editor::newDocument(QSize size)
{
    replaceDocument(Document(size), {}, -1);
}

QString Editor::open(const QString &path)
{
    if (isOpenRasterPath(path)) {
        LoadedProject project = loadOpenRaster(path);
        if (!project.document) {
            return project.error;
        }
        replaceDocument(std::move(*project.document), QFileInfo(path).absoluteFilePath(),
                        QFileInfo(path).size());
        return {};
    }
    LoadedImage loaded = loadImage(path);
    if (!loaded.error.isEmpty()) {
        return loaded.error;
    }
    Document document(loaded.image.size());
    // Transparent images open with the background hidden, so their transparency survives.
    document.setBackgroundVisible(!hasTransparency(loaded.image));
    document.setDotsPerMeter(loaded.image.dotsPerMeterX());
    document.layer(0).image = std::move(loaded.image);
    replaceDocument(std::move(document), QFileInfo(path).absoluteFilePath(), QFileInfo(path).size());
    return {};
}

QString Editor::save(const QString &path, const SaveFormat &format)
{
    commitTool();
    // Projects keep the layers; images are flattened.
    QString error = format.format == projectFormat().format ? saveOpenRaster(m_document, path)
                                                            : saveImage(m_document.composite(), path, format);
    if (!error.isEmpty()) {
        return error;
    }
    m_filePath = QFileInfo(path).absoluteFilePath();
    m_fileSize = QFileInfo(path).size();
    m_history.setClean();
    Q_EMIT fileChanged();
    return {};
}

void Editor::replaceDocument(Document document, const QString &path, qint64 fileSize)
{
    cancelTool();
    m_selection->cancel();
    m_history.clear();
    m_document = std::move(document);
    m_filePath = path;
    m_fileSize = fileSize;
    Q_EMIT documentReplaced();
    Q_EMIT modifiedChanged(false);
    Q_EMIT fileChanged();
}

} // namespace paint
