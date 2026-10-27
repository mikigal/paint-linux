#include "ribbon/PaintRibbon.h"

#include "ActionRegistry.h"
#include "ribbon/ColorsPanel.h"
#include "ribbon/PreviewMenu.h"
#include "ribbon/Ribbon.h"
#include "ribbon/RibbonButton.h"
#include "ribbon/ShapeGallery.h"
#include "tools/ToolSettings.h"
#include "ui/BrushPreview.h"
#include "ui/Icons.h"

#include <QAction>
#include <QCoreApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QVBoxLayout>
#include <QWidgetAction>

using namespace paint;

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("PaintRibbon", text);
}

RibbonButton *button(RibbonButton::Kind kind, QAction *action, const QString &name, QWidget *parent,
                     const QString &ribbonIcon = {})
{
    auto *result = new RibbonButton(kind, parent);
    result->setObjectName(QStringLiteral("ribbon.") + name);
    if (action) {
        result->setDefaultAction(action);
    }
    if (!ribbonIcon.isEmpty()) {
        result->setRibbonIcon(ui::icon(ribbonIcon));
    }
    return result;
}

QMenu *menu(const QString &name, QWidget *parent)
{
    auto *result = new QMenu(parent);
    // A separate namespace from the menu bar's menus ("menu.image" there, "menu.ribbon.image" here).
    result->setObjectName(QStringLiteral("menu.ribbon.") + name);
    return result;
}

// Two rows of small buttons, as most Paint groups are laid out.
QGridLayout *buttonGrid(QWidget *content)
{
    auto *grid = new QGridLayout(content);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(4);
    grid->setVerticalSpacing(8);
    return grid;
}

QMenu *rotateMenu(ActionRegistry *actions, const QString &name, QWidget *parent)
{
    QMenu *rotate = menu(name, parent);
    rotate->setTitle(tr("&Rotate"));
    rotate->setIcon(ui::icon(QStringLiteral("rotate")));
    rotate->addAction(actions->get(ActionId::RotateRight));
    rotate->addAction(actions->get(ActionId::RotateLeft));
    rotate->addAction(actions->get(ActionId::Rotate180));
    rotate->addSeparator();
    rotate->addAction(actions->get(ActionId::RotateCustom));
    return rotate;
}

QMenu *flipMenu(ActionRegistry *actions, const QString &name, QWidget *parent)
{
    QMenu *flip = menu(name, parent);
    flip->setTitle(tr("&Flip"));
    flip->setIcon(ui::icon(QStringLiteral("flip")));
    flip->addAction(actions->get(ActionId::FlipVertical));
    flip->addAction(actions->get(ActionId::FlipHorizontal));
    return flip;
}

QMenu *styleMenu(const QString &name, const QString &title, const QList<QAction *> &styles, QWidget *parent)
{
    QMenu *result = menu(name, parent);
    result->setTitle(title);
    result->addActions(styles);
    return result;
}

RibbonGroup *quickAccessGroup(ActionRegistry *actions, QWidget *parent)
{
    auto *content = new QWidget;
    QGridLayout *grid = buttonGrid(content);
    grid->addWidget(button(RibbonButton::Kind::Small, actions->get(ActionId::Save), QStringLiteral("save"),
                           content, QStringLiteral("save")),
                    0, 0);
    grid->addWidget(button(RibbonButton::Kind::Small, actions->get(ActionId::Undo), QStringLiteral("undo"),
                           content, QStringLiteral("undo")),
                    1, 0);
    grid->addWidget(button(RibbonButton::Kind::Small, actions->get(ActionId::Redo), QStringLiteral("redo"),
                           content, QStringLiteral("redo")),
                    1, 1);
    auto *group = new RibbonGroup(tr("Quick access"), content, parent);

    RibbonButton *compact = button(RibbonButton::Kind::LargeDropDown, nullptr,
                                   QStringLiteral("quickAccessCompact"), group, QStringLiteral("save"));
    compact->setToolTip(tr("Quick access"));
    QMenu *compactMenu = menu(QStringLiteral("quickAccess"), compact);
    compactMenu->addActions(
        {actions->get(ActionId::Save), actions->get(ActionId::Undo), actions->get(ActionId::Redo)});
    compact->setMenu(compactMenu);
    group->setCompactWidget(compact);
    return group;
}

RibbonGroup *selectionGroup(ActionRegistry *actions, ToolSettings *settings, QWidget *parent)
{
    auto *content = new QWidget;
    auto *layout = new QHBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    RibbonButton *select =
        button(RibbonButton::Kind::LargeSplit, actions->toolAction(settings->selectionTool()),
               QStringLiteral("select"), content);
    QMenu *selectMenu = menu(QStringLiteral("select"), select);
    selectMenu->addAction(actions->get(ActionId::ToolRectangleSelect));
    selectMenu->addAction(actions->get(ActionId::ToolFreeformSelect));
    selectMenu->addSeparator();
    selectMenu->addAction(actions->get(ActionId::SelectAll));
    selectMenu->addAction(actions->get(ActionId::InvertSelection));
    selectMenu->addSeparator();
    selectMenu->addAction(actions->get(ActionId::TransparentSelection));
    selectMenu->addSeparator();
    selectMenu->addAction(actions->get(ActionId::DeleteSelection));
    select->setMenu(selectMenu);
    layout->addWidget(select);

    // The button stands for the last used selection tool.
    QObject::connect(settings, &ToolSettings::toolChanged, select, [select, actions](Tool tool) {
        if (isSelectionTool(tool) && select->defaultAction() != actions->toolAction(tool)) {
            QMenu *keep = select->menu();
            select->setDefaultAction(actions->toolAction(tool));
            select->setMenu(keep);
        }
    });
    return new RibbonGroup(tr("Selection"), content, parent);
}

