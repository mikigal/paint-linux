#pragma once

#include <QScrollArea>

class ActionRegistry;

// Paint's inline shapes gallery: three visible rows of shape buttons that scroll.
class ShapeGallery : public QScrollArea
{
    Q_OBJECT

public:
    static constexpr int Columns = 7;
    static constexpr int VisibleRows = 3;
    static constexpr int CellSize = 24;

    explicit ShapeGallery(ActionRegistry *actions, QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }
};
