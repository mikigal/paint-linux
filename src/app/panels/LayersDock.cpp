#include "panels/LayersDock.h"

#include "ActionRegistry.h"
#include "document/Document.h"
#include "editor/Editor.h"
#include "ui/Icons.h"

#include <QAction>
#include <QApplication>
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
constexpr int EyeSize = 20;

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

QRect tileRect(const QRect &itemRect, QSize thumbnail)
{
    QRect tile(QPoint(0, 0), thumbnail.scaled(QSize(ThumbnailWidth, 120), Qt::KeepAspectRatio));
    tile.moveCenter(itemRect.center());
    return tile;
}

// The hide/show button in a tile's top-right corner.
QRect eyeRect(const QRect &tile)
{
    return QRect(tile.right() - EyeSize - 2, tile.top() + 3, EyeSize, EyeSize);
}

class LayerDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const QImage thumbnail = index.data(LayerModel::ThumbnailRole).value<QImage>();
        const bool visible = index.data(LayerModel::VisibleRole).toBool();
        const QRect tile = tileRect(option.rect, thumbnail.size());
        drawCheckerboard(painter, tile);
        // Hidden layers fade, as in Paint.
        painter->setOpacity(visible ? 1.0 : 0.35);
        painter->drawImage(tile, thumbnail);
        painter->setOpacity(1.0);
        drawTileFrame(painter, tile, option.palette, index.data(LayerModel::ActiveRole).toBool());
        if (!visible || (option.state & QStyle::State_MouseOver)) {
            const QRect eye = eyeRect(tile);
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(Qt::NoPen);
            QColor plate = option.palette.color(QPalette::Base);
            plate.setAlphaF(0.85);
            painter->setBrush(plate);
            painter->drawRoundedRect(eye, 4, 4);
            painter->restore();
            ui::icon(visible ? QStringLiteral("layer-visible") : QStringLiteral("layer-hidden"))
                .paint(painter, eye.adjusted(2, 2, -2, -2));
        }
    }

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &index) const override
    {
        const QImage thumbnail = index.data(LayerModel::ThumbnailRole).value<QImage>();
        return QSize(ThumbnailWidth + 2 * TileSpacing, thumbnail.height() + 2 * TileSpacing);
    }
};

// The list of layer tiles. Mouse handling is its own rather than Qt's drag and drop: a click
// picks the layer at once and a drag only moves it within the list.
class LayerList : public QListView
{
public:
    LayerList(Editor *editor, LayerModel *model, ActionRegistry *actions)
        : m_editor(editor)
        , m_model(model)
        , m_actions(actions)
    {
        setObjectName(QStringLiteral("layers.list"));
        setModel(model);
        setItemDelegate(new LayerDelegate(this));
        setFrameShape(QFrame::NoFrame);
        setSelectionMode(QAbstractItemView::NoSelection);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setMouseTracking(true);
        viewport()->setAttribute(Qt::WA_Hover);
        viewport()->setAutoFillBackground(false);
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        const QModelIndex index = indexAt(event->position().toPoint());
        if (event->button() != Qt::LeftButton || !index.isValid()) {
            QListView::mousePressEvent(event);
            return;
        }
        const qsizetype layer = m_model->layerIndex(index.row());
        const QImage thumbnail = index.data(LayerModel::ThumbnailRole).value<QImage>();
        if (eyeRect(tileRect(visualRect(index), thumbnail.size())).contains(event->position().toPoint())) {
            m_editor->setLayerVisible(layer, !m_editor->document().layer(layer).visible);
            return;
        }
        m_editor->setActiveLayer(layer);
        m_pressRow = index.row();
        m_pressPosition = event->position().toPoint();
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_pressRow < 0 || !(event->buttons() & Qt::LeftButton)) {
            QListView::mouseMoveEvent(event);
            return;
        }
        const QPoint position = event->position().toPoint();
        if (!m_dragging &&
            (position - m_pressPosition).manhattanLength() >= QApplication::startDragDistance()) {
            m_dragging = true;
        }
        if (m_dragging) {
            m_dropSlot = slotAt(position);
            viewport()->update();
        }
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton || m_pressRow < 0) {
            QListView::mouseReleaseEvent(event);
            return;
        }
        const int from = m_pressRow;
        const int slot = m_dragging ? slotAt(event->position().toPoint()) : -1;
        m_pressRow = -1;
        m_dragging = false;
        m_dropSlot = -1;
        viewport()->update();
        if (slot >= 0) {
            // A slot is a gap between tiles; taking the tile out first shifts the gaps below it.
            const int row = slot > from ? slot - 1 : slot;
            if (row != from) {
                m_editor->moveLayer(m_model->layerIndex(from), m_model->layerIndex(row));
            }
        }
    }

    void paintEvent(QPaintEvent *event) override
    {
        QListView::paintEvent(event);
        if (!m_dragging || m_dropSlot < 0 || m_model->rowCount() == 0) {
            return;
        }
        // Where the dragged layer will land.
        const int rows = m_model->rowCount();
        const QRect item = visualRect(m_model->index(std::min(m_dropSlot, rows - 1)));
        const int y = m_dropSlot < rows ? item.top() : item.bottom();
        QPainter painter(viewport());
        painter.setPen(QPen(palette().color(QPalette::Highlight), 3, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(item.left() + TileSpacing, y, item.right() - TileSpacing, y);
    }

    void contextMenuEvent(QContextMenuEvent *event) override
    {
        const QModelIndex index = indexAt(event->pos());
        if (!index.isValid()) {
            return;
        }
        m_editor->setActiveLayer(m_model->layerIndex(index.row()));
        QMenu menu(this);
        menu.setObjectName(QStringLiteral("menu.layer"));
        for (ActionId id : {ActionId::DuplicateLayer, ActionId::ToggleLayerVisibility,
                            ActionId::MergeLayerDown, ActionId::DeleteLayer}) {
            menu.addAction(m_actions->get(id));
        }
        menu.exec(event->globalPos());
    }

private:
    // The gap between tiles nearest to `position`: 0 is above the top tile, rowCount below the last.
    int slotAt(QPoint position) const
    {
        const int rows = m_model->rowCount();
        for (int row = 0; row < rows; ++row) {
            if (position.y() < visualRect(m_model->index(row)).center().y()) {
                return row;
            }
        }
        return rows;
    }

    Editor *m_editor;
    LayerModel *m_model;
    ActionRegistry *m_actions;
    int m_pressRow = -1;
    QPoint m_pressPosition;
    bool m_dragging = false;
    int m_dropSlot = -1;
};

// The background tile at the bottom of the panel: the document's background colour.
class BackgroundTile : public QWidget
{
public:
    BackgroundTile(Editor *editor, ActionRegistry *actions, QWidget *parent)
        : QWidget(parent)
        , m_editor(editor)
        , m_actions(actions)
    {
        setObjectName(QStringLiteral("layers.background"));
        setToolTip(tr("Background"));
    }

    QSize sizeHint() const override
    {
        return QSize(ThumbnailWidth + 2 * TileSpacing,
                     thumbnailHeight(m_editor->document().size()) + 2 * TileSpacing);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const Document &document = m_editor->document();
        QPainter painter(this);
        QRect tile(0, 0, ThumbnailWidth, thumbnailHeight(document.size()));
        tile.moveCenter(rect().center());
        if (document.isBackgroundVisible()) {
            painter.fillRect(tile, document.backgroundColor());
        } else {
            drawCheckerboard(&painter, tile);
        }
        drawTileFrame(&painter, tile, palette(), false);
    }

    void contextMenuEvent(QContextMenuEvent *event) override
    {
        QMenu menu(this);
        menu.setObjectName(QStringLiteral("menu.background"));
        menu.addAction(m_actions->get(ActionId::ToggleBackground));
        menu.addAction(m_actions->get(ActionId::BackgroundColor));
        menu.exec(event->globalPos());
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
            m_actions->get(ActionId::BackgroundColor)->trigger();
        }
    }

private:
    Editor *m_editor;
    ActionRegistry *m_actions;
};

} // namespace

