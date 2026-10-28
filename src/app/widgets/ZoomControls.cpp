#include "widgets/ZoomControls.h"

#include "ActionRegistry.h"
#include "canvas/CanvasView.h"
#include "ui/Accessibility.h"
#include "ui/Icons.h"

#include <QAction>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLocale>
#include <QSlider>
#include <QToolButton>

#include <cmath>
#include <ranges>

namespace {

constexpr int SliderSteps = 1000;

QToolButton *zoomButton(QAction *action, const QString &iconName, const QString &name, QWidget *parent)
{
    // Paint-style icons here; the actions keep their theme icons for menus.
    auto *button = new QToolButton(parent);
    button->setObjectName(name);
    button->setAutoRaise(true);
    button->setIcon(paint::ui::icon(iconName));
    button->setToolTip(action->toolTip());
    paint::ui::nameAfterToolTip(button);
    QObject::connect(button, &QToolButton::clicked, action, &QAction::trigger);
    return button;
}

} // namespace

QString formatZoom(double zoom)
{
    // At most one decimal, without trailing zeros: "12.5%", "58.5%", "100%".
    return QLocale::c().toString(std::round(zoom * 1000) / 10, 'g', 6) + QLatin1Char('%');
}

int ZoomControls::sliderPosition(double zoom)
{
    return int(std::round(SliderSteps * std::log(zoom / CanvasView::MinZoom) /
                          std::log(CanvasView::MaxZoom / CanvasView::MinZoom)));
}

double ZoomControls::zoomForSliderPosition(int position)
{
    return CanvasView::MinZoom *
           std::pow(CanvasView::MaxZoom / CanvasView::MinZoom, double(position) / SliderSteps);
}

ZoomControls::ZoomControls(CanvasView *canvas, ActionRegistry *actions, QWidget *parent)
    : QWidget(parent)
    , m_canvas(canvas)
    , m_box(new QComboBox(this))
    , m_slider(new QSlider(Qt::Horizontal, this))
{
    setObjectName(QStringLiteral("status.zoom"));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_box->setObjectName(QStringLiteral("status.zoom.box"));
    m_box->setEditable(true);
    m_box->setInsertPolicy(QComboBox::NoInsert);
    m_box->setToolTip(tr("Zoom"));
    for (double preset : CanvasView::zoomPresets() | std::views::reverse) {
        m_box->addItem(formatZoom(preset), preset);
    }
    m_box->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_box->setFixedWidth(96);
    m_slider->setObjectName(QStringLiteral("status.zoom.slider"));
    m_slider->setRange(0, SliderSteps);
    m_slider->setFixedWidth(110);
    m_slider->setToolTip(tr("Zoom"));
    m_slider->setAccessibleName(tr("Zoom"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(zoomButton(actions->get(ActionId::ZoomFit), QStringLiteral("fit-to-window"),
                                 QStringLiteral("status.zoom.fit"), this));
    layout->addWidget(m_box);
    layout->addSpacing(6);
    layout->addWidget(zoomButton(actions->get(ActionId::ZoomOut), QStringLiteral("zoom-out"),
                                 QStringLiteral("status.zoom.out"), this));
    layout->addWidget(m_slider);
    layout->addWidget(zoomButton(actions->get(ActionId::ZoomIn), QStringLiteral("zoom-in"),
                                 QStringLiteral("status.zoom.in"), this));

    connect(m_box, &QComboBox::activated, this,
            [this](int index) { m_canvas->setZoom(m_box->itemData(index).toDouble()); });
    connect(m_box->lineEdit(), &QLineEdit::editingFinished, this, &ZoomControls::applyTypedZoom);
    connect(m_slider, &QSlider::valueChanged, this, [this](int position) {
        // Ignore the rounding echo of syncZoom().
        if (sliderPosition(m_canvas->zoom()) != position) {
            m_canvas->setZoom(zoomForSliderPosition(position));
        }
    });
    connect(canvas, &CanvasView::zoomChanged, this, &ZoomControls::syncZoom);
    syncZoom(canvas->zoom());
}

void ZoomControls::syncZoom(double zoom)
{
    const QSignalBlocker blockSlider(m_slider);
    m_slider->setValue(sliderPosition(zoom));
    m_box->setEditText(formatZoom(zoom));
}

void ZoomControls::applyTypedZoom()
{
    QString text = m_box->currentText().trimmed();
    text.remove(QLatin1Char('%'));
    bool ok = false;
    const double percent = QLocale::c().toDouble(text.replace(QLatin1Char(','), QLatin1Char('.')), &ok);
    if (ok && percent > 0) {
        m_canvas->setZoom(percent / 100.0);
    }
    syncZoom(m_canvas->zoom());
}
