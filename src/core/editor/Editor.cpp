#include "editor/Editor.h"

#include "tools/ClickTools.h"
#include "tools/SelectionTool.h"
#include "tools/StrokeTools.h"
#include "tools/ToolSettings.h"

#include <QFileInfo>

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
    const auto inert = std::make_shared<InertTool>(*this);
    for (Tool tool : {Tool::Text, Tool::Shape}) {
        m_tools.insert(tool, inert);
    }
    m_activeTool = currentTool();

    connect(settings, &ToolSettings::transparentSelectionChanged, this, [this] { m_selection->refresh(); });
    connect(settings, &ToolSettings::colorChanged, this, [this](ColorSlot slot) {
        if (slot == ColorSlot::Secondary) {
            m_selection->refresh();
        }
    });
    connect(&m_history, &History::documentChanged, this, &Editor::pixelsChanged);
    connect(&m_history, &History::documentRestructured, this, &Editor::documentRestructured);
    connect(m_history.stack(), &QUndoStack::cleanChanged, this,
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

void Editor::undo()
{
    // Undo while drawing or moving a selection drops that unfinished work, not the previous step.
    if (InteractiveTool *tool = currentTool(); tool && tool->isActive()) {
        tool->cancel();
        return;
    }
    m_selection->cancel();
    m_history.undo();
}

void Editor::redo()
{
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
    if (QString error = saveImage(m_document.composite(), path, format); !error.isEmpty()) {
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
