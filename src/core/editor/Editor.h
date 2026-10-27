#pragma once

#include "document/Document.h"
#include "document/History.h"
#include "io/ImageIO.h"
#include "ops/ImageOps.h"
#include "tools/Tool.h"
#include "tools/ToolTypes.h"

#include <QHash>
#include <QObject>

#include <functional>
#include <memory>

namespace paint {

class SelectionTool;
class ShapeTool;
class TextTool;
class ToolSettings;

// The editing session behind a window: the document, its undo history, the file it belongs to
// and the interactive tools. UI-free, so tools can be driven from tests.
class Editor : public QObject, public ToolContext
{
    Q_OBJECT

public:
    // Provisional; Paint's own default for new images is to be confirmed.
    static constexpr QSize DefaultSize{1152, 648};

    explicit Editor(ToolSettings *settings, QObject *parent = nullptr);
    ~Editor() override;

    // ToolContext
    Document &document() override { return m_document; }
    ToolSettings &settings() override { return *m_settings; }
    History &history() override { return m_history; }
    void documentChanged(const QRect &area) override;
    void requestZoom(QPointF documentPosition, bool zoomIn) override;
    void selectionChanged() override;

    const Document &document() const { return m_document; }
    // The tool for the active tool setting.
    InteractiveTool *currentTool();
    void cancelTool();
    void commitTool();

    // The selection shared by the rectangle and free-form selection tools.
    SelectionTool *selection() const { return m_selection.get(); }
    ShapeTool *shape() const { return m_shape.get(); }
    TextTool *text() const { return m_text.get(); }
    void selectAll();
    void invertSelection();
    void deleteSelection();
    // The selected pixels (or the whole layer/image without a selection).
    QImage copySelection(bool visibleLayers) const;
    QImage cutSelection();
    // Pastes `image` as a floating selection; `enlargeCanvas` grows the canvas to fit it first.
    void paste(const QImage &image, QPointF topLeft, bool enlargeCanvas = false);
    void cropToSelection();

    // Image commands: on the selection if there is one, otherwise on the whole image.
    void rotate(RotateBy rotation);
    void rotate(qreal degrees);
    void flip(Qt::Orientation orientation);
    // Resizes to `size` (pixels of the selection or image), then skews by the given angles.
    void resizeAndSkew(QSize size, qreal skewHorizontal, qreal skewVertical);
    void invertColors();
    // The size the image commands work on: the selection's bounds, or the canvas.
    QSize operationSize() const;

    // Whole-image commands.
    // The new canvas covers `area` of the current one (negative origins add room left/top).
    void resizeCanvas(const QRect &area);
    void convertToBlackAndWhite();
    void setResolution(int dotsPerMeter);

    // Layers (indices count from the bottom). Each change is one undo step.
    // A new empty layer above the active one, which it replaces as the active layer.
    void addLayer();
    void duplicateLayer();
    void deleteLayer();
    // Merges the active layer into the one below it.
    void mergeLayerDown();
    void moveLayer(qsizetype from, qsizetype to);
    void setLayerVisible(qsizetype index, bool visible);
    // Picking the layer to draw on is not an edit.
    void setActiveLayer(qsizetype index);
    void setBackgroundVisible(bool visible);
    // Also shows a hidden background.
    void setBackgroundColor(const QColor &color);

    void undo();
    void redo();

    void newDocument(QSize size = DefaultSize);
    // Returns an error message, or an empty string on success.
    QString open(const QString &path);
    QString save(const QString &path, const SaveFormat &format);

    QString filePath() const { return m_filePath; }
    // Size of the saved file in bytes, or -1 when the image was never saved.
    qint64 fileSize() const { return m_fileSize; }
    bool isModified() const { return !m_history.isClean(); }

Q_SIGNALS:
    void pixelsChanged(const QRect &area);
    // The document's size or layers changed (crop, canvas size, undo of those).
    void documentRestructured();
    void selectionStateChanged();
    // A new or opened document replaced the old one (size and layers may differ).
    void documentReplaced();
    void zoomRequested(QPointF documentPosition, bool zoomIn);
    void activeLayerChanged();
    void modifiedChanged(bool modified);
    void fileChanged();

private:
    void replaceDocument(Document document, const QString &path, qint64 fileSize);
    void useSelectionTool();
    // Finishes tool work, applies `change` to the document and records it as one undoable step.
    void changeLayers(const QString &text, const std::function<void(Document &)> &change);
    // Applies `operation` to every layer as one undoable step.
    void transformDocument(const QString &text, const std::function<ImageOp(const QImage &)> &operation);

    ToolSettings *m_settings;
    Document m_document;
    History m_history;
    QHash<Tool, std::shared_ptr<InteractiveTool>> m_tools;
    std::shared_ptr<SelectionTool> m_selection;
    std::shared_ptr<ShapeTool> m_shape;
    std::shared_ptr<TextTool> m_text;
    InteractiveTool *m_activeTool = nullptr;
    QString m_filePath;
    qint64 m_fileSize = -1;
};

} // namespace paint
