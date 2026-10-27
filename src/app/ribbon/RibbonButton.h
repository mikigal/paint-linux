#pragma once

#include <QToolButton>

// An icon-only ribbon button in Paint's two sizes. The frame comes from the system style
// (hover, pressed and checked states look native); icons and chevrons follow Paint's layout.
class RibbonButton : public QToolButton
{
    Q_OBJECT

public:
    enum class Kind {
        Small,         // 32×32 icon button
        SmallDropDown, // icon + chevron; the whole button opens the menu
        Large,         // 2 rows tall, icon only
        LargeDropDown, // 2 rows tall, icon over a chevron; the whole button opens the menu
        LargeSplit,    // 2 rows tall; the icon triggers the action, the chevron opens the menu
    };

    explicit RibbonButton(Kind kind, QWidget *parent = nullptr);

    Kind kind() const { return m_kind; }
    // Overrides the default action's icon (the ribbon uses Paint icons where menus use theme icons).
    void setRibbonIcon(const QIcon &icon);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void actionEvent(QActionEvent *event) override;

private:
    QRect panelRect() const;
    QRect chevronRect() const;
    QIcon currentIcon() const;

    Kind m_kind;
    QIcon m_ribbonIcon;
};

// Draws Paint's small "⌄" chevron centred in `rect`.
void drawChevron(QPainter *painter, const QRectF &rect, const QColor &color);
