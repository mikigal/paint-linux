#include "ribbon/PreviewMenu.h"

#include <QAction>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QStyleOptionMenuItem>
#include <QWidgetAction>

namespace {

constexpr QSize PreviewSize(72, 20);
constexpr int RowHeight = 40;
constexpr int RowWidth = 240;

} // namespace

PreviewMenuRow::PreviewMenuRow(QAction *action, PreviewFactory preview, QWidget *parent)
    : QWidget(parent)
    , m_action(action)
    , m_preview(std::move(preview))
{
    setObjectName(QStringLiteral("menuRow.") + action->objectName().section(QLatin1Char('.'), 1));
    setAttribute(Qt::WA_Hover);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setToolTip(action->toolTip());
    connect(action, &QAction::changed, this, qOverload<>(&QWidget::update));
}

QSize PreviewMenuRow::sizeHint() const
{
    return {RowWidth, RowHeight};
}

bool PreviewMenuRow::event(QEvent *event)
{
    if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverLeave) {
        update();
    }
    return QWidget::event(event);
}

void PreviewMenuRow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const bool selected = underMouse() || hasFocus();

    QStyleOptionMenuItem option;
    option.initFrom(this);
    option.menuItemType = QStyleOptionMenuItem::Normal;
    option.checkType = QStyleOptionMenuItem::NotCheckable;
    option.rect = rect();
    option.state.setFlag(QStyle::State_Selected, selected);
    option.state.setFlag(QStyle::State_Enabled, m_action->isEnabled());
    // Only the item frame/highlight; the text and preview are drawn below.
    style()->drawControl(QStyle::CE_MenuItem, &option, &painter, this);

    if (m_action->isChecked()) {
        // Paint marks the current item with a short accent bar.
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().color(QPalette::Highlight));
        painter.drawRoundedRect(QRectF(4, height() / 2.0 - 8, 3, 16), 1.5, 1.5);
    }

    // Breeze keeps the normal text colour on its light hover highlight.
    painter.setPen(
        palette().color(m_action->isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::Text));
    const QRect textRect = rect().adjusted(16, 0, -PreviewSize.width() - 20, 0);
    painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                     m_action->iconText().remove(QLatin1Char('&')));

    const QImage preview = m_preview(PreviewSize, devicePixelRatioF(), palette().color(QPalette::Text));
    painter.drawImage(QPoint(width() - PreviewSize.width() - 16, (height() - PreviewSize.height()) / 2),
                      preview);
}

void PreviewMenuRow::activate()
{
    if (!m_action->isEnabled()) {
        return;
    }
    // Close the whole menu chain first, as a regular menu item does.
    for (QWidget *w = parentWidget(); w; w = w->parentWidget()) {
        if (auto *menu = qobject_cast<QMenu *>(w)) {
            menu->close();
        }
    }
    m_action->trigger();
}

void PreviewMenuRow::mouseReleaseEvent(QMouseEvent *event)
{
    if (rect().contains(event->position().toPoint())) {
        activate();
    }
}

void PreviewMenuRow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space) {
        activate();
        return;
    }
    QWidget::keyPressEvent(event);
}

void addPreviewRow(QMenu *menu, QAction *action, PreviewMenuRow::PreviewFactory preview)
{
    auto *widgetAction = new QWidgetAction(menu);
    widgetAction->setObjectName(QStringLiteral("menuRowAction.") +
                                action->objectName().section(QLatin1Char('.'), 1));
    widgetAction->setDefaultWidget(new PreviewMenuRow(action, std::move(preview), menu));
    menu->addAction(widgetAction);
}
