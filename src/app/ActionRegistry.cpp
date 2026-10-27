#include "ActionRegistry.h"

#include "tools/ToolSettings.h"
#include "ui/Icons.h"
#include "ui/Names.h"

#include <QAction>
#include <QActionGroup>
#include <QCoreApplication>
#include <QWidget>

using namespace paint;

namespace {

struct ActionSpec
{
    ActionId id;
    const char *name; // object name without the "action." prefix
    const char *text;
    // "name" is a Paint icon, "@theme" a system theme icon, "@theme|name" a theme icon with a
    // Paint fallback. Standard commands use theme icons so menus look like other Linux apps.
    const char *icon = "";
    QList<QKeySequence> shortcuts = {};
    bool checkable = false;
};

QKeySequence keys(QKeyCombination combination)
{
    return QKeySequence(combination);
}

QList<QKeySequence> standard(QKeySequence::StandardKey key)
{
    return QKeySequence::keyBindings(key);
}

QList<ActionSpec> actionSpecs()
{
    using Q = QT_PREPEND_NAMESPACE(Qt::Key);
    const auto ctrl = [](Q key) { return keys(Qt::CTRL | key); };
    const auto ctrlShift = [](Q key) { return keys(Qt::CTRL | Qt::SHIFT | key); };
    return {
        // File
        {ActionId::New, "file.new", QT_TRANSLATE_NOOP("ActionRegistry", "&New"), "@document-new",
         standard(QKeySequence::New)},
        {ActionId::Open, "file.open", QT_TRANSLATE_NOOP("ActionRegistry", "&Open…"), "@document-open",
         standard(QKeySequence::Open)},
        {ActionId::ImportFromFile, "file.importFromFile",
         QT_TRANSLATE_NOOP("ActionRegistry", "&From a file…"), "import"},
        {ActionId::Save, "file.save", QT_TRANSLATE_NOOP("ActionRegistry", "&Save"), "@document-save|save",
         standard(QKeySequence::Save)},
        // Paint's "Save as ▸ Other format…" is the full save dialog (F12).
        {ActionId::SaveAs,
         "file.saveAs",
         QT_TRANSLATE_NOOP("ActionRegistry", "&Other format…"),
         "@document-save-as",
         {keys(Qt::Key_F12), ctrlShift(Qt::Key_S)}},
        {ActionId::SaveAsPng, "file.saveAsPng", QT_TRANSLATE_NOOP("ActionRegistry", "&PNG picture")},
        {ActionId::SaveAsJpeg, "file.saveAsJpeg", QT_TRANSLATE_NOOP("ActionRegistry", "&JPEG picture")},
        {ActionId::SaveAsBmp, "file.saveAsBmp", QT_TRANSLATE_NOOP("ActionRegistry", "&BMP picture")},
        {ActionId::SaveAsGif, "file.saveAsGif", QT_TRANSLATE_NOOP("ActionRegistry", "&GIF picture")},
        {ActionId::SaveAsProject, "file.saveAsProject",
         QT_TRANSLATE_NOOP("ActionRegistry", "Save as pro&ject…")},
        {ActionId::Print, "file.print", QT_TRANSLATE_NOOP("ActionRegistry", "&Print…"),
         "@document-print|print", standard(QKeySequence::Print)},
        {ActionId::PageSetup, "file.pageSetup", QT_TRANSLATE_NOOP("ActionRegistry", "Page &setup…"),
         "@document-page-setup"},
        {ActionId::PrintPreview, "file.printPreview", QT_TRANSLATE_NOOP("ActionRegistry", "Print pre&view"),
         "@document-print-preview"},
        {ActionId::WallpaperFill, "file.wallpaperFill", QT_TRANSLATE_NOOP("ActionRegistry", "&Fill")},
        {ActionId::WallpaperTile, "file.wallpaperTile", QT_TRANSLATE_NOOP("ActionRegistry", "&Tile")},
        {ActionId::WallpaperCenter, "file.wallpaperCenter", QT_TRANSLATE_NOOP("ActionRegistry", "&Center")},
        {ActionId::ImageProperties,
         "file.imageProperties",
         QT_TRANSLATE_NOOP("ActionRegistry", "Image p&roperties…"),
         "image-properties",
         {ctrl(Qt::Key_E)}},
        {ActionId::Quit, "file.quit", QT_TRANSLATE_NOOP("ActionRegistry", "&Quit"), "@application-exit",
         standard(QKeySequence::Quit)},
        // Edit
        {ActionId::Undo,
         "edit.undo",
         QT_TRANSLATE_NOOP("ActionRegistry", "&Undo"),
         "@edit-undo|undo",
         {ctrl(Qt::Key_Z)}},
        {ActionId::Redo,
         "edit.redo",
         QT_TRANSLATE_NOOP("ActionRegistry", "&Redo"),
         "@edit-redo|redo",
         {ctrl(Qt::Key_Y), ctrlShift(Qt::Key_Z)}},
        {ActionId::Cut,
         "edit.cut",
         QT_TRANSLATE_NOOP("ActionRegistry", "Cu&t"),
         "@edit-cut",
         {ctrl(Qt::Key_X)}},
        {ActionId::Copy,
         "edit.copy",
         QT_TRANSLATE_NOOP("ActionRegistry", "&Copy"),
         "@edit-copy",
         {ctrl(Qt::Key_C)}},
        {ActionId::CopyVisibleLayers,
         "edit.copyVisibleLayers",
         QT_TRANSLATE_NOOP("ActionRegistry", "Copy &visible layers"),
         "",
         {ctrlShift(Qt::Key_C)}},
        {ActionId::Paste,
         "edit.paste",
         QT_TRANSLATE_NOOP("ActionRegistry", "&Paste"),
         "@edit-paste",
         {ctrl(Qt::Key_V)}},
        {ActionId::SelectAll,
         "edit.selectAll",
         QT_TRANSLATE_NOOP("ActionRegistry", "Select &all"),
         "@edit-select-all|select-all",
         {ctrl(Qt::Key_A)}},
        {ActionId::InvertSelection, "edit.invertSelection",
         QT_TRANSLATE_NOOP("ActionRegistry", "&Invert selection"), "select-invert"},
        {ActionId::DeleteSelection,
         "edit.delete",
         QT_TRANSLATE_NOOP("ActionRegistry", "&Delete"),
         "@edit-delete|delete",
         {keys(Qt::Key_Delete)}},
        {ActionId::TransparentSelection,
         "edit.transparentSelection",
         QT_TRANSLATE_NOOP("ActionRegistry", "&Transparent selection"),
         "",
         {},
         true},
        // Image
        {ActionId::Crop,
         "image.crop",
         QT_TRANSLATE_NOOP("ActionRegistry", "C&rop"),
         "crop",
         {ctrlShift(Qt::Key_X)}},
        {ActionId::ResizeSkew,
         "image.resizeSkew",
         QT_TRANSLATE_NOOP("ActionRegistry", "Re&size and skew…"),
         "resize",
         {ctrl(Qt::Key_W)}},
        {ActionId::RotateRight, "image.rotateRight", QT_TRANSLATE_NOOP("ActionRegistry", "Rotate &right 90°"),
         "rotate-right"},
        {ActionId::RotateLeft, "image.rotateLeft", QT_TRANSLATE_NOOP("ActionRegistry", "Rotate &left 90°"),
         "rotate-left"},
        {ActionId::Rotate180, "image.rotate180", QT_TRANSLATE_NOOP("ActionRegistry", "Rotate &180°"),
         "rotate-180"},
        {ActionId::RotateCustom, "image.rotateCustom", QT_TRANSLATE_NOOP("ActionRegistry", "&Custom rotate…"),
         "rotate-custom"},
        {ActionId::FlipVertical, "image.flipVertical", QT_TRANSLATE_NOOP("ActionRegistry", "Flip &vertical"),
         "flip-vertical"},
        {ActionId::FlipHorizontal, "image.flipHorizontal",
         QT_TRANSLATE_NOOP("ActionRegistry", "Flip &horizontal"), "flip-horizontal"},
        {ActionId::InvertColors,
         "image.invertColors",
         QT_TRANSLATE_NOOP("ActionRegistry", "&Invert colors"),
         "invert-colors",
         {ctrlShift(Qt::Key_I)}},
        // View
        {ActionId::ZoomIn,
         "view.zoomIn",
         QT_TRANSLATE_NOOP("ActionRegistry", "Zoom &in"),
         "@zoom-in|zoom-in",
         {ctrl(Qt::Key_PageUp)}},
        {ActionId::ZoomOut,
         "view.zoomOut",
         QT_TRANSLATE_NOOP("ActionRegistry", "Zoom &out"),
         "@zoom-out|zoom-out",
         {ctrl(Qt::Key_PageDown)}},
        {ActionId::ZoomActualSize,
         "view.zoomActualSize",
         QT_TRANSLATE_NOOP("ActionRegistry", "&100%"),
         "@zoom-original",
         {ctrl(Qt::Key_0)}},
        {ActionId::ZoomFit,
         "view.zoomFit",
         QT_TRANSLATE_NOOP("ActionRegistry", "&Fit to window"),
         "@zoom-fit-best|fit-to-window",
         {ctrl(Qt::Key_1)}},
        {ActionId::Rulers,
         "view.rulers",
         QT_TRANSLATE_NOOP("ActionRegistry", "&Rulers"),
         "rulers",
         {ctrl(Qt::Key_R)},
         true},
        {ActionId::Gridlines,
         "view.gridlines",
         QT_TRANSLATE_NOOP("ActionRegistry", "&Gridlines"),
         "gridlines",
         {ctrl(Qt::Key_G)},
         true},
        {ActionId::StatusBar,
         "view.statusBar",
         QT_TRANSLATE_NOOP("ActionRegistry", "&Status bar"),
         "",
         {},
         true},
        {ActionId::ToolbarAlwaysShow,
         "view.toolbarAlwaysShow",
         QT_TRANSLATE_NOOP("ActionRegistry", "&Always show"),
         "",
         {},
         true},
        {ActionId::ToolbarAutoHide,
         "view.toolbarAutoHide",
         QT_TRANSLATE_NOOP("ActionRegistry", "Automatically &hide"),
         "",
         {},
         true},
        {ActionId::LayersPanel,
         "view.layersPanel",
         QT_TRANSLATE_NOOP("ActionRegistry", "&Layers"),
         "layers",
         {},
         true},
        {ActionId::FullScreen,
         "view.fullScreen",
         QT_TRANSLATE_NOOP("ActionRegistry", "F&ull screen"),
         "@view-fullscreen|full-screen",
         {keys(Qt::Key_F11)},
         true},
        {ActionId::Thumbnail,
         "view.thumbnail",
         QT_TRANSLATE_NOOP("ActionRegistry", "&Thumbnail"),
         "thumbnail",
         {},
         true},
        // Layers
        {ActionId::AddLayer,
         "layer.add",
         QT_TRANSLATE_NOOP("ActionRegistry", "&Add layer"),
         "layer-add",
         {ctrlShift(Qt::Key_N)}},
        {ActionId::DuplicateLayer, "layer.duplicate", QT_TRANSLATE_NOOP("ActionRegistry", "D&uplicate layer"),
         "layer-duplicate"},
        {ActionId::DeleteLayer, "layer.delete", QT_TRANSLATE_NOOP("ActionRegistry", "&Delete layer"),
         "delete"},
        {ActionId::MergeLayerDown, "layer.mergeDown", QT_TRANSLATE_NOOP("ActionRegistry", "&Merge down"),
         "layer-merge-down"},
        {ActionId::ToggleLayerVisibility, "layer.toggleVisibility",
         QT_TRANSLATE_NOOP("ActionRegistry", "&Hide layer"), "layer-hidden"},
        {ActionId::ToggleBackground,
         "layer.toggleBackground",
         QT_TRANSLATE_NOOP("ActionRegistry", "Hide &background"),
         "layer-hidden",
         {ctrlShift(Qt::Key_H)}},
        {ActionId::BackgroundColor,
         "layer.backgroundColor",
         QT_TRANSLATE_NOOP("ActionRegistry", "Change background &color…"),
         "edit-colors",
         {ctrlShift(Qt::Key_B)}},
        // Sizes and colours
        {ActionId::CycleSelectionTool,
         "tool.cycleSelection",
         QT_TRANSLATE_NOOP("ActionRegistry", "Switch selection tool"),
         "",
         {keys(Qt::Key_S)}},
        {ActionId::SizeIncrease,
         "tool.sizeIncrease",
         QT_TRANSLATE_NOOP("ActionRegistry", "&Increase size"),
         "",
         {ctrl(Qt::Key_Plus), ctrl(Qt::Key_Equal)}},
        {ActionId::SizeDecrease,
         "tool.sizeDecrease",
         QT_TRANSLATE_NOOP("ActionRegistry", "&Decrease size"),
         "",
         {ctrl(Qt::Key_Minus)}},
        {ActionId::EditColors, "color.edit", QT_TRANSLATE_NOOP("ActionRegistry", "&Edit colors…"),
         "edit-colors"},
        // Text format bar
        {ActionId::TextBold,
         "text.bold",
         QT_TRANSLATE_NOOP("ActionRegistry", "Bold"),
         "text-bold",
         {ctrl(Qt::Key_B)},
         true},
        {ActionId::TextItalic,
         "text.italic",
         QT_TRANSLATE_NOOP("ActionRegistry", "Italic"),
         "text-italic",
         {ctrl(Qt::Key_I)},
         true},
        {ActionId::TextUnderline,
         "text.underline",
         QT_TRANSLATE_NOOP("ActionRegistry", "Underline"),
         "text-underline",
         {ctrl(Qt::Key_U)},
         true},
        {ActionId::TextStrikethrough,
         "text.strikethrough",
         QT_TRANSLATE_NOOP("ActionRegistry", "Strikethrough"),
         "text-strikethrough",
         {},
         true},
        {ActionId::TextAlignLeft,
         "text.alignLeft",
         QT_TRANSLATE_NOOP("ActionRegistry", "Align left"),
         "text-align-left",
         {},
         true},
        {ActionId::TextAlignCenter,
         "text.alignCenter",
         QT_TRANSLATE_NOOP("ActionRegistry", "Center"),
         "text-align-center",
         {},
         true},
        {ActionId::TextAlignRight,
         "text.alignRight",
         QT_TRANSLATE_NOOP("ActionRegistry", "Align right"),
         "text-align-right",
         {},
         true},
        {ActionId::TextBackground,
         "text.background",
         QT_TRANSLATE_NOOP("ActionRegistry", "Fill background"),
         "shape-fill",
         {},
         true},
        // Help
        {ActionId::About, "help.about", QT_TRANSLATE_NOOP("ActionRegistry", "&About Paint for Linux"),
         "@help-about"},
        {ActionId::AboutQt, "help.aboutQt", QT_TRANSLATE_NOOP("ActionRegistry", "About &Qt"), "@qtlogo"},
    };
}

QIcon resolveIcon(QLatin1StringView spec)
{
    if (spec.isEmpty()) {
        return {};
    }
    if (!spec.startsWith(QLatin1Char('@'))) {
        return ui::icon(spec.toString());
    }
    const QString text = spec.sliced(1).toString();
    const qsizetype bar = text.indexOf(QLatin1Char('|'));
    const QString themeName = bar < 0 ? text : text.left(bar);
    const QIcon fallback = bar < 0 ? QIcon() : ui::icon(text.mid(bar + 1));
    return QIcon::fromTheme(themeName, fallback);
}

struct ToolSpec
{
    Tool tool;
    ActionId id;
    const char *name;
    const char *icon;
    Qt::Key key;
};

constexpr ToolSpec ToolSpecs[] = {
    {Tool::RectangleSelect, ActionId::ToolRectangleSelect, "tool.rectangleSelect", "select-rectangle",
     Qt::Key_unknown},
    {Tool::FreeformSelect, ActionId::ToolFreeformSelect, "tool.freeformSelect", "select-freeform",
     Qt::Key_unknown},
    {Tool::Pencil, ActionId::ToolPencil, "tool.pencil", "pencil", Qt::Key_P},
    {Tool::Fill, ActionId::ToolFill, "tool.fill", "fill", Qt::Key_B},
    {Tool::Text, ActionId::ToolText, "tool.text", "text", Qt::Key_T},
    {Tool::Eraser, ActionId::ToolEraser, "tool.eraser", "eraser", Qt::Key_E},
    {Tool::ColorPicker, ActionId::ToolColorPicker, "tool.colorPicker", "color-picker", Qt::Key_I},
    {Tool::Magnifier, ActionId::ToolMagnifier, "tool.magnifier", "magnifier", Qt::Key_Z},
    {Tool::Brush, ActionId::ToolBrush, "tool.brush", "brushes", Qt::Key_unknown},
    {Tool::Shape, ActionId::ToolShape, "tool.shape", "shapes", Qt::Key_unknown},
};

// Stable identifiers for object names ("action.shape.fivePointStar").
QString identifier(const QString &displayName)
{
    QString result;
    bool upper = false;
    for (const QChar c : displayName) {
        if (!c.isLetterOrNumber()) {
            upper = !result.isEmpty();
            continue;
        }
        result.append(upper ? c.toUpper() : (result.isEmpty() ? c.toLower() : c));
        upper = false;
    }
    return result;
}

constexpr int ShapeSizes[] = {1, 3, 5, 8};

} // namespace

