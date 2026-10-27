#include "MainWindow.h"

#include "ActionRegistry.h"
#include "PaintConfig.h"
#include "canvas/CanvasView.h"
#include "canvas/SidePanel.h"
#include "dialogs/CustomRotateDialog.h"
#include "dialogs/EditColorsDialog.h"
#include "dialogs/ImagePropertiesDialog.h"
#include "dialogs/ResizeSkewDialog.h"
#include "editor/Editor.h"
#include "io/ImageIO.h"
#include "io/MimeImport.h"
#include "io/OpenRaster.h"
#include "panels/LayersDock.h"
#include "platform/RemoteImageLoader.h"
#include "ribbon/PaintRibbon.h"
#include "ribbon/Ribbon.h"
#include "ribbon/RibbonHost.h"
#include "tools/SelectionTool.h"
#include "tools/ShapeTool.h"
#include "tools/TextTool.h"
#include "tools/ToolSettings.h"
#include "ui/Icons.h"
#include "widgets/TextFormatBar.h"
#include "widgets/ZoomControls.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>

#include <cmath>

using namespace paint;

namespace {

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
    , m_settings(new ToolSettings(this))
    , m_editor(new Editor(m_settings, this))
    , m_actions(new ActionRegistry(m_settings, this))
    , m_canvas(new CanvasView(m_editor, this))
    , m_sidePanel(new SidePanel(m_settings, m_canvas))
    , m_layers(new LayersDock(m_editor, m_actions, this))
    , m_remoteImages(new RemoteImageLoader(this))
{
    setObjectName(QStringLiteral("mainWindow"));
    resize(DefaultWindowSize);

    setCentralWidget(m_canvas);
    // Files dropped anywhere outside the image itself are opened.
    setAcceptDrops(true);
    addDockWidget(Qt::RightDockWidgetArea, m_layers);
    m_layers->hide();

    createMenus();
    createRibbon();
    createStatusBar();
    connectActions();
    updateWindowTitle();
    updateDocumentInfo();
    updateSelectionInfo();
    updatePasteAvailability();
}

MainWindow::~MainWindow()
{
    // The editor (a child) clears its undo stack while being destroyed, after this object is no
    // longer a MainWindow; its signals must not reach the lambdas above by then.
    disconnect(m_editor, nullptr, this, nullptr);
    disconnect(m_editor->history().stack(), nullptr, this, nullptr);
    // A visible layers panel reports being hidden as the window goes.
    disconnect(m_layers, nullptr, this, nullptr);
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
    image->addSeparator();
    // Paint keeps these on the layers panel; a native menu lists them as well.
    QMenu *layers = submenu(image, QStringLiteral("imageLayers"), tr("&Layers"));
    layers->setIcon(ui::icon(QStringLiteral("layers")));
    for (ActionId id : {ActionId::AddLayer, ActionId::DuplicateLayer, ActionId::MergeLayerDown,
                        ActionId::ToggleLayerVisibility, ActionId::DeleteLayer}) {
        add(layers, id);
    }
    layers->addSeparator();
    add(layers, ActionId::ToggleBackground);
    add(layers, ActionId::BackgroundColor);

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

    // The text tool's format bar appears in a row of its own under the ribbon.
    addToolBarBreak(Qt::TopToolBarArea);
    m_textBar = new TextFormatBar(m_settings, m_actions, this);
    addToolBar(Qt::TopToolBarArea, m_textBar);
    m_textBar->setVisible(m_settings->tool() == Tool::Text);
    connect(m_settings, &ToolSettings::toolChanged, m_textBar,
            [this](Tool tool) { m_textBar->setVisible(tool == Tool::Text); });
    connect(m_textBar, &TextFormatBar::finished, m_canvas, [this] { m_canvas->setFocus(); });
}

void MainWindow::createStatusBar()
{
    QStatusBar *bar = statusBar();
    bar->setObjectName(QStringLiteral("statusBar"));
    m_cursorLabel = addStatusItem(bar, QStringLiteral("cursor"), QStringLiteral("cursor-position"));
    m_selectionLabel = addStatusItem(bar, QStringLiteral("selection"), QStringLiteral("select-rectangle"));
    m_canvasSizeLabel = addStatusItem(bar, QStringLiteral("canvasSize"), QStringLiteral("canvas-size"));
    m_fileSizeLabel = addStatusItem(bar, QStringLiteral("fileSize"), QStringLiteral("save"));
    bar->addPermanentWidget(new ZoomControls(m_canvas, m_actions, bar));
}

void MainWindow::updateWindowTitle()
{
    const QString path = m_editor->filePath();
    // Qt appends the application display name ("… — Paint for Linux").
    setWindowTitle((path.isEmpty() ? tr("Untitled") : QFileInfo(path).fileName()) + QStringLiteral("[*]"));
    setWindowModified(m_editor->isModified());
}

void MainWindow::updateDocumentInfo()
{
    const QSize size = m_editor->document().size();
    m_canvasSizeLabel->setText(tr("%1 × %2px").arg(size.width()).arg(size.height()));
    // Paint shows the size of the saved file ("Size: 20.4KB") once there is one.
    const qint64 bytes = m_editor->fileSize();
    m_fileSizeLabel->parentWidget()->setVisible(bytes >= 0);
    if (bytes >= 0) {
        const double kilobytes = bytes / 1024.0;
        m_fileSizeLabel->setText(kilobytes < 1024 ? tr("Size: %1KB").arg(kilobytes, 0, 'f', 1)
                                                  : tr("Size: %1MB").arg(kilobytes / 1024.0, 0, 'f', 1));
    }
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
    on(ActionId::New, &MainWindow::newImage);
    on(ActionId::Open, &MainWindow::openImage);
    on(ActionId::Save, &MainWindow::saveImage);
    on(ActionId::SaveAs, [this] { saveImageAs(); });
    on(ActionId::SaveAsPng, [this] { saveImageAs(QStringLiteral("png")); });
    on(ActionId::SaveAsJpeg, [this] { saveImageAs(QStringLiteral("jpg")); });
    on(ActionId::SaveAsProject, &MainWindow::saveProjectAs);
    on(ActionId::AddLayer, [this] { m_editor->addLayer(); });
    on(ActionId::DuplicateLayer, [this] { m_editor->duplicateLayer(); });
    on(ActionId::DeleteLayer, [this] { m_editor->deleteLayer(); });
    on(ActionId::MergeLayerDown, [this] { m_editor->mergeLayerDown(); });
    on(ActionId::ToggleLayerVisibility, [this] {
        const qsizetype active = m_editor->document().activeLayerIndex();
        m_editor->setLayerVisible(active, !m_editor->document().layer(active).visible);
    });
    on(ActionId::ToggleBackground,
       [this] { m_editor->setBackgroundVisible(!m_editor->document().isBackgroundVisible()); });
    on(ActionId::BackgroundColor, &MainWindow::showBackgroundColor);
    on(ActionId::SaveAsBmp, [this] { saveImageAs(QStringLiteral("bmp")); });
    on(ActionId::Undo, [this] { m_editor->undo(); });
    on(ActionId::Redo, [this] { m_editor->redo(); });
    on(ActionId::Cut, [this] { copyToClipboard(m_editor->cutSelection()); });
    on(ActionId::Copy, [this] { copyToClipboard(m_editor->copySelection(false)); });
    on(ActionId::CopyVisibleLayers, [this] { copyToClipboard(m_editor->copySelection(true)); });
    on(ActionId::Paste, &MainWindow::pasteFromClipboard);
    on(ActionId::ImportFromFile, &MainWindow::importFromFile);
    on(ActionId::SelectAll, [this] { m_editor->selectAll(); });
    on(ActionId::InvertSelection, [this] { m_editor->invertSelection(); });
    on(ActionId::DeleteSelection, [this] { m_editor->deleteSelection(); });
    on(ActionId::Crop, [this] { m_editor->cropToSelection(); });
    on(ActionId::RotateRight, [this] { m_editor->rotate(RotateBy::Right90); });
    on(ActionId::RotateLeft, [this] { m_editor->rotate(RotateBy::Left90); });
    on(ActionId::Rotate180, [this] { m_editor->rotate(RotateBy::Half); });
    on(ActionId::RotateCustom, &MainWindow::showCustomRotate);
    on(ActionId::FlipVertical, [this] { m_editor->flip(Qt::Vertical); });
    on(ActionId::FlipHorizontal, [this] { m_editor->flip(Qt::Horizontal); });
    on(ActionId::ResizeSkew, &MainWindow::showResizeSkew);
    on(ActionId::InvertColors, [this] { m_editor->invertColors(); });
    on(ActionId::ImageProperties, &MainWindow::showImageProperties);
    on(ActionId::EditColors, &MainWindow::showEditColors);
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

    History &history = m_editor->history();
    const auto syncHistory = [this, &history] {
        m_actions->setHistoryAvailable(history.canUndo(), history.canRedo());
    };
    connect(history.stack(), &QUndoStack::indexChanged, this, syncHistory);
    connect(history.stack(), &QUndoStack::cleanChanged, this, syncHistory);
    connect(m_editor, &Editor::modifiedChanged, this, &MainWindow::updateWindowTitle);
    connect(m_editor, &Editor::fileChanged, this, [this] {
        updateWindowTitle();
        updateDocumentInfo();
    });
    connect(m_editor, &Editor::documentReplaced, this, [this, syncHistory] {
        syncHistory();
        updateDocumentInfo();
        updateLayerState();
        m_layers->refresh();
    });
    connect(m_editor, &Editor::documentRestructured, this, [this] {
        updateDocumentInfo();
        updateLayerState();
        m_layers->refresh();
    });
    connect(m_editor, &Editor::activeLayerChanged, this, &MainWindow::updateLayerState);
    connect(m_editor, &Editor::selectionStateChanged, this, &MainWindow::updateSelectionInfo);
    connect(m_canvas, &CanvasView::canvasResizePreview, this, [this](std::optional<QSize> size) {
        if (size) {
            m_canvasSizeLabel->setText(tr("%1 × %2px").arg(size->width()).arg(size->height()));
        } else {
            updateDocumentInfo();
        }
    });
    // The palette's custom colours are remembered between sessions.
    loadCustomColors();
    connect(m_settings, &ToolSettings::customColorsChanged, this, [this] {
        QStringList names;
        for (const QColor &color : m_settings->customColors()) {
            names.append(color.isValid() ? color.name() : QString());
        }
        QSettings().setValue(QStringLiteral("colors/custom"), names);
    });
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this,
            &MainWindow::updatePasteAvailability);
    connect(m_canvas, &CanvasView::dropped, this,
            [this](const QMimeData *mime, QPointF position, bool onCanvas) {
                // On the image: place it there. Next to it: open a dropped file like a drop on the window.
                importMimeData(mime, onCanvas ? std::optional<QPointF>(position) : std::nullopt, !onCanvas);
            });
    // Layer thumbnails follow edits, but not on every pointer move.
    auto *thumbnailTimer = new QTimer(this);
    thumbnailTimer->setSingleShot(true);
    thumbnailTimer->setInterval(150);
    connect(thumbnailTimer, &QTimer::timeout, m_layers, &LayersDock::refresh);
    connect(history.stack(), &QUndoStack::indexChanged, thumbnailTimer, qOverload<>(&QTimer::start));

