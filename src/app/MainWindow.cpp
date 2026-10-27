#include "MainWindow.h"

#include "ActionRegistry.h"
#include "PaintConfig.h"
#include "canvas/CanvasView.h"
#include "canvas/SidePanel.h"
#include "panels/LayersDock.h"
#include "ribbon/PaintRibbon.h"
#include "ribbon/Ribbon.h"
#include "ribbon/RibbonHost.h"
#include "tools/ToolSettings.h"
#include "ui/Icons.h"
#include "widgets/ZoomControls.h"

#include <QAction>
#include <QApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSet>
#include <QStatusBar>
#include <QToolBar>

using namespace paint;

namespace {

// Provisional; M2 picks the default canvas size the way Paint does.
constexpr QSize DefaultCanvasSize(1152, 648);
constexpr QSize DefaultWindowSize(1280, 800);
constexpr int StatusItemWidth = 150;

QString objectSuffix(const QAction *action)
{
    return action ? action->objectName().section(QLatin1Char('.'), -1) : QString();
}

// A status bar field: a Paint icon followed by text, with a stable width so the bar does not jump.
QLabel *addStatusItem(QStatusBar *bar, const QString &name, const QString &iconName)
{
    auto *item = new QWidget(bar);
    item->setObjectName(QStringLiteral("status.") + name);
    auto *layout = new QHBoxLayout(item);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(6);
    auto *icon = new QLabel(item);
    icon->setPixmap(ui::icon(iconName).pixmap(QSize(16, 16), item->devicePixelRatioF()));
    auto *text = new QLabel(item);
    text->setObjectName(QStringLiteral("status.") + name + QStringLiteral(".text"));
    layout->addWidget(icon);
    layout->addWidget(text, 1);
    item->setFixedWidth(StatusItemWidth);
    bar->addWidget(item);
    return text;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_document(DefaultCanvasSize)
    , m_settings(new ToolSettings(this))
    , m_actions(new ActionRegistry(m_settings, this))
    , m_canvas(new CanvasView(&m_document, this))
    , m_sidePanel(new SidePanel(m_settings, m_canvas))
    , m_layers(new LayersDock(&m_document, m_actions, this))
{
    setObjectName(QStringLiteral("mainWindow"));
    // Qt appends the application display name ("— Paint for Linux").
    setWindowTitle(tr("Untitled") + QStringLiteral("[*]"));
    resize(DefaultWindowSize);

    setCentralWidget(m_canvas);
    addDockWidget(Qt::RightDockWidgetArea, m_layers);
    m_layers->hide();

    createMenus();
    createRibbon();
    createStatusBar();
    connectActions();
}

void MainWindow::createMenus()
{
    menuBar()->setObjectName(QStringLiteral("menuBar"));
    const auto add = [this](QMenu *menu, ActionId id) { menu->addAction(m_actions->get(id)); };
    const auto submenu = [](QMenu *parent, const QString &name, const QString &title) {
        QMenu *menu = parent->addMenu(title);
        menu->setObjectName(QStringLiteral("menu.") + name);
        return menu;
    };

    QMenu *file = menuBar()->addMenu(tr("&File"));
    file->setObjectName(QStringLiteral("menu.file"));
    add(file, ActionId::New);
    add(file, ActionId::Open);
    QMenu *import = submenu(file, QStringLiteral("import"), tr("&Import to canvas"));
    add(import, ActionId::ImportFromFile);
    QMenu *recent = submenu(file, QStringLiteral("recent"), tr("&Recent"));
    recent->addAction(tr("No recent images"))->setEnabled(false);
    file->addSeparator();
    add(file, ActionId::Save);
    QMenu *saveAs = submenu(file, QStringLiteral("saveAs"), tr("Save &as"));
    saveAs->setIcon(QIcon::fromTheme(QStringLiteral("document-save-as")));
    for (ActionId id : {ActionId::SaveAsPng, ActionId::SaveAsJpeg, ActionId::SaveAsBmp, ActionId::SaveAsGif,
                        ActionId::SaveAs}) {
        add(saveAs, id);
    }
    add(file, ActionId::SaveAsProject);
    file->addSeparator();
    QMenu *print = submenu(file, QStringLiteral("print"), tr("&Print"));
    print->setIcon(QIcon::fromTheme(QStringLiteral("document-print")));
    add(print, ActionId::Print);
    add(print, ActionId::PageSetup);
    add(print, ActionId::PrintPreview);
    QMenu *wallpaper = submenu(file, QStringLiteral("wallpaper"), tr("Set as &desktop background"));
    add(wallpaper, ActionId::WallpaperFill);
    add(wallpaper, ActionId::WallpaperTile);
    add(wallpaper, ActionId::WallpaperCenter);
    add(file, ActionId::ImageProperties);
    file->addSeparator();
    add(file, ActionId::Quit);

    QMenu *edit = menuBar()->addMenu(tr("&Edit"));
    edit->setObjectName(QStringLiteral("menu.edit"));
    add(edit, ActionId::Undo);
    add(edit, ActionId::Redo);
    edit->addSeparator();
    add(edit, ActionId::Cut);
    add(edit, ActionId::Copy);
    add(edit, ActionId::CopyVisibleLayers);
    add(edit, ActionId::Paste);
    edit->addSeparator();
    add(edit, ActionId::SelectAll);
    add(edit, ActionId::InvertSelection);
    add(edit, ActionId::DeleteSelection);

    QMenu *image = menuBar()->addMenu(tr("&Image"));
    image->setObjectName(QStringLiteral("menu.image"));
    add(image, ActionId::Crop);
    add(image, ActionId::ResizeSkew);
    QMenu *rotate = submenu(image, QStringLiteral("imageRotate"), tr("&Rotate"));
    rotate->setIcon(ui::icon(QStringLiteral("rotate")));
    for (ActionId id : {ActionId::RotateRight, ActionId::RotateLeft, ActionId::Rotate180}) {
        add(rotate, id);
    }
    rotate->addSeparator();
    add(rotate, ActionId::RotateCustom);
    QMenu *flip = submenu(image, QStringLiteral("imageFlip"), tr("&Flip"));
    flip->setIcon(ui::icon(QStringLiteral("flip")));
    add(flip, ActionId::FlipVertical);
    add(flip, ActionId::FlipHorizontal);
    add(image, ActionId::InvertColors);

    QMenu *view = menuBar()->addMenu(tr("&View"));
    view->setObjectName(QStringLiteral("menu.view"));
    QMenu *zoom = submenu(view, QStringLiteral("zoom"), tr("&Zoom"));
    zoom->setIcon(QIcon::fromTheme(QStringLiteral("zoom"), ui::icon(QStringLiteral("zoom-in"))));
    for (ActionId id : {ActionId::ZoomIn, ActionId::ZoomOut, ActionId::ZoomActualSize, ActionId::ZoomFit}) {
        add(zoom, id);
    }
    view->addSeparator();
    add(view, ActionId::Rulers);
    add(view, ActionId::Gridlines);
    add(view, ActionId::StatusBar);
    QMenu *toolbar = submenu(view, QStringLiteral("toolbar"), tr("&Toolbar"));
    add(toolbar, ActionId::ToolbarAlwaysShow);
    add(toolbar, ActionId::ToolbarAutoHide);
    add(view, ActionId::LayersPanel);
    view->addSeparator();
    add(view, ActionId::FullScreen);
    add(view, ActionId::Thumbnail);

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    help->setObjectName(QStringLiteral("menu.help"));
    add(help, ActionId::About);
    add(help, ActionId::AboutQt);
}

void MainWindow::createRibbon()
{
    auto *options = new QMenu(this);
    options->setObjectName(QStringLiteral("menu.toolbarOptions"));
    options->addAction(m_actions->get(ActionId::ToolbarAlwaysShow));
    options->addAction(m_actions->get(ActionId::ToolbarAutoHide));
    m_ribbonHost = new RibbonHost(createPaintRibbon(m_actions, m_settings, nullptr), options);

    // A fixed tool bar: it spans the window above the dock areas, like Paint's ribbon.
    auto *bar = new QToolBar(tr("Toolbar"), this);
    bar->setObjectName(QStringLiteral("ribbonToolBar"));
    bar->setMovable(false);
    bar->setFloatable(false);
    bar->setContextMenuPolicy(Qt::PreventContextMenu);
    bar->layout()->setContentsMargins(0, 0, 0, 0);
    bar->layout()->setSpacing(0);
    bar->addWidget(m_ribbonHost);
    addToolBar(Qt::TopToolBarArea, bar);
    setMinimumWidth(m_ribbonHost->ribbon()->minimumSizeHint().width());
}

void MainWindow::createStatusBar()
{
    QStatusBar *bar = statusBar();
    bar->setObjectName(QStringLiteral("statusBar"));
    m_cursorLabel = addStatusItem(bar, QStringLiteral("cursor"), QStringLiteral("cursor-position"));
    m_selectionLabel = addStatusItem(bar, QStringLiteral("selection"), QStringLiteral("select-rectangle"));
    m_canvasSizeLabel = addStatusItem(bar, QStringLiteral("canvasSize"), QStringLiteral("canvas-size"));
    m_fileSizeLabel = addStatusItem(bar, QStringLiteral("fileSize"), QStringLiteral("save"));
    m_fileSizeLabel->parentWidget()->hide(); // shown once the image is saved
    m_canvasSizeLabel->setText(
        tr("%1 × %2px").arg(m_document.size().width()).arg(m_document.size().height()));
    bar->addPermanentWidget(new ZoomControls(m_canvas, m_actions, bar));
}

void MainWindow::setCursorPosition(std::optional<QPoint> position)
{
    m_cursorLabel->setText(position ? tr("%1, %2px").arg(position->x()).arg(position->y()) : QString());
}

void MainWindow::connectActions()
{
    const auto on = [this](ActionId id, auto slot) {
        connect(m_actions->get(id), &QAction::triggered, this, slot);
    };

    on(ActionId::Quit, &QWidget::close);
    on(ActionId::About, &MainWindow::showAbout);
    on(ActionId::AboutQt, &QApplication::aboutQt);
    on(ActionId::ZoomIn, [this] { m_canvas->zoomIn(); });
    on(ActionId::ZoomOut, [this] { m_canvas->zoomOut(); });
    on(ActionId::ZoomActualSize, [this] { m_canvas->zoomToActualSize(); });
    on(ActionId::ZoomFit, [this] { m_canvas->zoomToFit(); });
    on(ActionId::FullScreen, [this](bool checked) { checked ? showFullScreen() : showNormal(); });
    on(ActionId::ToolbarAlwaysShow, [this] { m_ribbonHost->setAutoHide(false); });
    on(ActionId::ToolbarAutoHide, [this] { m_ribbonHost->setAutoHide(true); });

    connect(m_actions->get(ActionId::StatusBar), &QAction::toggled, statusBar(), &QWidget::setVisible);
    connect(m_actions->get(ActionId::LayersPanel), &QAction::toggled, m_layers, &QWidget::setVisible);
    connect(m_layers, &QDockWidget::visibilityChanged, this,
            [this] { m_actions->get(ActionId::LayersPanel)->setChecked(m_layers->isVisible()); });

    connect(m_canvas, &CanvasView::cursorPositionChanged, this, &MainWindow::setCursorPosition);
    connect(m_canvas, &CanvasView::contextMenuRequested, this, &MainWindow::showCanvasContextMenu);

    // Commands from later milestones say so instead of silently doing nothing.
    static const QSet<ActionId> implemented{
        ActionId::Quit,
        ActionId::About,
        ActionId::AboutQt,
        ActionId::ZoomIn,
        ActionId::ZoomOut,
        ActionId::ZoomActualSize,
        ActionId::ZoomFit,
        ActionId::FullScreen,
        ActionId::StatusBar,
        ActionId::LayersPanel,
        ActionId::ToolbarAlwaysShow,
        ActionId::ToolbarAutoHide,
        ActionId::TransparentSelection,
        ActionId::CycleSelectionTool,
        ActionId::SizeIncrease,
        ActionId::SizeDecrease,
    };
    QSet<QAction *> handled;
    for (ActionId id : implemented) {
        handled.insert(m_actions->get(id));
    }
    const QStringList toolChoicePrefixes{
        QStringLiteral("action.tool."),  QStringLiteral("action.brush."),
        QStringLiteral("action.shape."), QStringLiteral("action.outline."),
        QStringLiteral("action.fill."),  QStringLiteral("action.shapeSize."),
    };
    for (QAction *action : m_actions->allActions()) {
        // Tool, brush, shape and style choices are handled by the registry itself.
        const bool toolChoice = std::ranges::any_of(toolChoicePrefixes, [action](const QString &prefix) {
            return action->objectName().startsWith(prefix);
        });
        if (!handled.contains(action) && !toolChoice) {
            connect(action, &QAction::triggered, this, [this, action] {
                statusBar()->showMessage(tr("“%1” is not available yet.").arg(action->iconText()), 3000);
            });
        }
        // Running any command dismisses the auto-hidden ribbon.
        connect(action, &QAction::triggered, m_ribbonHost, &RibbonHost::hideRibbonPopup);
    }
}

void MainWindow::showCanvasContextMenu(QPoint globalPosition)
{
    // Paint offers the editing menu for selections; drawing tools use the right button to paint.
    if (!isSelectionTool(m_settings->tool())) {
        return;
    }
    QMenu menu(this);
    menu.setObjectName(QStringLiteral("menu.canvasContext"));
    for (ActionId id : {ActionId::Cut, ActionId::Copy, ActionId::Paste, ActionId::CopyVisibleLayers}) {
        menu.addAction(m_actions->get(id));
    }
    menu.addSeparator();
    for (ActionId id :
         {ActionId::Crop, ActionId::SelectAll, ActionId::InvertSelection, ActionId::DeleteSelection}) {
        menu.addAction(m_actions->get(id));
    }
    menu.addSeparator();
    menu.addMenu(findChild<QMenu *>(QStringLiteral("menu.imageRotate")));
    menu.addMenu(findChild<QMenu *>(QStringLiteral("menu.imageFlip")));
    menu.addAction(m_actions->get(ActionId::ResizeSkew));
    menu.addAction(m_actions->get(ActionId::InvertColors));
    menu.exec(globalPosition);
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("About %1").arg(QApplication::applicationDisplayName()),
                       tr("<h3>%1 %2</h3><p>%3.</p>")
                           .arg(QApplication::applicationDisplayName(), QApplication::applicationVersion(),
                                QStringLiteral(PAINT_DESCRIPTION)));
}

QJsonObject MainWindow::automationState() const
{
    QJsonArray layers;
    for (qsizetype i = 0; i < m_document.layerCount(); ++i) {
        const paint::Layer &layer = m_document.layer(i);
        layers.append(
            QJsonObject{{QStringLiteral("name"), layer.name}, {QStringLiteral("visible"), layer.visible}});
    }
    QJsonArray compactGroups;
    for (RibbonGroup *group : m_ribbonHost->ribbon()->groups()) {
        if (group->isCompact()) {
            compactGroups.append(group->title());
        }
    }
    return QJsonObject{
        {QStringLiteral("canvasSize"), QJsonArray{m_document.size().width(), m_document.size().height()}},
        {QStringLiteral("backgroundColor"), m_document.backgroundColor().name(QColor::HexArgb)},
        {QStringLiteral("backgroundVisible"), m_document.isBackgroundVisible()},
        {QStringLiteral("activeLayer"), m_document.activeLayerIndex()},
        {QStringLiteral("layers"), layers},
        {QStringLiteral("modified"), isWindowModified()},
        {QStringLiteral("tool"), objectSuffix(m_actions->toolAction(m_settings->tool()))},
        {QStringLiteral("brush"), objectSuffix(m_actions->brushAction(m_settings->brushType()))},
        {QStringLiteral("shape"), objectSuffix(m_actions->shapeAction(m_settings->shapeType()))},
        {QStringLiteral("color1"), m_settings->color(ColorSlot::Primary).name()},
        {QStringLiteral("color2"), m_settings->color(ColorSlot::Secondary).name()},
        {QStringLiteral("activeColor"), m_settings->activeColorSlot() == ColorSlot::Primary ? 1 : 2},
        {QStringLiteral("size"), m_settings->size()},
        {QStringLiteral("opacity"), m_settings->opacity()},
        {QStringLiteral("zoom"), m_canvas->zoom()},
        {QStringLiteral("compactGroups"), compactGroups},
        {QStringLiteral("ribbonAutoHide"), m_ribbonHost->autoHide()},
        {QStringLiteral("layersPanel"), m_layers->isVisible()},
    };
}