ActionRegistry::ActionRegistry(ToolSettings *settings, QWidget *window)
    : QObject(window)
    , m_settings(settings)
{
    createActions(window);
    createToolActions(window);
    connectSettings();
}

void ActionRegistry::createActions(QWidget *window)
{
    for (const ActionSpec &spec : actionSpecs()) {
        auto *action = new QAction(resolveIcon(QLatin1StringView(spec.icon)),
                                   QCoreApplication::translate("ActionRegistry", spec.text), window);
        action->setObjectName(QStringLiteral("action.") + QLatin1StringView(spec.name));
        action->setShortcuts(spec.shortcuts);
        action->setCheckable(spec.checkable);
        // Shortcuts work even for actions that are not in any visible menu.
        window->addAction(action);
        m_actions.insert(spec.id, action);
    }

    get(ActionId::Quit)->setMenuRole(QAction::QuitRole);
    get(ActionId::About)->setMenuRole(QAction::AboutRole);
    get(ActionId::AboutQt)->setMenuRole(QAction::AboutQtRole);
    get(ActionId::StatusBar)->setChecked(true);

    auto *toolbarMode = new QActionGroup(this);
    toolbarMode->addAction(get(ActionId::ToolbarAlwaysShow));
    toolbarMode->addAction(get(ActionId::ToolbarAutoHide));
    get(ActionId::ToolbarAlwaysShow)->setChecked(true);

    // Commands that need a selection or history start disabled, as in a fresh Paint window.
    setSelectionAvailable(false);
    setHistoryAvailable(false, false);
    setLayerState(1, 0, true, true);
}

