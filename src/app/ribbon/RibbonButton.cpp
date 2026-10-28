#include "ribbon/RibbonButton.h"

#include "ui/Accessibility.h"

#include <QActionEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOptionToolButton>
#include <QStylePainter>

namespace {

constexpr int SmallSize = 32;
constexpr int IconSize = 20;
constexpr int LargeIconSize = 24;
constexpr int ChevronWidth = 14;
constexpr int LargeWidth = 44;
constexpr int LargeHeight = 72; // two rows of small buttons with their gap
constexpr int LargePanelHeight = 56;

} // namespace

void drawChevron(QPainter *painter, const QRectF &rect, const QColor &color)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(color, 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const QPointF c = rect.center();
    painter->drawPolyline(
        QPolygonF{{c.x() - 3.5, c.y() - 1.75}, {c.x(), c.y() + 1.75}, {c.x() + 3.5, c.y() - 1.75}});
    painter->restore();
}

RibbonButton::RibbonButton(Kind kind, QWidget *parent)
    : QToolButton(parent)
    , m_kind(kind)
{
    setAutoRaise(true);
    setFocusPolicy(Qt::TabFocus);
    // Icon-only buttons without an action are known by their tooltip.
    paint::ui::nameAfterToolTip(this);
    const bool large = kind == Kind::Large || kind == Kind::LargeDropDown || kind == Kind::LargeSplit;
    // Paint draws the two-row buttons with larger icons.
    setIconSize(large ? QSize(LargeIconSize, LargeIconSize) : QSize(IconSize, IconSize));
    setToolButtonStyle(Qt::ToolButtonIconOnly);
    switch (kind) {
    case Kind::SmallDropDown:
    case Kind::LargeDropDown:
        setPopupMode(QToolButton::InstantPopup);
        break;
    case Kind::LargeSplit:
        // The chevron area opens the menu (see mousePressEvent); a long press works too.
        setPopupMode(QToolButton::DelayedPopup);
        break;
    case Kind::Small:
    case Kind::Large:
        break;
    }
}

void RibbonButton::setRibbonIcon(const QIcon &icon)
{
    m_ribbonIcon = icon;
    update();
}

QSize RibbonButton::sizeHint() const
{
    switch (m_kind) {
    case Kind::Small:
        return {SmallSize, SmallSize};
    case Kind::SmallDropDown:
        return {SmallSize + ChevronWidth, SmallSize};
    case Kind::Large:
    case Kind::LargeDropDown:
    case Kind::LargeSplit:
        return {LargeWidth, LargeHeight};
    }
    return {};
}

QRect RibbonButton::panelRect() const
{
    if (m_kind == Kind::Large || m_kind == Kind::LargeDropDown || m_kind == Kind::LargeSplit) {
        const int height = m_kind == Kind::Large ? LargePanelHeight - 16 : LargePanelHeight;
        return QRect((width() - (LargeWidth - 4)) / 2, (this->height() - height) / 2, LargeWidth - 4, height);
    }
    return rect();
}

QRect RibbonButton::chevronRect() const
{
    const QRect panel = panelRect();
    if (m_kind == Kind::SmallDropDown) {
        return QRect(panel.right() - ChevronWidth - 2, panel.top(), ChevronWidth, panel.height());
    }
    return QRect(panel.left(), panel.bottom() - 18, panel.width(), 16);
}

QIcon RibbonButton::currentIcon() const
{
    return m_ribbonIcon.isNull() ? icon() : m_ribbonIcon;
}

void RibbonButton::paintEvent(QPaintEvent *)
{
    QStylePainter painter(this);
    QStyleOptionToolButton option;
    initStyleOption(&option);
    option.rect = panelRect();
    // No arrow from the style: Paint's chevron is drawn below.
    option.features &= ~(QStyleOptionToolButton::HasMenu | QStyleOptionToolButton::MenuButtonPopup);
    painter.drawPrimitive(QStyle::PE_PanelButtonTool, option);
    if (isChecked()) {
        // Paint outlines the selected tool in the accent colour, on top of the native checked look.
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(palette().color(QPalette::Highlight), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(option.rect).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
        painter.restore();
    }

    const QIcon::Mode mode = isEnabled() ? QIcon::Normal : QIcon::Disabled;
    const QIcon::State state = isChecked() ? QIcon::On : QIcon::Off;
    const QPixmap pixmap = currentIcon().pixmap(iconSize(), devicePixelRatioF(), mode, state);
    const QRect panel = panelRect();
    QRect iconRect(QPoint(0, 0), iconSize());
    switch (m_kind) {
    case Kind::Small:
    case Kind::Large:
        iconRect.moveCenter(panel.center());
        break;
    case Kind::SmallDropDown:
        iconRect.moveCenter(QPoint(panel.left() + SmallSize / 2, panel.center().y()));
        break;
    case Kind::LargeDropDown:
    case Kind::LargeSplit:
        iconRect.moveCenter(QPoint(panel.center().x(), panel.top() + (panel.height() - 18) / 2 + 1));
        break;
    }
    painter.drawPixmap(iconRect, pixmap);

    if (m_kind != Kind::Small && m_kind != Kind::Large) {
        const QPalette::ColorGroup group = isEnabled() ? QPalette::Active : QPalette::Disabled;
        drawChevron(&painter, chevronRect(), palette().color(group, QPalette::ButtonText));
    }
}

void RibbonButton::mousePressEvent(QMouseEvent *event)
{
    if (m_kind == Kind::LargeSplit && menu() && event->button() == Qt::LeftButton &&
        chevronRect().contains(event->position().toPoint())) {
        showMenu();
        return;
    }
    QToolButton::mousePressEvent(event);
}

void RibbonButton::actionEvent(QActionEvent *event)
{
    QToolButton::actionEvent(event);
    // Paint's ribbon shows only icons; the action text becomes the tool tip.
    if (event->type() == QEvent::ActionChanged && event->action() == defaultAction()) {
        setToolButtonStyle(Qt::ToolButtonIconOnly);
    }
}
