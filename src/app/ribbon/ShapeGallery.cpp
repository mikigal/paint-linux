#include "ribbon/ShapeGallery.h"

#include "ActionRegistry.h"

#include <QAction>
#include <QGridLayout>
#include <QScrollBar>
#include <QToolButton>

using namespace paint;

ShapeGallery::ShapeGallery(ActionRegistry *actions, QWidget *parent)
    : QScrollArea(parent)
{
    setObjectName(QStringLiteral("ribbon.shapeGallery"));
    setAccessibleName(tr("Shapes"));
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    viewport()->setBackgroundRole(QPalette::Base);

    auto *grid = new QWidget;
    grid->setBackgroundRole(QPalette::Base);
    auto *layout = new QGridLayout(grid);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    int index = 0;
    for (ShapeType type : AllShapeTypes) {
        auto *button = new QToolButton(grid);
        button->setAutoRaise(true);
        button->setFixedSize(CellSize, CellSize);
        button->setIconSize(QSize(18, 18));
        button->setDefaultAction(actions->shapeAction(type));
        button->setObjectName(QStringLiteral("ribbon.shape.") +
                              actions->shapeAction(type)->objectName().section(QLatin1Char('.'), -1));
        layout->addWidget(button, index / Columns, index % Columns);
        // Scroll the chosen shape into view, e.g. when it was picked from a menu.
        connect(actions->shapeAction(type), &QAction::toggled, button, [this, button](bool checked) {
            if (checked) {
                ensureWidgetVisible(button, 0, 0);
            }
        });
        ++index;
    }
    setWidget(grid);
    verticalScrollBar()->setSingleStep(CellSize);
}

QSize ShapeGallery::sizeHint() const
{
    const int frame = 2 * frameWidth();
    return QSize(Columns * CellSize + verticalScrollBar()->sizeHint().width() + frame + 2,
                 VisibleRows * CellSize + frame + 2);
}