void ActionRegistry::createToolActions(QWidget *window)
{
    m_toolGroup = new QActionGroup(this);
    for (const ToolSpec &spec : ToolSpecs) {
        auto *action =
            new QAction(ui::icon(QLatin1StringView(spec.icon).toString()), ui::toolName(spec.tool), window);
        action->setObjectName(QStringLiteral("action.") + QLatin1StringView(spec.name));
        action->setCheckable(true);
        action->setData(QVariant::fromValue(spec.tool));
        if (spec.key != Qt::Key_unknown) {
            action->setShortcut(QKeySequence(spec.key));
            action->setToolTip(QStringLiteral("%1 (%2)").arg(
                action->text(), action->shortcut().toString(QKeySequence::NativeText)));
        }
        m_toolGroup->addAction(action);
        window->addAction(action);
        m_actions.insert(spec.id, action);
        connect(action, &QAction::triggered, this, [this, tool = spec.tool] { m_settings->setTool(tool); });
    }

    auto *brushGroup = new QActionGroup(this);
    for (BrushType type : AllBrushTypes) {
        auto *action = new QAction(ui::brushName(type), window);
        action->setObjectName(QStringLiteral("action.brush.") + identifier(ui::brushName(type)));
        action->setCheckable(true);
        brushGroup->addAction(action);
        m_brushActions.insert(type, action);
        connect(action, &QAction::triggered, this, [this, type] {
            m_settings->setBrushType(type);
            m_settings->setTool(Tool::Brush);
        });
    }

    m_shapeGroup = new QActionGroup(this);
    m_shapeGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    for (ShapeType type : AllShapeTypes) {
        auto *action = new QAction(ui::shapeIcon(type), ui::shapeName(type), window);
        action->setObjectName(QStringLiteral("action.shape.") + identifier(ui::shapeName(type)));
        action->setCheckable(true);
        m_shapeGroup->addAction(action);
        m_shapeActions.insert(type, action);
        connect(action, &QAction::triggered, this, [this, type] {
            m_settings->setShapeType(type);
            m_settings->setTool(Tool::Shape);
            syncTool(Tool::Shape);
        });
    }

    auto *outlineGroup = new QActionGroup(this);
    auto *fillGroup = new QActionGroup(this);
    for (StrokeStyle style : AllStrokeStyles) {
        auto *outline = new QAction(ui::outlineStyleName(style), window);
        outline->setObjectName(QStringLiteral("action.outline.") + identifier(ui::outlineStyleName(style)));
        outline->setCheckable(true);
        outlineGroup->addAction(outline);
        m_outlineActions.insert(style, outline);
        connect(outline, &QAction::triggered, this, [this, style] { m_settings->setOutlineStyle(style); });

        auto *fill = new QAction(ui::fillStyleName(style), window);
        fill->setObjectName(QStringLiteral("action.fill.") + identifier(ui::fillStyleName(style)));
        fill->setCheckable(true);
        fillGroup->addAction(fill);
        m_fillActions.insert(style, fill);
        connect(fill, &QAction::triggered, this, [this, style] { m_settings->setFillStyle(style); });
    }

    auto *sizeGroup = new QActionGroup(this);
    sizeGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    for (int size : ShapeSizes) {
        auto *action = new QAction(tr("%1 px").arg(size), window);
        action->setObjectName(QStringLiteral("action.shapeSize.%1").arg(size));
        action->setCheckable(true);
        action->setData(size);
        sizeGroup->addAction(action);
        m_shapeSizeActions.append(action);
        connect(action, &QAction::triggered, this, [this, size] { m_settings->setSize(size); });
    }

    connect(get(ActionId::CycleSelectionTool), &QAction::triggered, this, [this] {
        m_settings->setTool(m_settings->tool() == Tool::RectangleSelect  ? Tool::FreeformSelect
                            : m_settings->tool() == Tool::FreeformSelect ? Tool::RectangleSelect
                                                                         : m_settings->selectionTool());
    });
    connect(get(ActionId::SizeIncrease), &QAction::triggered, this,
            [this] { m_settings->setSize(m_settings->size() + 1); });
    connect(get(ActionId::SizeDecrease), &QAction::triggered, this,
            [this] { m_settings->setSize(m_settings->size() - 1); });
    connect(get(ActionId::TransparentSelection), &QAction::toggled, m_settings,
            &ToolSettings::setTransparentSelection);

    // Text formatting changes the settings; the text tool applies them to the text being typed.
    const auto formatToggle = [this](ActionId id, bool TextFormat::*property) {
        connect(get(id), &QAction::triggered, this, [this, property](bool checked) {
            TextFormat format = m_settings->textFormat();
            format.*property = checked;
            m_settings->setTextFormat(format);
        });
    };
    formatToggle(ActionId::TextBold, &TextFormat::bold);
    formatToggle(ActionId::TextItalic, &TextFormat::italic);
    formatToggle(ActionId::TextUnderline, &TextFormat::underline);
    formatToggle(ActionId::TextStrikethrough, &TextFormat::strikeOut);
    auto *alignment = new QActionGroup(this);
    for (const auto &[id, flag] : {std::pair{ActionId::TextAlignLeft, Qt::AlignLeft},
                                   {ActionId::TextAlignCenter, Qt::AlignHCenter},
                                   {ActionId::TextAlignRight, Qt::AlignRight}}) {
        alignment->addAction(get(id));
        connect(get(id), &QAction::triggered, this, [this, flag] { m_settings->setTextAlignment(flag); });
    }
    connect(get(ActionId::TextBackground), &QAction::triggered, m_settings, &ToolSettings::setTextBackground);
}