    connect(m_canvas, &CanvasView::cursorPositionChanged, this, &MainWindow::setCursorPosition);
    connect(m_canvas, &CanvasView::contextMenuRequested, this, &MainWindow::showCanvasContextMenu);

    // Commands from later milestones say so instead of silently doing nothing.
    static const QSet<ActionId> implemented{
        ActionId::Quit,
        ActionId::New,
        ActionId::Open,
        ActionId::Save,
        ActionId::SaveAs,
        ActionId::SaveAsPng,
        ActionId::SaveAsJpeg,
        ActionId::SaveAsBmp,
        ActionId::SaveAsProject,
        ActionId::AddLayer,
        ActionId::DuplicateLayer,
        ActionId::DeleteLayer,
        ActionId::MergeLayerDown,
        ActionId::ToggleLayerVisibility,
        ActionId::ToggleBackground,
        ActionId::BackgroundColor,
        ActionId::Undo,
        ActionId::Redo,
        ActionId::Cut,
        ActionId::Copy,
        ActionId::CopyVisibleLayers,
        ActionId::Paste,
        ActionId::ImportFromFile,
        ActionId::SelectAll,
        ActionId::InvertSelection,
        ActionId::DeleteSelection,
        ActionId::Crop,
        ActionId::RotateRight,
        ActionId::RotateLeft,
        ActionId::Rotate180,
        ActionId::RotateCustom,
        ActionId::FlipVertical,
        ActionId::FlipHorizontal,
        ActionId::ResizeSkew,
        ActionId::InvertColors,
        ActionId::ImageProperties,
        ActionId::EditColors,
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
        QStringLiteral("action.text."),
    };
    for (QAction *action : m_actions->allActions()) {
        // Tool, brush, shape, style and text format choices are handled by the registry itself.
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

void MainWindow::updateSelectionInfo()
{
    const SelectionTool *selection = m_editor->selection();
    const bool selected = selection->hasSelection();
    m_actions->setSelectionAvailable(selected);
    QSize size = selected ? selection->bounds().size() : QSize();
    QRectF extent = m_editor->shape()->extent();
    if (extent.isEmpty()) {
        extent = m_editor->text()->extent();
    }
    if (!extent.isEmpty()) {
        // A shape or text box being drawn or edited shows its size instead.
        size = QSize(int(std::round(extent.width())), int(std::round(extent.height())));
    }
    m_selectionLabel->setText(size.isValid() ? tr("%1 × %2px").arg(size.width()).arg(size.height())
                                             : QString());
}

void MainWindow::updatePasteAvailability()
{
    m_actions->get(ActionId::Paste)
        ->setEnabled(mimeDataHasImage(QGuiApplication::clipboard()->mimeData(), true));
}

void MainWindow::copyToClipboard(const QImage &image)
{
    if (image.isNull()) {
        return;
    }
    // Qt offers the image to other applications as image/png, image/bmp and more.
    auto *mime = new QMimeData;
    mime->setImageData(image.convertToFormat(QImage::Format_ARGB32));
    QGuiApplication::clipboard()->setMimeData(mime);
}

void MainWindow::pasteFromClipboard()
{
    importMimeData(QGuiApplication::clipboard()->mimeData(), std::nullopt, false);
}

void MainWindow::importFromFile()
{
    const QString directory = m_editor->filePath().isEmpty()
                                  ? QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
                                  : QFileInfo(m_editor->filePath()).absolutePath();
    const QString path = QFileDialog::getOpenFileName(this, tr("Import to canvas"), directory,
                                                      openNameFilters().join(QStringLiteral(";;")));
    if (path.isEmpty()) {
        return;
    }
    const LoadedImage loaded = loadImage(path);
    if (!loaded.error.isEmpty()) {
        QMessageBox::warning(this, QApplication::applicationDisplayName(), loaded.error);
        return;
    }
    pasteImage(loaded.image);
}

void MainWindow::importMimeData(const QMimeData *mime, std::optional<QPointF> topLeft, bool openFiles)
{
    if (const std::optional<ImportedImage> imported = imageFromMimeData(mime)) {
        // Work outside the drop or paste handler: opening may ask about unsaved work.
        QTimer::singleShot(0, this, [this, imported = *imported, topLeft, openFiles] {
            if (openFiles && !imported.path.isEmpty()) {
                openFile(imported.path);
            } else {
                pasteImage(imported.image, topLeft);
            }
        });
        return;
    }
    if (const QUrl url = remoteImageUrl(mime); url.isValid()) {
        statusBar()->showMessage(tr("Downloading %1…").arg(url.host()));
        m_remoteImages->load(url, [this, topLeft](const QImage &image, const QString &error) {
            statusBar()->clearMessage();
            if (image.isNull()) {
                QMessageBox::warning(this, QApplication::applicationDisplayName(),
                                     tr("The image could not be downloaded: %1").arg(error));
                return;
            }
            pasteImage(image, topLeft);
        });
    }
}

bool MainWindow::pasteImage(const QImage &image, std::optional<QPointF> topLeft)
{
    if (image.isNull()) {
        return false;
    }
    // Paint pastes at the top-left corner of the visible part of the canvas.
    QPointF position = topLeft.value_or(m_canvas->viewportToDocument(QPointF(0, 0)));
    position = QPointF(std::max(0.0, std::floor(position.x())), std::max(0.0, std::floor(position.y())));
    const QSize canvas = m_editor->document().size();
    bool enlarge = false;
    if (image.width() > canvas.width() || image.height() > canvas.height()) {
        QMessageBox box(QMessageBox::Question, QApplication::applicationDisplayName(),
                        tr("The image you're pasting is larger than the canvas."), QMessageBox::NoButton,
                        this);
        box.setObjectName(QStringLiteral("dialog.enlargeCanvas"));
        box.setInformativeText(tr("Do you want to enlarge the canvas to fit it?"));
        QPushButton *grow = box.addButton(tr("&Enlarge canvas"), QMessageBox::AcceptRole);
        QPushButton *keep = box.addButton(tr("&Keep canvas size"), QMessageBox::NoRole);
        box.addButton(QMessageBox::Cancel);
        box.setDefaultButton(grow);
        box.exec();
        if (box.clickedButton() != grow && box.clickedButton() != keep) {
            return false;
        }
        enlarge = box.clickedButton() == grow;
        if (enlarge && !topLeft) {
            position = QPointF(0, 0);
        }
    }
    m_editor->paste(image, position, enlarge);
    return true;
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (mimeDataHasImage(event->mimeData(), true)) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    importMimeData(event->mimeData(), std::nullopt, true);
    event->acceptProposedAction();
}

void MainWindow::loadCustomColors()
{
    const QStringList names = QSettings().value(QStringLiteral("colors/custom")).toStringList();
    for (int i = 0; i < std::min<int>(int(names.size()), ToolSettings::CustomColorCount); ++i) {
        m_settings->setCustomColor(i, names.at(i).isEmpty() ? QColor() : QColor(names.at(i)));
    }
}

void MainWindow::showResizeSkew()
{
    ResizeSkewDialog dialog(m_editor->operationSize(), this);
    if (dialog.exec() == QDialog::Accepted && dialog.changesAnything()) {
        m_editor->resizeAndSkew(dialog.resultSize(), dialog.skewHorizontal(), dialog.skewVertical());
    }
}

void MainWindow::showCustomRotate()
{
    CustomRotateDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted && dialog.angle() != 0) {
        m_editor->rotate(dialog.angle());
    }
}

void MainWindow::showImageProperties()
{
    const QString path = m_editor->filePath();
    const ImagePropertiesDialog::Facts facts{
        m_editor->document().size(),
        m_editor->document().dotsPerMeter(),
        path.isEmpty() ? QDateTime() : QFileInfo(path).lastModified(),
        m_editor->fileSize(),
        Editor::DefaultSize,
    };
    ImagePropertiesDialog dialog(facts, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (dialog.resultSize() != facts.size) {
        m_editor->resizeCanvas(QRect(QPoint(0, 0), dialog.resultSize()));
    }
    if (dialog.blackAndWhite()) {
        m_editor->convertToBlackAndWhite();
    }
}

std::optional<QColor> MainWindow::pickColor(const QColor &initial)
{
    QSettings settings;
    QList<QColor> custom;
    for (const QString &name : settings.value(QStringLiteral("editColors/custom")).toStringList()) {
        custom.append(name.isEmpty() ? QColor() : QColor(name));
    }
    EditColorsDialog dialog(initial, custom, this);
    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }
    QStringList names;
    for (const QColor &color : dialog.customColors()) {
        names.append(color.isValid() ? color.name() : QString());
    }
    settings.setValue(QStringLiteral("editColors/custom"), names);
    return dialog.color();
}

void MainWindow::showEditColors()
{
    const ColorSlot slot = m_settings->activeColorSlot();
    const std::optional<QColor> color = pickColor(m_settings->color(slot));
    if (!color) {
        return;
    }
    // Like Paint: the colour goes to the active slot and into the palette's custom row.
    m_settings->setColor(slot, *color);
    if (!m_settings->customColors().contains(*color)) {
        m_settings->addCustomColor(*color);
    }
}

void MainWindow::showBackgroundColor()
{
    if (const std::optional<QColor> color = pickColor(m_editor->document().backgroundColor())) {
        m_editor->setBackgroundColor(*color);
    }
}

void MainWindow::updateLayerState()
{
    const Document &document = m_editor->document();
    m_actions->setLayerState(document.layerCount(), document.activeLayerIndex(),
                             document.layer(document.activeLayerIndex()).visible,
                             document.isBackgroundVisible());
}

void MainWindow::newImage()
{
    if (maybeSave()) {
        m_editor->newDocument();
    }
}

void MainWindow::openImage()
{
    if (!maybeSave()) {
        return;
    }
    const QString directory = m_editor->filePath().isEmpty()
                                  ? QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
                                  : QFileInfo(m_editor->filePath()).absolutePath();
    const QString path = QFileDialog::getOpenFileName(this, tr("Open"), directory,
                                                      openNameFilters().join(QStringLiteral(";;")));
    if (!path.isEmpty()) {
        openFile(path);
    }
}

bool MainWindow::openFile(const QString &path)
{
    if (!maybeSave()) {
        return false;
    }
    if (const QString error = m_editor->open(path); !error.isEmpty()) {
        QMessageBox::warning(this, QApplication::applicationDisplayName(), error);
        return false;
    }
    return true;
}

bool MainWindow::saveImage()
{
    const QString path = m_editor->filePath();
    if (const std::optional<SaveFormat> format = saveFormatForPath(path); !path.isEmpty() && format) {
        return saveFile(path, *format);
    }
    // Never saved, or opened from a format Paint does not save to: ask where, as PNG.
    return saveImageAs();
}

bool MainWindow::saveImageAs(const QString &suffix)
{
    QStringList filters;
    for (const SaveFormat &format : saveFormats()) {
        filters.append(format.nameFilter());
    }
    const SaveFormat initial = saveFormatForSuffix(suffix).value_or(saveFormats().first());
    QString selected = initial.nameFilter();
    const QString current = m_editor->filePath();
    const QString directory = current.isEmpty()
                                  ? QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
                                  : QFileInfo(current).absolutePath();
    const QString baseName = current.isEmpty() ? tr("Untitled") : QFileInfo(current).completeBaseName();
    QString path = QFileDialog::getSaveFileName(this, tr("Save as"),
                                                directory + QLatin1Char('/') + baseName + QLatin1Char('.') +
                                                    initial.suffixes.first(),
                                                filters.join(QStringLiteral(";;")), &selected);
    if (path.isEmpty()) {
        return false;
    }
    // Without a known suffix, use the chosen filter's format and add its suffix.
    std::optional<SaveFormat> format = saveFormatForPath(path);
    if (!format) {
        for (const SaveFormat &candidate : saveFormats()) {
            if (candidate.nameFilter() == selected) {
                format = candidate;
            }
        }
        format = format ? format : saveFormats().first();
        path += QLatin1Char('.') + format->suffixes.first();
    }
    return saveFile(path, *format);
}

bool MainWindow::saveProjectAs()
{
    const QString current = m_editor->filePath();
    const QString directory = current.isEmpty()
                                  ? QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
                                  : QFileInfo(current).absolutePath();
    const QString baseName = current.isEmpty() ? tr("Untitled") : QFileInfo(current).completeBaseName();
    QString path = QFileDialog::getSaveFileName(
        this, tr("Save as project"), directory + QLatin1Char('/') + baseName + QStringLiteral(".ora"),
        projectFormat().nameFilter());
    if (path.isEmpty()) {
        return false;
    }
    if (!isOpenRasterPath(path)) {
        path += QStringLiteral(".ora");
    }
    return saveFile(path, projectFormat());
}

bool MainWindow::saveFile(const QString &path)
{
    const std::optional<SaveFormat> format = saveFormatForPath(path);
    if (!format) {
        QMessageBox::warning(this, QApplication::applicationDisplayName(),
                             tr("Paint for Linux can't save files of this type."));
        return false;
    }
    return saveFile(path, *format);
}

bool MainWindow::saveFile(const QString &path, const SaveFormat &format)
{
    if (const QString error = m_editor->save(path, format); !error.isEmpty()) {
        QMessageBox::warning(this, QApplication::applicationDisplayName(), error);
        return false;
    }
    return true;
}

bool MainWindow::maybeSave()
{
    // A shape, text box or moved selection still being edited is part of the work.
    m_editor->commitTool();
    if (!m_editor->isModified()) {
        return true;
    }
    QMessageBox box(QMessageBox::Question, QApplication::applicationDisplayName(),
                    tr("Do you want to save your work?"), QMessageBox::NoButton, this);
    box.setObjectName(QStringLiteral("dialog.saveChanges"));
    QPushButton *save = box.addButton(QMessageBox::Save);
    QPushButton *dontSave = box.addButton(tr("Do&n't save"), QMessageBox::DestructiveRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(save);
    box.exec();
    if (box.clickedButton() == save) {
        return saveImage();
    }
    return box.clickedButton() == dontSave;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) {
        event->accept();
    } else {
        event->ignore();
    }
}

QString MainWindow::documentPixel(int x, int y) const
{
    const QImage pixel = m_editor->document().composite(QRect(x, y, 1, 1));
    return pixel.isNull() ? QString() : pixel.pixelColor(0, 0).name(QColor::HexArgb);
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
    const Document &document = m_editor->document();
    QJsonArray layers;
    for (qsizetype i = 0; i < document.layerCount(); ++i) {
        const paint::Layer &layer = document.layer(i);
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
        {QStringLiteral("canvasSize"), QJsonArray{document.size().width(), document.size().height()}},
        {QStringLiteral("backgroundColor"), document.backgroundColor().name(QColor::HexArgb)},
        {QStringLiteral("backgroundVisible"), document.isBackgroundVisible()},
        {QStringLiteral("activeLayer"), document.activeLayerIndex()},
        {QStringLiteral("layers"), layers},
        {QStringLiteral("modified"), m_editor->isModified()},
        {QStringLiteral("filePath"), m_editor->filePath()},
        {QStringLiteral("canUndo"), m_editor->history().canUndo()},
        {QStringLiteral("canRedo"), m_editor->history().canRedo()},
        {QStringLiteral("selection"),
         m_editor->selection()->hasSelection()
             ? QJsonValue(QJsonArray{m_editor->selection()->bounds().x(), m_editor->selection()->bounds().y(),
                                     m_editor->selection()->bounds().width(),
                                     m_editor->selection()->bounds().height()})
             : QJsonValue()},
        {QStringLiteral("floating"), m_editor->selection()->isFloating()},
        {QStringLiteral("shapeEditing"), m_editor->shape()->isEditing()},
        {QStringLiteral("textEditing"), m_editor->text()->isEditing()},
        {QStringLiteral("text"), m_editor->text()->text()},
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
        {QStringLiteral("backgroundVisible"), m_editor->document().isBackgroundVisible()},
    };
}
