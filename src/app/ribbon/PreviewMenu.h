#pragma once

#include <QImage>
#include <QWidget>

#include <functional>

class QAction;
class QMenu;

// A menu row with the item's name on the left and a wide preview image on the right, as in
// Paint's Brushes menu. Drawn with the style's menu item frame so it matches native menus.
class PreviewMenuRow : public QWidget
{
    Q_OBJECT

public:
    using PreviewFactory = std::function<QImage(QSize size, qreal devicePixelRatio, const QColor &color)>;

    PreviewMenuRow(QAction *action, PreviewFactory preview, QWidget *parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool event(QEvent *event) override;

private:
    void activate();

    QAction *m_action;
    PreviewFactory m_preview;
};

// Adds `action` to `menu` as a preview row.
void addPreviewRow(QMenu *menu, QAction *action, PreviewMenuRow::PreviewFactory preview);