void ActionRegistry::connectSettings()
{
    connect(m_settings, &ToolSettings::toolChanged, this, &ActionRegistry::syncTool);
    connect(m_settings, &ToolSettings::brushTypeChanged, this,
            [this](BrushType type) { m_brushActions.value(type)->setChecked(true); });
    connect(m_settings, &ToolSettings::shapeTypeChanged, this, [this] { syncTool(m_settings->tool()); });
    connect(m_settings, &ToolSettings::outlineStyleChanged, this,
            [this](StrokeStyle style) { m_outlineActions.value(style)->setChecked(true); });
    connect(m_settings, &ToolSettings::fillStyleChanged, this,
            [this](StrokeStyle style) { m_fillActions.value(style)->setChecked(true); });
    connect(m_settings, &ToolSettings::sizeChanged, this, [this] { syncTool(m_settings->tool()); });
    connect(m_settings, &ToolSettings::transparentSelectionChanged, get(ActionId::TransparentSelection),
            &QAction::setChecked);
    connect(m_settings, &ToolSettings::textFormatChanged, this, &ActionRegistry::syncTextFormat);
    connect(m_settings, &ToolSettings::textAlignmentChanged, this, &ActionRegistry::syncTextFormat);
    connect(m_settings, &ToolSettings::textBackgroundChanged, this, &ActionRegistry::syncTextFormat);
    syncTextFormat();

    m_brushActions.value(m_settings->brushType())->setChecked(true);
    m_outlineActions.value(m_settings->outlineStyle())->setChecked(true);
    m_fillActions.value(m_settings->fillStyle())->setChecked(true);
    syncTool(m_settings->tool());
}