RibbonGroup *imageGroup(ActionRegistry *actions, QWidget *parent)
{
    auto *content = new QWidget;
    auto *layout = new QHBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    auto *small = new QWidget(content);
    QGridLayout *grid = buttonGrid(small);
    grid->addWidget(
        button(RibbonButton::Kind::Small, actions->get(ActionId::Crop), QStringLiteral("crop"), small), 0, 0);
    RibbonButton *rotate = button(RibbonButton::Kind::SmallDropDown, nullptr, QStringLiteral("rotate"), small,
                                  QStringLiteral("rotate"));
    rotate->setToolTip(tr("Rotate"));
    rotate->setMenu(rotateMenu(actions, QStringLiteral("rotate"), rotate));
    grid->addWidget(rotate, 0, 1);
    grid->addWidget(button(RibbonButton::Kind::Small, actions->get(ActionId::InvertColors),
                           QStringLiteral("invertColors"), small),
                    1, 0);
    RibbonButton *flip = button(RibbonButton::Kind::SmallDropDown, nullptr, QStringLiteral("flip"), small,
                                QStringLiteral("flip"));
    flip->setToolTip(tr("Flip"));
    flip->setMenu(flipMenu(actions, QStringLiteral("flip"), flip));
    grid->addWidget(flip, 1, 1);
    layout->addWidget(small);
    layout->addWidget(button(RibbonButton::Kind::Large, actions->get(ActionId::ResizeSkew),
                             QStringLiteral("resize"), content));
    auto *group = new RibbonGroup(tr("Image"), content, parent);

    RibbonButton *compact = button(RibbonButton::Kind::LargeDropDown, nullptr, QStringLiteral("imageCompact"),
                                   group, QStringLiteral("resize"));
    compact->setToolTip(tr("Image"));
    QMenu *compactMenu = menu(QStringLiteral("image"), compact);
    compactMenu->addAction(actions->get(ActionId::Crop));
    compactMenu->addAction(actions->get(ActionId::ResizeSkew));
    compactMenu->addMenu(rotateMenu(actions, QStringLiteral("imageRotate"), compactMenu));
    compactMenu->addMenu(flipMenu(actions, QStringLiteral("imageFlip"), compactMenu));
    compactMenu->addAction(actions->get(ActionId::InvertColors));
    compact->setMenu(compactMenu);
    group->setCompactWidget(compact);
    return group;
}

RibbonGroup *toolsGroup(ActionRegistry *actions, QWidget *parent)
{
    auto *content = new QWidget;
    QGridLayout *grid = buttonGrid(content);
    const struct
    {
        Tool tool;
        int row, column;
        const char *name;
    } tools[] = {
        {Tool::Pencil, 0, 0, "pencil"},
        {Tool::Fill, 0, 1, "fill"},
        {Tool::Text, 0, 2, "text"},
        {Tool::Eraser, 1, 0, "eraser"},
        {Tool::ColorPicker, 1, 1, "colorPicker"},
        {Tool::Magnifier, 1, 2, "magnifier"},
    };
    for (const auto &tool : tools) {
        grid->addWidget(button(RibbonButton::Kind::Small, actions->toolAction(tool.tool),
                               QLatin1StringView(tool.name).toString(), content),
                        tool.row, tool.column);
    }
    return new RibbonGroup(tr("Tools"), content, parent);
}

RibbonGroup *brushesGroup(ActionRegistry *actions, QWidget *parent)
{
    auto *content = new QWidget;
    auto *layout = new QHBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    RibbonButton *brushes = button(RibbonButton::Kind::LargeSplit, actions->get(ActionId::ToolBrush),
                                   QStringLiteral("brushes"), content);
    QMenu *brushMenu = menu(QStringLiteral("brushes"), brushes);
    for (BrushType type : AllBrushTypes) {
        addPreviewRow(brushMenu, actions->brushAction(type),
                      [type](QSize size, qreal ratio, const QColor &color) {
                          return ui::brushPreview(type, size, ratio, color);
                      });
    }
    brushes->setMenu(brushMenu);
    layout->addWidget(brushes);
    return new RibbonGroup(tr("Brushes"), content, parent);
}

QList<QAction *> strokeStyleActions(ActionRegistry *actions, bool outline)
{
    QList<QAction *> result;
    for (StrokeStyle style : AllStrokeStyles) {
        result.append(outline ? actions->outlineAction(style) : actions->fillAction(style));
    }
    return result;
}