LayerModel::LayerModel(Editor *editor, QObject *parent)
    : QAbstractListModel(parent)
    , m_editor(editor)
{
    refresh();
}

int LayerModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_editor->document().layerCount());
}

qsizetype LayerModel::layerIndex(int row) const
{
    return m_editor->document().layerCount() - 1 - row;
}

int LayerModel::rowOf(qsizetype layer) const
{
    return int(m_editor->document().layerCount() - 1 - layer);
}

QVariant LayerModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return {};
    }
    const Document &document = m_editor->document();
    const qsizetype layer = layerIndex(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
    case Qt::AccessibleTextRole:
        return document.layer(layer).name;
    case ActiveRole:
        return layer == document.activeLayerIndex();
    case VisibleRole:
        return document.layer(layer).visible;
    case ThumbnailRole:
        return m_thumbnails.value(layer);
    default:
        return {};
    }
}

void LayerModel::refresh()
{
    beginResetModel();
    const Document &document = m_editor->document();
    m_thumbnails.clear();
    for (qsizetype i = 0; i < document.layerCount(); ++i) {
        m_thumbnails.append(document.layer(i).image.scaled(ThumbnailWidth, thumbnailHeight(document.size()),
                                                           Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }
    endResetModel();
}

LayersDock::LayersDock(Editor *editor, ActionRegistry *actions, QWidget *parent)
    : QDockWidget(tr("Layers"), parent)
    , m_model(new LayerModel(editor, this))
    , m_list(new LayerList(editor, m_model, actions))
    , m_background(new BackgroundTile(editor, actions, nullptr))
{
    setObjectName(QStringLiteral("layersDock"));
    setFeatures(QDockWidget::DockWidgetClosable);
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto *add = new QToolButton;
    add->setObjectName(QStringLiteral("layers.add"));
    add->setAutoRaise(true);
    add->setDefaultAction(actions->get(ActionId::AddLayer));

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addWidget(add, 0, Qt::AlignHCenter);
    layout->addWidget(m_list, 1);
    layout->addWidget(m_background);
    setWidget(content);
    content->setMinimumWidth(ThumbnailWidth + 2 * TileSpacing + 8);

    connect(editor, &Editor::activeLayerChanged, this, &LayersDock::refresh);
}

void LayersDock::refresh()
{
    m_model->refresh();
    m_background->updateGeometry();
    m_background->update();
}