void ActionRegistry::syncTool(Tool tool)
{
    toolAction(tool)->setChecked(true);

    // Paint highlights the current shape, and enables the shape options, only while drawing shapes.
    const bool shapes = tool == Tool::Shape;
    for (auto it = m_shapeActions.cbegin(); it != m_shapeActions.cend(); ++it) {
        it.value()->setChecked(shapes && it.key() == m_settings->shapeType());
    }
    for (QAction *action : std::as_const(m_outlineActions)) {
        action->setEnabled(shapes);
    }
    for (QAction *action : std::as_const(m_fillActions)) {
        action->setEnabled(shapes);
    }
    for (QAction *action : std::as_const(m_shapeSizeActions)) {
        action->setEnabled(shapes);
        action->setChecked(action->data().toInt() == m_settings->sizeFor(Tool::Shape));
    }
    get(ActionId::SizeIncrease)->setEnabled(toolUsesSize(tool));
    get(ActionId::SizeDecrease)->setEnabled(toolUsesSize(tool));
    // Text formatting (and its Ctrl+B/I/U) only while the text tool is active.
    for (ActionId id : {ActionId::TextBold, ActionId::TextItalic, ActionId::TextUnderline,
                        ActionId::TextStrikethrough, ActionId::TextAlignLeft, ActionId::TextAlignCenter,
                        ActionId::TextAlignRight, ActionId::TextBackground}) {
        get(id)->setEnabled(tool == Tool::Text);
    }
}

