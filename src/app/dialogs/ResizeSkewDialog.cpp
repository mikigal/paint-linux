#include "dialogs/ResizeSkewDialog.h"

#include "ui/Icons.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <cmath>

namespace {

constexpr int MaxPercent = 500;
constexpr int MaxPixels = 20000;
constexpr int MaxSkew = 89;

} // namespace

ResizeSkewDialog::ResizeSkewDialog(QSize original, QWidget *parent)
    : QDialog(parent)
    , m_original(original.expandedTo(QSize(1, 1)))
    , m_percentage(new QRadioButton(tr("&Percentage"), this))
    , m_pixels(new QRadioButton(tr("Pi&xels"), this))
    , m_horizontal(new QSpinBox(this))
    , m_vertical(new QSpinBox(this))
    , m_keepRatio(new QCheckBox(tr("&Maintain aspect ratio"), this))
    , m_skewHorizontal(new QSpinBox(this))
    , m_skewVertical(new QSpinBox(this))
{
    setObjectName(QStringLiteral("dialog.resizeSkew"));
    setWindowTitle(tr("Resize and skew"));
    m_percentage->setObjectName(QStringLiteral("resizeSkew.percentage"));
    m_pixels->setObjectName(QStringLiteral("resizeSkew.pixels"));
    m_horizontal->setObjectName(QStringLiteral("resizeSkew.horizontal"));
    m_vertical->setObjectName(QStringLiteral("resizeSkew.vertical"));
    m_keepRatio->setObjectName(QStringLiteral("resizeSkew.keepRatio"));
    m_skewHorizontal->setObjectName(QStringLiteral("resizeSkew.skewHorizontal"));
    m_skewVertical->setObjectName(QStringLiteral("resizeSkew.skewVertical"));

    auto *units = new QButtonGroup(this);
    units->addButton(m_percentage);
    units->addButton(m_pixels);
    m_percentage->setChecked(true);
    m_keepRatio->setChecked(true);
    m_horizontal->setRange(1, MaxPercent);
    m_vertical->setRange(1, MaxPercent);
    m_horizontal->setValue(100);
    m_vertical->setValue(100);
    for (QSpinBox *skew : {m_skewHorizontal, m_skewVertical}) {
        skew->setRange(-MaxSkew, MaxSkew);
        skew->setSuffix(QStringLiteral("°"));
    }

    auto *resize = new QGroupBox(tr("Resize"), this);
    auto *unitRow = new QHBoxLayout;
    unitRow->addWidget(m_percentage);
    unitRow->addWidget(m_pixels);
    unitRow->addStretch(1);
    auto *resizeForm = new QFormLayout;
    const auto labelled = [this](const QString &icon, const QString &text) {
        // Paint shows small arrows before the fields; the theme's icons stand in for them.
        auto *row = new QWidget(this);
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        auto *picture = new QLabel(row);
        picture->setPixmap(paint::ui::icon(icon).pixmap(QSize(16, 16), devicePixelRatioF()));
        layout->addWidget(picture);
        layout->addWidget(new QLabel(text, row));
        return row;
    };
    resizeForm->addRow(labelled(QStringLiteral("flip-horizontal"), tr("Horizontal:")), m_horizontal);
    resizeForm->addRow(labelled(QStringLiteral("flip-vertical"), tr("Vertical:")), m_vertical);
    auto *resizeLayout = new QVBoxLayout(resize);
    resizeLayout->addLayout(unitRow);
    resizeLayout->addLayout(resizeForm);
    resizeLayout->addWidget(m_keepRatio);

    auto *skew = new QGroupBox(tr("Skew (degrees)"), this);
    auto *skewForm = new QFormLayout(skew);
    skewForm->addRow(tr("Hori&zontal:"), m_skewHorizontal);
    skewForm->addRow(tr("Ver&tical:"), m_skewVertical);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(resize);
    layout->addWidget(skew);
    layout->addWidget(buttons);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    connect(units, &QButtonGroup::buttonToggled, this, [this](QAbstractButton *, bool checked) {
        if (checked) {
            switchUnits();
        }
    });
    connect(m_horizontal, &QSpinBox::valueChanged, this, &ResizeSkewDialog::horizontalChanged);
    connect(m_vertical, &QSpinBox::valueChanged, this, &ResizeSkewDialog::verticalChanged);
    connect(m_keepRatio, &QCheckBox::toggled, this, [this](bool keep) {
        if (keep) {
            horizontalChanged(m_horizontal->value());
        }
    });
}

bool ResizeSkewDialog::percentage() const
{
    return m_percentage->isChecked();
}

void ResizeSkewDialog::switchUnits()
{
    // Keep the same result size when switching between percentages and pixels.
    const QSizeF current = percentage()
                               ? QSizeF(m_horizontal->value(), m_vertical->value())         // were pixels
                               : QSizeF(m_horizontal->value() * m_original.width() / 100.0, // were percent
                                        m_vertical->value() * m_original.height() / 100.0);
    m_syncing = true;
    if (percentage()) {
        m_horizontal->setRange(1, MaxPercent);
        m_vertical->setRange(1, MaxPercent);
        m_horizontal->setValue(int(std::round(current.width() * 100 / m_original.width())));
        m_vertical->setValue(int(std::round(current.height() * 100 / m_original.height())));
    } else {
        m_horizontal->setRange(1, MaxPixels);
        m_vertical->setRange(1, MaxPixels);
        m_horizontal->setValue(int(std::round(current.width())));
        m_vertical->setValue(int(std::round(current.height())));
    }
    m_syncing = false;
}

void ResizeSkewDialog::horizontalChanged(int value)
{
    if (m_syncing || !m_keepRatio->isChecked()) {
        return;
    }
    m_syncing = true;
    m_vertical->setValue(
        percentage() ? value : int(std::round(value * double(m_original.height()) / m_original.width())));
    m_syncing = false;
}

void ResizeSkewDialog::verticalChanged(int value)
{
    if (m_syncing || !m_keepRatio->isChecked()) {
        return;
    }
    m_syncing = true;
    m_horizontal->setValue(
        percentage() ? value : int(std::round(value * double(m_original.width()) / m_original.height())));
    m_syncing = false;
}

QSize ResizeSkewDialog::resultSize() const
{
    if (percentage()) {
        return QSize(std::max(1, int(std::round(m_original.width() * m_horizontal->value() / 100.0))),
                     std::max(1, int(std::round(m_original.height() * m_vertical->value() / 100.0))));
    }
    return QSize(m_horizontal->value(), m_vertical->value());
}

int ResizeSkewDialog::skewHorizontal() const
{
    return m_skewHorizontal->value();
}

int ResizeSkewDialog::skewVertical() const
{
    return m_skewVertical->value();
}

bool ResizeSkewDialog::changesAnything() const
{
    return resultSize() != m_original || skewHorizontal() != 0 || skewVertical() != 0;
}
