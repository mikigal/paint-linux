#include "canvas/SidePanel.h"

#include "tools/ToolSettings.h"
#include "ui/Icons.h"

#include <QAbstractScrollArea>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QPainter>
#include <QSlider>
#include <QToolTip>
#include <QVBoxLayout>

using namespace paint;

namespace {

constexpr int CardWidth = 40;
constexpr int SliderLength = 200;
constexpr int CardSpacing = 12;
constexpr int PanelInset = 12;

} // namespace

SliderCard::SliderCard(const QString &name, QWidget *parent)
    : QFrame(parent)
    , m_icon(new QLabel(this))
    , m_slider(new QSlider(Qt::Vertical, this))
{
    setObjectName(QStringLiteral("sidePanel.") + name);
    m_slider->setObjectName(QStringLiteral("sidePanel.") + name + QStringLiteral(".slider"));
    m_slider->setFixedHeight(SliderLength);
    m_icon->setAlignment(Qt::AlignCenter);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 10, 4, 10);
    layout->setSpacing(8);
    layout->addWidget(m_icon, 0, Qt::AlignHCenter);
    layout->addWidget(m_slider, 0, Qt::AlignHCenter);
    setFixedWidth(CardWidth);

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(12);
    shadow->setOffset(0, 1);
    shadow->setColor(QColor(0, 0, 0, 40));
    setGraphicsEffect(shadow);

    connect(m_slider, &QSlider::sliderMoved, this, &SliderCard::showValue);
}

void SliderCard::setIcon(const QString &iconName, const QString &toolTip)
{
    m_iconName = iconName;
    m_icon->setPixmap(ui::icon(iconName).pixmap(QSize(20, 20), devicePixelRatioF()));
    m_icon->setToolTip(toolTip);
    m_slider->setToolTip(toolTip);
    m_slider->setAccessibleName(toolTip);
}

void SliderCard::showValue(int value)
{
    QToolTip::showText(m_slider->mapToGlobal(QPoint(m_slider->width() + 4, 0)), m_format.arg(value),
                       m_slider);
}

void SliderCard::paintEvent(QPaintEvent *)
{
    // A rounded card in the window colour, like Paint's floating panels.
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor border = palette().color(QPalette::WindowText);
    border.setAlphaF(0.12);
    painter.setPen(QPen(border, 1));
    painter.setBrush(palette().color(QPalette::Window));
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
}

void SliderCard::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event->type() == QEvent::PaletteChange && !m_iconName.isEmpty()) {
        m_icon->setPixmap(ui::icon(m_iconName).pixmap(QSize(20, 20), devicePixelRatioF()));
    }
}

SidePanel::SidePanel(ToolSettings *settings, QAbstractScrollArea *area)
    : QWidget(area)
    , m_area(area)
    , m_settings(settings)
    , m_size(new SliderCard(QStringLiteral("size"), this))
    , m_second(new SliderCard(QStringLiteral("opacity"), this))
{
    setObjectName(QStringLiteral("sidePanel"));
    m_size->setIcon(QStringLiteral("line-thickness"), tr("Size"));
    m_size->setValueFormat(tr("%1 px"));
    m_size->slider()->setRange(ToolSettings::MinSize, ToolSettings::MaxSize);

    auto *layout = new QVBoxLayout(this);
    // Room for the cards' shadows.
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(CardSpacing);
    layout->addWidget(m_size);
    layout->addWidget(m_second);

    connect(m_size->slider(), &QSlider::valueChanged, settings, &ToolSettings::setSize);
    connect(settings, &ToolSettings::sizeChanged, m_size->slider(), &QSlider::setValue);
    connect(m_second->slider(), &QSlider::valueChanged, this, [this](int value) {
        if (m_settings->tool() == Tool::Fill) {
            m_settings->setFillTolerance(value);
        } else {
            m_settings->setOpacity(value);
        }
    });
    connect(settings, &ToolSettings::opacityChanged, this, [this] { syncTool(m_settings->tool()); });
    connect(settings, &ToolSettings::toolChanged, this, &SidePanel::syncTool);
    area->installEventFilter(this);
    // Rulers move the viewport without resizing the area.
    m_viewport = area->viewport();
    m_viewport->installEventFilter(this);
    syncTool(settings->tool());
}

bool SidePanel::eventFilter(QObject *watched, QEvent *event)
{
    const bool area = watched == m_area || watched == m_viewport;
    if (area &&
        (event->type() == QEvent::Resize || event->type() == QEvent::Move || event->type() == QEvent::Show)) {
        reposition();
    }
    return QWidget::eventFilter(watched, event);
}

void SidePanel::syncTool(Tool tool)
{
    m_size->setVisible(toolUsesSize(tool));
    m_size->slider()->setValue(m_settings->size());

    QSlider *second = m_second->slider();
    const QSignalBlocker blocker(second);
    if (tool == Tool::Fill) {
        m_second->setIcon(QStringLiteral("tolerance"), tr("Tolerance"));
        m_second->setValueFormat(tr("%1%"));
        second->setRange(0, 100);
        second->setValue(m_settings->fillTolerance());
        m_second->show();
    } else if (toolUsesOpacity(tool)) {
        m_second->setIcon(QStringLiteral("opacity"),
                          tool == Tool::Eraser ? tr("Transparency") : tr("Opacity"));
        m_second->setValueFormat(tr("%1%"));
        second->setRange(1, 100);
        second->setValue(m_settings->opacity());
        m_second->show();
    } else {
        m_second->hide();
    }
    setVisible(m_size->isVisibleTo(this) || m_second->isVisibleTo(this));
    reposition();
}

void SidePanel::reposition()
{
    if (!m_viewport) {
        return;
    }
    adjustSize();
    const QRect area = m_viewport->geometry();
    move(area.left() + PanelInset - 6, area.top() + std::max(0, (area.height() - height()) / 2));
    raise();
}
