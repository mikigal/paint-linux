#pragma once

#include "document/Document.h"
#include "document/History.h"
#include "io/ImageIO.h"
#include "tools/Tool.h"
#include "tools/ToolTypes.h"

#include <QHash>
#include <QObject>

#include <memory>

namespace paint {

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

    const Document &document() const { return m_document; }
    // The tool for the active tool setting.
    InteractiveTool *currentTool();
    void cancelTool();

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
    // A new or opened document replaced the old one (size and layers may differ).
    void documentReplaced();
    void zoomRequested(QPointF documentPosition, bool zoomIn);
    void modifiedChanged(bool modified);
    void fileChanged();

private:
    void replaceDocument(Document document, const QString &path, qint64 fileSize);

    ToolSettings *m_settings;
    Document m_document;
    History m_history;
    QHash<Tool, std::shared_ptr<InteractiveTool>> m_tools;
    QString m_filePath;
    qint64 m_fileSize = -1;
};

} // namespace paint
