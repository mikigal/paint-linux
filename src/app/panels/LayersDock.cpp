#include "panels/LayersDock.h"

#include "ActionRegistry.h"
#include "document/Document.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QListView>
#include <QMenu>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVBoxLayout>

using namespace paint;

namespace {

constexpr int ThumbnailWidth = 80;
constexpr int TileSpacing = 8;

int thumbnailHeight(QSize documentSize)
{
    return std::clamp(ThumbnailWidth * documentSize.height() / std::max(1, documentSize.width()), 24, 120);
}

void drawCheckerboard(QPainter *painter, const QRect &rect)
{
    constexpr int Cell = 6;
    painter->save();
    painter->setClipRect(rect);
    painter->fillRect(rect, Qt::white);
    for (int y = rect.top(); y < rect.bottom(); y += Cell) {
        for (int x = rect.left() + ((y - rect.top()) / Cell % 2) * Cell; x < rect.right(); x += 2 * Cell) {
            painter->fillRect(QRect(x, y, Cell, Cell), QColor(0xe0, 0xe0, 0xe0));
        }
    }
    painter->restore();
}

// Draws a tile with Paint's selection border: 2 px accent when active, a hairline otherwise.
void drawTileFrame(QPainter *painter, const QRect &tile, const QPalette &palette, bool active)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(Qt::NoBrush);
    if (active) {
        painter->setPen(QPen(palette.color(QPalette::Highlight), 2));
        painter->drawRoundedRect(QRectF(tile).adjusted(-2, -2, 2, 2), 4, 4);
    } else {
        QColor line = palette.color(QPalette::WindowText);
        line.setAlphaF(0.25);
        painter->setPen(QPen(line, 1));
        painter->drawRect(QRectF(tile).adjusted(-0.5, -0.5, 0.5, 0.5));
    }
    painter->restore();
}

class LayerDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const QImage thumbnail = index.data(LayerModel::ThumbnailRole).value<QImage>();
        QRect tile(QPoint(0, 0), thumbnail.size().scaled(QSize(ThumbnailWidth, 120), Qt::KeepAspectRatio));
        tile.moveCenter(option.rect.center());
        drawCheckerboard(painter, tile);
        painter->drawImage(tile, thumbnail);
        drawTileFrame(painter, tile, option.palette, index.data(LayerModel::ActiveRole).toBool());
    }

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &index) const override
    {
        const QImage thumbnail = index.data(LayerModel::ThumbnailRole).value<QImage>();
        return QSize(ThumbnailWidth + 2 * TileSpacing, thumbnail.height() + 2 * TileSpacing);
    }
};

// The background tile at the bottom of the strip: the document's background colour.
class BackgroundTile : public QWidget
{
public:
    BackgroundTile(Document *document, ActionRegistry *actions, QWidget *parent)
        : QWidget(parent)
        , m_document(document)
        , m_actions(actions)
    {
        setObjectName(QStringLiteral("layers.background"));
        setToolTip(tr("Background"));
    }

    QSize sizeHint() const override
    {
        return QSize(ThumbnailWidth + 2 * TileSpacing, thumbnailHeight(m_document->size()) + 2 * TileSpacing);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        QRect tile(0, 0, ThumbnailWidth, thumbnailHeight(m_document->size()));
        tile.moveCenter(rect().center());
        if (m_document->isBackgroundVisible()) {
            painter.fillRect(tile, m_document->backgroundColor());
        } else {
            drawCheckerboard(&painter, tile);
        }
        drawTileFrame(&painter, tile, palette(), false);
    }

    void contextMenuEvent(QContextMenuEvent *event) override
    {
        QMenu menu(this);
        menu.addAction(m_actions->get(ActionId::ToggleBackground));
        menu.addAction(m_actions->get(ActionId::BackgroundColor));
        menu.exec(event->globalPos());
    }

    void mouseReleaseEvent(QMouseEvent *) override { m_actions->get(ActionId::BackgroundColor)->trigger(); }

private:
    Document *m_document;
    ActionRegistry *m_actions;
};

} // namespace

LayerModel::LayerModel(Document *document, QObject *parent)
    : QAbstractListModel(parent)
    , m_document(document)
{}

int LayerModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_document->layerCount());
}

qsizetype LayerModel::layerIndex(int row) const
{
    return m_document->layerCount() - 1 - row;
}

QVariant LayerModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return {};
    }
    const qsizetype layer = layerIndex(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
        return m_document->layer(layer).name;
    case ActiveRole:
        return layer == m_document->activeLayerIndex();
    case VisibleRole:
        return m_document->layer(layer).visible;
    case ThumbnailRole:
        return m_document->layer(layer).image.scaled(ThumbnailWidth, thumbnailHeight(m_document->size()),
                                                     Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    default:
        return {};
    }
}

void LayerModel::refresh()
{
    beginResetModel();
    endResetModel();
}

LayersDock::LayersDock(Document *document, ActionRegistry *actions, QWidget *parent)
    : QDockWidget(tr("Layers"), parent)
    , m_document(document)
    , m_model(new LayerModel(document, this))
    , m_list(new QListView)
    , m_background(new BackgroundTile(document, actions, nullptr))
{
    setObjectName(QStringLiteral("layersDock"));
    setFeatures(QDockWidget::DockWidgetClosable);
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto *add = new QToolButton;
    add->setObjectName(QStringLiteral("layers.add"));
    add->setAutoRaise(true);
    add->setDefaultAction(actions->get(ActionId::AddLayer));

    m_list->setObjectName(QStringLiteral("layers.list"));
    m_list->setModel(m_model);
    m_list->setItemDelegate(new LayerDelegate(m_list));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->viewport()->setAutoFillBackground(false);
    m_list->setContextMenuPolicy(Qt::ActionsContextMenu);
    m_list->addActions({actions->get(ActionId::DuplicateLayer), actions->get(ActionId::ToggleLayerVisibility),
                        actions->get(ActionId::MergeLayerDown), actions->get(ActionId::DeleteLayer)});

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addWidget(add, 0, Qt::AlignHCenter);
    layout->addWidget(m_list, 1);
    layout->addWidget(m_background);
    setWidget(content);
    content->setMinimumWidth(ThumbnailWidth + 2 * TileSpacing + 8);
}

void LayersDock::refresh()
{
    m_model->refresh();
    m_background->update();
}
