#pragma once

#include "tools/ToolTypes.h"

#include <QHash>
#include <QList>
#include <QObject>

class QAction;
class QActionGroup;
class QWidget;

namespace paint {
class ToolSettings;
}

// Every command of the application. Menus, the ribbon, context menus and the automation
// server all share these QActions, so enabled/checked state and shortcuts stay in sync.
enum class ActionId {
    // File
    New,
    Open,
    ImportFromFile,
    Save,
    SaveAs,
    SaveAsPng,
    SaveAsJpeg,
    SaveAsBmp,
    SaveAsGif,
    SaveAsProject,
    Print,
    PageSetup,
    PrintPreview,
    WallpaperFill,
    WallpaperTile,
    WallpaperCenter,
    ImageProperties,
    Quit,
    // Edit
    Undo,
    Redo,
    Cut,
    Copy,
    CopyVisibleLayers,
    Paste,
    SelectAll,
    InvertSelection,
    DeleteSelection,
    TransparentSelection,
    // Image
    Crop,
    ResizeSkew,
    RotateRight,
    RotateLeft,
    Rotate180,
    RotateCustom,
    FlipVertical,
    FlipHorizontal,
    InvertColors,
    // View
    ZoomIn,
    ZoomOut,
    ZoomActualSize,
    ZoomFit,
    Rulers,
    Gridlines,
    StatusBar,
    ToolbarAlwaysShow,
    ToolbarAutoHide,
    LayersPanel,
    FullScreen,
    Thumbnail,
    // Layers
    AddLayer,
    DuplicateLayer,
    DeleteLayer,
    MergeLayerDown,
    ToggleLayerVisibility,
    ToggleBackground,
    BackgroundColor,
    // Tools
    ToolRectangleSelect,
    ToolFreeformSelect,
    ToolPencil,
    ToolFill,
    ToolText,
    ToolEraser,
    ToolColorPicker,
    ToolMagnifier,
    ToolBrush,
    ToolShape,
    CycleSelectionTool,
    SizeIncrease,
    SizeDecrease,
    EditColors,
    // Help
    About,
    AboutQt,
};

class ActionRegistry : public QObject
{
    Q_OBJECT

public:
    // Actions are owned by `window`, which also makes their shortcuts work without menus.
    ActionRegistry(paint::ToolSettings *settings, QWidget *window);

    QAction *get(ActionId id) const { return m_actions.value(id); }
    QAction *toolAction(paint::Tool tool) const;
    QAction *brushAction(paint::BrushType type) const { return m_brushActions.value(type); }
    QAction *shapeAction(paint::ShapeType type) const { return m_shapeActions.value(type); }
    QAction *outlineAction(paint::StrokeStyle style) const { return m_outlineActions.value(style); }
    QAction *fillAction(paint::StrokeStyle style) const { return m_fillActions.value(style); }
    // Line widths offered by the Shapes group's Size button.
    QList<QAction *> shapeSizeActions() const { return m_shapeSizeActions; }

    QList<QAction *> allActions() const;

    // Enabled state that depends on the document.
    void setSelectionAvailable(bool available);
    void setHistoryAvailable(bool canUndo, bool canRedo);
    void setLayerState(qsizetype layerCount, qsizetype activeLayer);

private:
    void createActions(QWidget *window);
    void createToolActions(QWidget *window);
    void connectSettings();
    void syncTool(paint::Tool tool);

    paint::ToolSettings *m_settings;
    QHash<ActionId, QAction *> m_actions;
    QHash<paint::BrushType, QAction *> m_brushActions;
    QHash<paint::ShapeType, QAction *> m_shapeActions;
    QHash<paint::StrokeStyle, QAction *> m_outlineActions;
    QHash<paint::StrokeStyle, QAction *> m_fillActions;
    QList<QAction *> m_shapeSizeActions;
    QActionGroup *m_toolGroup = nullptr;
    QActionGroup *m_shapeGroup = nullptr;
};
