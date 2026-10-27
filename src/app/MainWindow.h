#pragma once

#include "document/Document.h"

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
class ToolSettings;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    ActionRegistry *actionRegistry() const { return m_actions; }
    paint::ToolSettings *toolSettings() const { return m_settings; }
    CanvasView *canvas() const { return m_canvas; }
    RibbonHost *ribbonHost() const { return m_ribbonHost; }

    // Snapshot of the editor state reported to UI scenarios by the automation server.
    QJsonObject automationState() const;

private:
    void createMenus();
    void createRibbon();
    void createStatusBar();
    void connectActions();
    void showAbout();
    void showCanvasContextMenu(QPoint globalPosition);
    void setCursorPosition(std::optional<QPoint> position);

    paint::Document m_document;
    paint::ToolSettings *m_settings;
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
