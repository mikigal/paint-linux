#include "ribbon/RibbonHost.h"

#include "ribbon/Ribbon.h"
#include "ribbon/RibbonButton.h"

#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

// A flat button showing only Paint's chevron, pointing down.
class ChevronButton : public QToolButton
{
public:
    using QToolButton::QToolButton;

    QSize sizeHint() const override { return {24, 20}; }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QToolButton::paintEvent(event);
        QPainter painter(this);
        drawChevron(
            &painter, rect(),
            palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::ButtonText));
    }
};

} // namespace

RibbonHost::RibbonHost(Ribbon *ribbon, QMenu *options, QWidget *parent)
    : QWidget(parent)
    , m_ribbon(ribbon)
    , m_strip(new QWidget(this))
    , m_options(new ChevronButton(this))
    , m_popup(new QFrame(this))
{
    setObjectName(QStringLiteral("ribbonHost"));
    m_options->setObjectName(QStringLiteral("ribbon.toolbarOptions"));
    m_options->setAutoRaise(true);
    m_options->setPopupMode(QToolButton::InstantPopup);
    m_options->setToolTip(tr("Toolbar options"));
    m_options->setMenu(options);
    // Paint's chevron sits in the bottom-right corner of the ribbon.
    m_options->setParent(ribbon);

    auto *show = new ChevronButton(m_strip);
    show->setObjectName(QStringLiteral("ribbon.showToolbar"));
    show->setAutoRaise(true);
    show->setToolTip(tr("Show toolbar"));
    show->setAccessibleName(tr("Show toolbar"));
    connect(show, &QToolButton::clicked, this, &RibbonHost::showRibbonPopup);
    auto *stripLayout = new QHBoxLayout(m_strip);
    stripLayout->setContentsMargins(4, 0, 8, 2);
    stripLayout->addStretch(1);
    stripLayout->addWidget(show);
    m_strip->hide();

    m_popup->setObjectName(QStringLiteral("ribbonPopup"));
    m_popup->setFrameShape(QFrame::StyledPanel);
    m_popup->setAutoFillBackground(true);
    m_popup->hide();
    auto *popupLayout = new QVBoxLayout(m_popup);
    popupLayout->setContentsMargins(0, 0, 0, 0);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(ribbon);
    layout->addWidget(m_strip);

    ribbon->installEventFilter(this);
    m_options->raise();
}

void RibbonHost::setAutoHide(bool autoHide)
{
    if (autoHide == m_autoHide) {
        return;
    }
    m_autoHide = autoHide;
    hideRibbonPopup();
    if (autoHide) {
        layout()->removeWidget(m_ribbon);
        m_popup->layout()->addWidget(m_ribbon);
        m_strip->show();
    } else {
        dockRibbon();
    }
}

void RibbonHost::dockRibbon()
{
    m_strip->hide();
    m_popup->layout()->removeWidget(m_ribbon);
    m_ribbon->setParent(this);
    static_cast<QVBoxLayout *>(layout())->insertWidget(0, m_ribbon);
    m_ribbon->show();
    m_options->raise();
}

void RibbonHost::showRibbonPopup()
{
    if (!m_autoHide) {
        return;
    }
    // An overlay inside the main window rather than a popup window: it needs no compositor
    // positioning and is dismissed by our own click and Escape handling.
    QWidget *window = this->window();
    m_popup->setParent(window);
    m_ribbon->show();
    m_popup->setFixedWidth(width());
    m_popup->adjustSize();
    m_popup->move(mapTo(window, QPoint(0, 0)));
    m_popup->raise();
    m_popup->show();
    m_options->raise();
    qApp->installEventFilter(this);
}

void RibbonHost::hideRibbonPopup()
{
    if (m_popup->isVisible()) {
        m_popup->hide();
        qApp->removeEventFilter(this);
    }
}

bool RibbonHost::eventFilter(QObject *watched, QEvent *event)
{
    if (m_popup->isVisible()) {
        if (event->type() == QEvent::MouseButtonPress) {
            // A click elsewhere in the main window dismisses the ribbon; menus opened from the
            // ribbon are separate windows and keep it open.
            auto *widget = qobject_cast<QWidget *>(watched);
            if (widget && widget->window() == m_popup->window() && widget != m_popup &&
                !m_popup->isAncestorOf(widget)) {
                hideRibbonPopup();
            }
        } else if (event->type() == QEvent::KeyPress &&
                   static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape &&
                   !QApplication::activePopupWidget()) {
            hideRibbonPopup();
            return true;
        }
    }
    if (watched == m_ribbon && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        m_options->adjustSize();
        m_options->move(m_ribbon->width() - m_options->width() - 4,
                        m_ribbon->height() - m_options->height() - 4);
        m_options->raise();
    }
    return QWidget::eventFilter(watched, event);
}

bool RibbonHost::isRibbonPopupVisible() const
{
    return m_popup->isVisible();
}