RibbonGroup *shapesGroup(ActionRegistry *actions, ToolSettings *settings, QWidget *parent)
{
    auto *content = new QWidget;
    auto *layout = new QHBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(new ShapeGallery(actions, content));

    auto *options = new QWidget(content);
    auto *optionsLayout = new QVBoxLayout(options);
    optionsLayout->setContentsMargins(0, 0, 0, 0);
    optionsLayout->setSpacing(8);
    RibbonButton *outline = button(RibbonButton::Kind::SmallDropDown, nullptr, QStringLiteral("outline"),
                                   options, QStringLiteral("shape-outline"));
    outline->setToolTip(tr("Outline"));
    outline->setMenu(
        styleMenu(QStringLiteral("outline"), tr("&Outline"), strokeStyleActions(actions, true), outline));
    RibbonButton *fill = button(RibbonButton::Kind::SmallDropDown, nullptr, QStringLiteral("shapeFill"),
                                options, QStringLiteral("shape-fill"));
    fill->setToolTip(tr("Fill"));
    fill->setMenu(
        styleMenu(QStringLiteral("shapeFill"), tr("&Fill"), strokeStyleActions(actions, false), fill));
    optionsLayout->addWidget(outline);
    optionsLayout->addWidget(fill);
    layout->addWidget(options);

    RibbonButton *size = button(RibbonButton::Kind::LargeDropDown, nullptr, QStringLiteral("shapeSize"),
                                content, QStringLiteral("line-thickness"));
    size->setToolTip(tr("Size"));
    size->setMenu(styleMenu(QStringLiteral("shapeSize"), tr("&Size"), actions->shapeSizeActions(), size));
    layout->addWidget(size);

    // The shape options only apply while drawing shapes.
    const auto syncEnabled = [outline, fill, size](Tool tool) {
        for (QWidget *w :
             {static_cast<QWidget *>(outline), static_cast<QWidget *>(fill), static_cast<QWidget *>(size)}) {
            w->setEnabled(tool == Tool::Shape);
        }
    };
    QObject::connect(settings, &ToolSettings::toolChanged, content, syncEnabled);
    syncEnabled(settings->tool());

    auto *group = new RibbonGroup(tr("Shapes"), content, parent);
    RibbonButton *compact = button(RibbonButton::Kind::LargeDropDown, actions->get(ActionId::ToolShape),
                                   QStringLiteral("shapesCompact"), group);
    QMenu *compactMenu = menu(QStringLiteral("shapes"), compact);
    auto *galleryAction = new QWidgetAction(compactMenu);
    galleryAction->setDefaultWidget(new ShapeGallery(actions, compactMenu));
    compactMenu->addAction(galleryAction);
    compactMenu->addSeparator();
    compactMenu->addMenu(styleMenu(QStringLiteral("outlineCompact"), tr("&Outline"),
                                   strokeStyleActions(actions, true), compactMenu));
    compactMenu->addMenu(styleMenu(QStringLiteral("shapeFillCompact"), tr("&Fill"),
                                   strokeStyleActions(actions, false), compactMenu));
    compactMenu->addMenu(
        styleMenu(QStringLiteral("shapeSizeCompact"), tr("&Size"), actions->shapeSizeActions(), compactMenu));
    compact->setMenu(compactMenu);
    // The shapes button only opens the gallery; it does not activate a shape by itself.
    compact->setPopupMode(QToolButton::InstantPopup);
    group->setCompactWidget(compact);
    return group;
}

RibbonGroup *colorsGroup(ActionRegistry *actions, ToolSettings *settings, QWidget *parent)
{
    QAction *editColors = actions->get(ActionId::EditColors);
    auto *group = new RibbonGroup(tr("Colors"), new ColorsPanel(settings, editColors), parent);
    group->setCompactWidget(new ColorsPanel(settings, editColors, ColorsPanel::Mode::Compact));
    return group;
}

RibbonGroup *layersGroup(ActionRegistry *actions, QWidget *parent)
{
    auto *content = new QWidget;
    auto *layout = new QHBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(button(RibbonButton::Kind::Large, actions->get(ActionId::LayersPanel),
                             QStringLiteral("layers"), content));
    return new RibbonGroup(tr("Layers"), content, parent);
}

} // namespace

Ribbon *createPaintRibbon(ActionRegistry *actions, ToolSettings *settings, QWidget *parent)
{
    auto *ribbon = new Ribbon(parent);
    // Collapse order follows Paint: the shapes gallery goes first, then the Image group; quick
    // access and the palette only fold up in very narrow windows.
    ribbon->addGroup(quickAccessGroup(actions, ribbon), 2);
    ribbon->addGroup(selectionGroup(actions, settings, ribbon));
    ribbon->addGroup(imageGroup(actions, ribbon), 1);
    ribbon->addGroup(toolsGroup(actions, ribbon));
    ribbon->addGroup(brushesGroup(actions, ribbon));
    ribbon->addGroup(shapesGroup(actions, settings, ribbon), 0);
    ribbon->addGroup(colorsGroup(actions, settings, ribbon), 3);
    ribbon->addGroup(layersGroup(actions, ribbon));
    return ribbon;
}
