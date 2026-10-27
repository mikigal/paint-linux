#pragma once

#include <QAbstractListModel>
#include <QDockWidget>

class ActionRegistry;
class QListView;

namespace paint {
class Document;
}

// The document's layers, top layer first, for the layers panel.
class LayerModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        ActiveRole = Qt::UserRole + 1,
        VisibleRole,
        ThumbnailRole,
    };

    explicit LayerModel(paint::Document *document, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    qsizetype layerIndex(int row) const;
    void refresh();

private:
    paint::Document *m_document;
};

// Paint's layers strip: an add button, layer thumbnails and the background tile.
class LayersDock : public QDockWidget
{
    Q_OBJECT

public:
    LayersDock(paint::Document *document, ActionRegistry *actions, QWidget *parent = nullptr);

    void refresh();

private:
    paint::Document *m_document;
    LayerModel *m_model;
    QListView *m_list;
    QWidget *m_background;
};
