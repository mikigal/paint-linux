#pragma once

#include <QJsonObject>
#include <QMainWindow>

#include <optional>

class ActionRegistry;
class CanvasView;
class LayersDock;
class QLabel;
class RibbonHost;
class SidePanel;

namespace paint {
class Editor;
struct SaveFormat;
class ToolSettings;
} // namespace paint

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    ActionRegistry *actionRegistry() const { return m_actions; }
    paint::ToolSettings *toolSettings() const { return m_settings; }
    CanvasView *canvas() const { return m_canvas; }
    RibbonHost *ribbonHost() const { return m_ribbonHost; }
    paint::Editor *editor() const { return m_editor; }

    // Snapshot of the editor state reported to UI scenarios by the automation server.
    QJsonObject automationState() const;

    // Opens an image (command line, "Open", automation). Asks to save unsaved work first.
    Q_INVOKABLE bool openFile(const QString &path);
    // Saves to `path` in the format its suffix names.
    Q_INVOKABLE bool saveFile(const QString &path);
    // The visible colour of a document pixel as #AARRGGBB, for UI scenarios.
    Q_INVOKABLE QString documentPixel(int x, int y) const;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void createMenus();
    void createRibbon();
    void createStatusBar();
    void connectActions();
    void showAbout();
    void newImage();
    void openImage();
    bool saveImage();
    // Asks for a file name; `suffix` preselects a format ("png", "jpg"…).
    bool saveImageAs(const QString &suffix = {});
    bool saveFile(const QString &path, const paint::SaveFormat &format);
    // Asks whether to save unsaved work; false means the user cancelled.
    bool maybeSave();
    void updateWindowTitle();
    void updateDocumentInfo();
    void showCanvasContextMenu(QPoint globalPosition);
    void setCursorPosition(std::optional<QPoint> position);

    paint::ToolSettings *m_settings;
    paint::Editor *m_editor;
    ActionRegistry *m_actions;
    CanvasView *m_canvas;
    SidePanel *m_sidePanel;
    LayersDock *m_layers;
    RibbonHost *m_ribbonHost = nullptr;
    QLabel *m_cursorLabel = nullptr;
    QLabel *m_selectionLabel = nullptr;
    QLabel *m_canvasSizeLabel = nullptr;
    QLabel *m_fileSizeLabel = nullptr;
};