void ActionRegistry::syncTextFormat()
{
    const TextFormat format = m_settings->textFormat();
    get(ActionId::TextBold)->setChecked(format.bold);
    get(ActionId::TextItalic)->setChecked(format.italic);
    get(ActionId::TextUnderline)->setChecked(format.underline);
    get(ActionId::TextStrikethrough)->setChecked(format.strikeOut);
    const Qt::Alignment alignment = m_settings->textAlignment();
    get(alignment & Qt::AlignHCenter ? ActionId::TextAlignCenter
        : alignment & Qt::AlignRight ? ActionId::TextAlignRight
                                     : ActionId::TextAlignLeft)
        ->setChecked(true);
    get(ActionId::TextBackground)->setChecked(m_settings->textBackground());
}

QAction *ActionRegistry::toolAction(Tool tool) const
{
    for (const ToolSpec &spec : ToolSpecs) {
        if (spec.tool == tool) {
            return get(spec.id);
        }
    }
    return nullptr;
}

QList<QAction *> ActionRegistry::allActions() const
{
    QList<QAction *> result = m_actions.values();
    result += m_brushActions.values();
    result += m_shapeActions.values();
    result += m_outlineActions.values();
    result += m_fillActions.values();
    result += m_shapeSizeActions;
    return result;
}

void ActionRegistry::setSelectionAvailable(bool available)
{
    for (ActionId id :
         {ActionId::Cut, ActionId::Crop, ActionId::DeleteSelection, ActionId::InvertSelection}) {
        get(id)->setEnabled(available);
    }
}

void ActionRegistry::setHistoryAvailable(bool canUndo, bool canRedo)
{
    get(ActionId::Undo)->setEnabled(canUndo);
    get(ActionId::Redo)->setEnabled(canRedo);
}

void ActionRegistry::setLayerState(qsizetype layerCount, qsizetype activeLayer, bool activeVisible,
                                   bool backgroundVisible)
{
    get(ActionId::DeleteLayer)->setEnabled(layerCount > 1);
    get(ActionId::MergeLayerDown)->setEnabled(activeLayer > 0);
    get(ActionId::ToggleLayerVisibility)->setText(activeVisible ? tr("&Hide layer") : tr("&Show layer"));
    get(ActionId::ToggleLayerVisibility)
        ->setIcon(ui::icon(activeVisible ? QStringLiteral("layer-hidden") : QStringLiteral("layer-visible")));
    get(ActionId::ToggleBackground)
        ->setText(backgroundVisible ? tr("Hide &background") : tr("Show &background"));
}
