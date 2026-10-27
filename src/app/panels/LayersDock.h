#pragma once

#include <QAbstractListModel>
#include <QDockWidget>
#include <QImage>
#include <QList>

class ActionRegistry;
class QListView;

namespace paint {
class Editor;
}

// The document's layers, top layer first, for the layers panel. Thumbnails are cached and
// rebuilt by refresh().
class LayerModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        ActiveRole = Qt::UserRole + 1,
        VisibleRole,
        ThumbnailRole,
    };

    explicit LayerModel(paint::Editor *editor, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    // Rows run top to bottom, layers bottom to top.
    qsizetype layerIndex(int row) const;
    int rowOf(qsizetype layer) const;
    void refresh();

private:
    paint::Editor *m_editor;
    QList<QImage> m_thumbnails; // by layer index
};

// Paint's layers panel: an add button, a tile per layer (click to draw on it, drag to reorder,
// the eye to hide it, right-click for its menu) and the background tile below them.
class LayersDock : public QDockWidget
{
    Q_OBJECT

public:
    LayersDock(paint::Editor *editor, ActionRegistry *actions, QWidget *parent = nullptr);

    void refresh();
    QListView *list() const { return m_list; }

private:
    LayerModel *m_model;
    QListView *m_list;
    QWidget *m_background;
};
