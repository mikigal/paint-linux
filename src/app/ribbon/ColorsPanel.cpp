#include "ribbon/ColorsPanel.h"

#include "color/DefaultPalette.h"
#include "ribbon/RibbonButton.h"
#include "tools/ToolSettings.h"
#include "ui/Icons.h"
#include "ui/Names.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>
#include <QWidgetAction>

using namespace paint;

ColorSwatch::ColorSwatch(int diameter, QWidget *parent)
    : QAbstractButton(parent)
    , m_diameter(diameter)
{
    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::TabFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void ColorSwatch::setColor(const QColor &color)
{
    if (color != m_color) {
        m_color = color;
        update();
    }
}

void ColorSwatch::setActive(bool active)
{
    if (active != m_active) {
        m_active = active;
        update();
    }
}

QSize ColorSwatch::sizeHint() const
{
    // Wells reserve room for the ring around them; swatches sit on Paint's 24 px pitch.
    const int side = m_diameter >= 24 ? m_diameter + 10 : 24;
    return {side, side};
}

void ColorSwatch::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPointF centre = QRectF(rect()).center();
    const qreal radius = m_diameter / 2.0;

    QColor outline = palette().color(QPalette::WindowText);
    outline.setAlphaF(m_color.isValid() ? 0.18 : 0.4);

    if (m_active || underMouse() || hasFocus()) {
        QColor ring = palette().color(QPalette::Highlight);
        if (!m_active) {
            ring.setAlphaF(0.6);
        }
        painter.setPen(QPen(ring, m_active ? 2.0 : 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(centre, radius + 3.5, radius + 3.5);
    }

    painter.setPen(QPen(outline, 1.0));
    painter.setBrush(m_color.isValid() ? QBrush(m_color) : Qt::NoBrush);
    painter.drawEllipse(centre, radius - 0.5, radius - 0.5);
}

void ColorSwatch::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        Q_EMIT rightClicked();
        event->accept();
        return;
    }
    QAbstractButton::mousePressEvent(event);
}

ColorsPanel::ColorsPanel(ToolSettings *settings, QAction *editColors, Mode mode, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_primary(new ColorSwatch(26, this))
    , m_secondary(new ColorSwatch(26, this))
{
    const QString prefix = mode == Mode::Full ? QStringLiteral("ribbon.") : QStringLiteral("ribbon.compact.");
    setObjectName(prefix + QStringLiteral("colors"));
    m_primary->setObjectName(prefix + QStringLiteral("color1"));
    m_primary->setToolTip(tr("Color 1"));
    m_secondary->setObjectName(prefix + QStringLiteral("color2"));
    m_secondary->setToolTip(tr("Color 2"));
    connect(m_primary, &QAbstractButton::clicked, this,
            [this] { m_settings->setActiveColorSlot(ColorSlot::Primary); });
    connect(m_secondary, &QAbstractButton::clicked, this,
            [this] { m_settings->setActiveColorSlot(ColorSlot::Secondary); });

    auto *wells = new QVBoxLayout;
    wells->setSpacing(0);
    wells->addWidget(m_primary);
    wells->addWidget(m_secondary);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addLayout(wells);

    if (mode == Mode::Compact) {
        layout->addWidget(createCompactDropDown(settings, editColors));
    } else {
        layout->addLayout(createPaletteGrid());
        auto *edit = new RibbonButton(RibbonButton::Kind::Large, this);
        edit->setObjectName(QStringLiteral("ribbon.editColors"));
        edit->setDefaultAction(editColors);
        edit->setIconSize(QSize(28, 28));
        layout->addWidget(edit);
    }

    connect(settings, &ToolSettings::colorChanged, this, &ColorsPanel::refresh);
    connect(settings, &ToolSettings::activeColorSlotChanged, this, &ColorsPanel::refresh);
    connect(settings, &ToolSettings::customColorsChanged, this, &ColorsPanel::refresh);
    refresh();
}

QWidget *ColorsPanel::createCompactDropDown(ToolSettings *settings, QAction *editColors)
{
    auto *more = new RibbonButton(RibbonButton::Kind::LargeDropDown, this);
    more->setObjectName(QStringLiteral("ribbon.colorsCompact"));
    more->setRibbonIcon(ui::icon(QStringLiteral("edit-colors")));
    more->setToolTip(tr("Colors"));
    auto *menu = new QMenu(more);
    menu->setObjectName(QStringLiteral("menu.ribbon.colors"));
    auto *palette = new QWidgetAction(menu);
    auto *full = new ColorsPanel(settings, editColors, Mode::Full, menu);
    full->setContentsMargins(8, 8, 8, 8);
    palette->setDefaultWidget(full);
    menu->addAction(palette);
    more->setMenu(menu);
    return more;
}

QGridLayout *ColorsPanel::createPaletteGrid()
{
    auto *grid = new QGridLayout;
    grid->setSpacing(0);
    const auto pick = [this](ColorSwatch *swatch, bool rightButton) {
        if (!swatch->color().isValid()) {
            return;
        }
        // Left click sets the active colour, right click always sets colour 2 (as in Paint).
        m_settings->setColor(rightButton ? ColorSlot::Secondary : m_settings->activeColorSlot(),
                             swatch->color());
    };
    for (int i = 0; i < int(DefaultPalette.size()); ++i) {
        auto *swatch = new ColorSwatch(16, this);
        swatch->setObjectName(QStringLiteral("ribbon.palette.%1").arg(i));
        swatch->setColor(QColor::fromRgb(DefaultPalette.at(i)));
        swatch->setToolTip(ui::paletteColorName(i));
        grid->addWidget(swatch, i / DefaultPaletteColumns, i % DefaultPaletteColumns);
        connect(swatch, &QAbstractButton::clicked, this, [pick, swatch] { pick(swatch, false); });
        connect(swatch, &ColorSwatch::rightClicked, this, [pick, swatch] { pick(swatch, true); });
    }
    for (int i = 0; i < ToolSettings::CustomColorCount; ++i) {
        auto *swatch = new ColorSwatch(16, this);
        swatch->setObjectName(QStringLiteral("ribbon.customColor.%1").arg(i));
        grid->addWidget(swatch, 2, i);
        m_custom.append(swatch);
        connect(swatch, &QAbstractButton::clicked, this, [pick, swatch] { pick(swatch, false); });
        connect(swatch, &ColorSwatch::rightClicked, this, [pick, swatch] { pick(swatch, true); });
    }
    return grid;
}

void ColorsPanel::refresh()
{
    m_primary->setColor(m_settings->color(ColorSlot::Primary));
    m_secondary->setColor(m_settings->color(ColorSlot::Secondary));
    m_primary->setActive(m_settings->activeColorSlot() == ColorSlot::Primary);
    m_secondary->setActive(m_settings->activeColorSlot() == ColorSlot::Secondary);
    const QList<QColor> custom = m_settings->customColors();
    for (qsizetype i = 0; i < m_custom.size(); ++i) {
        m_custom.at(i)->setColor(custom.at(i));
        m_custom.at(i)->setToolTip(custom.at(i).isValid() ? custom.at(i).name().toUpper()
                                                          : tr("Custom color"));
    }
}
