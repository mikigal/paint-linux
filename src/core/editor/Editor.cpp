#include "editor/Editor.h"

#include "tools/ClickTools.h"
#include "tools/StrokeTools.h"
#include "tools/ToolSettings.h"

#include <QFileInfo>

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
    const auto inert = std::make_shared<InertTool>(*this);
    for (Tool tool : {Tool::RectangleSelect, Tool::FreeformSelect, Tool::Text, Tool::Shape}) {
        m_tools.insert(tool, inert);
    }

    connect(&m_history, &History::documentChanged, this, &Editor::pixelsChanged);
    connect(m_history.stack(), &QUndoStack::cleanChanged, this,
            [this](bool clean) { Q_EMIT modifiedChanged(!clean); });
    // Switching tools abandons a half-drawn stroke.
    connect(settings, &ToolSettings::toolChanged, this, [this] {
        for (const auto &tool : std::as_const(m_tools)) {
            tool->cancel();
        }
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

void Editor::undo()
{
    // Undo while drawing drops the unfinished stroke rather than the previous one.
    if (InteractiveTool *tool = currentTool(); tool && tool->isActive()) {
        tool->cancel();
        return;
    }
    m_history.undo();
}

void Editor::redo()
{
    cancelTool();
    m_history.redo();
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
    document.layer(0).image = std::move(loaded.image);
    replaceDocument(std::move(document), QFileInfo(path).absoluteFilePath(), QFileInfo(path).size());
    return {};
}

QString Editor::save(const QString &path, const SaveFormat &format)
{
    cancelTool();
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
    m_history.clear();
    m_document = std::move(document);
    m_filePath = path;
    m_fileSize = fileSize;
    Q_EMIT documentReplaced();
    Q_EMIT modifiedChanged(false);
    Q_EMIT fileChanged();
}

} // namespace paint
