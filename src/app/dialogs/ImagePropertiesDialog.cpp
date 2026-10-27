#include "dialogs/ImagePropertiesDialog.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include <cmath>

namespace {

constexpr int MaxPixels = 20000;
constexpr double MetersPerInch = 0.0254;

} // namespace

ImagePropertiesDialog::ImagePropertiesDialog(const Facts &facts, QWidget *parent)
    : QDialog(parent)
    , m_facts(facts)
    , m_inches(new QRadioButton(tr("&Inches"), this))
    , m_centimeters(new QRadioButton(tr("&Centimeters"), this))
    , m_pixels(new QRadioButton(tr("&Pixels"), this))
    , m_blackAndWhite(new QRadioButton(tr("&Black and white"), this))
    , m_color(new QRadioButton(tr("C&olor"), this))
    , m_width(new QDoubleSpinBox(this))
    , m_height(new QDoubleSpinBox(this))
    , m_size(facts.size)
{
    setObjectName(QStringLiteral("dialog.imageProperties"));
    setWindowTitle(tr("Image properties"));
    m_inches->setObjectName(QStringLiteral("imageProperties.inches"));
    m_centimeters->setObjectName(QStringLiteral("imageProperties.centimeters"));
    m_pixels->setObjectName(QStringLiteral("imageProperties.pixels"));
    m_blackAndWhite->setObjectName(QStringLiteral("imageProperties.blackAndWhite"));
    m_color->setObjectName(QStringLiteral("imageProperties.color"));
    m_width->setObjectName(QStringLiteral("imageProperties.width"));
    m_height->setObjectName(QStringLiteral("imageProperties.height"));

    const QLocale locale;
    const QString unavailable = tr("Not available");
    auto *attributes = new QGroupBox(tr("File attributes"), this);
    auto *attributesForm = new QFormLayout(attributes);
    attributesForm->addRow(tr("Last saved:"),
                           new QLabel(facts.lastSaved.isValid()
                                          ? locale.toString(facts.lastSaved, QLocale::ShortFormat)
                                          : unavailable,
                                      attributes));
    attributesForm->addRow(
        tr("Size on disk:"),
        new QLabel(facts.sizeOnDisk >= 0 ? locale.formattedDataSize(facts.sizeOnDisk) : unavailable,
                   attributes));
    const int dpi = int(std::round(facts.dotsPerMeter * MetersPerInch));
    attributesForm->addRow(tr("Resolution:"), new QLabel(tr("%1 DPI").arg(dpi), attributes));

    auto *units = new QGroupBox(tr("Units"), this);
    auto *unitsLayout = new QHBoxLayout(units);
    auto *unitGroup = new QButtonGroup(this);
    for (QRadioButton *button : {m_inches, m_centimeters, m_pixels}) {
        unitGroup->addButton(button);
        unitsLayout->addWidget(button);
    }
    m_pixels->setChecked(true);

    auto *colors = new QGroupBox(tr("Colors"), this);
    auto *colorsLayout = new QHBoxLayout(colors);
    auto *colorGroup = new QButtonGroup(this);
    for (QRadioButton *button : {m_blackAndWhite, m_color}) {
        colorGroup->addButton(button);
        colorsLayout->addWidget(button);
    }
    m_color->setChecked(true);

    auto *size = new QGroupBox(tr("Image size"), this);
    auto *sizeForm = new QFormLayout(size);
    sizeForm->addRow(tr("&Width:"), m_width);
    sizeForm->addRow(tr("&Height:"), m_height);
    auto *defaults = new QPushButton(tr("&Default"), size);
    defaults->setObjectName(QStringLiteral("imageProperties.default"));
    sizeForm->addRow(QString(), defaults);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(attributes);
    layout->addWidget(units);
    layout->addWidget(colors);
    layout->addWidget(size);
    layout->addWidget(buttons);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    connect(unitGroup, &QButtonGroup::buttonToggled, this, [this](QAbstractButton *, bool checked) {
        if (checked) {
            showSize(m_size);
        }
    });
    const auto edited = [this] {
        if (!m_syncing) {
            m_size = QSize(toPixels(m_width->value()), toPixels(m_height->value()));
        }
    };
    connect(m_width, &QDoubleSpinBox::valueChanged, this, edited);
    connect(m_height, &QDoubleSpinBox::valueChanged, this, edited);
    connect(defaults, &QPushButton::clicked, this, [this] { showSize(m_facts.defaultSize); });
    showSize(m_size);
}

ImagePropertiesDialog::Unit ImagePropertiesDialog::unit() const
{
    return m_inches->isChecked()        ? Unit::Inches
           : m_centimeters->isChecked() ? Unit::Centimeters
                                        : Unit::Pixels;
}

double ImagePropertiesDialog::fromPixels(int pixels) const
{
    const double inches = pixels / (m_facts.dotsPerMeter * MetersPerInch);
    switch (unit()) {
    case Unit::Inches:
        return inches;
    case Unit::Centimeters:
        return inches * 2.54;
    case Unit::Pixels:
        break;
    }
    return pixels;
}

int ImagePropertiesDialog::toPixels(double value) const
{
    double inches = value;
    switch (unit()) {
    case Unit::Inches:
        break;
    case Unit::Centimeters:
        inches = value / 2.54;
        break;
    case Unit::Pixels:
        return std::max(1, int(std::round(value)));
    }
    return std::max(1, int(std::round(inches * m_facts.dotsPerMeter * MetersPerInch)));
}

void ImagePropertiesDialog::showSize(QSize size)
{
    m_syncing = true;
    const bool pixels = unit() == Unit::Pixels;
    for (QDoubleSpinBox *box : {m_width, m_height}) {
        box->setDecimals(pixels ? 0 : 2);
        box->setRange(pixels ? 1 : 0.01, fromPixels(MaxPixels));
        box->setSingleStep(pixels ? 1 : 0.1);
    }
    m_width->setValue(fromPixels(size.width()));
    m_height->setValue(fromPixels(size.height()));
    m_size = size;
    m_syncing = false;
}

QSize ImagePropertiesDialog::resultSize() const
{
    return m_size;
}

bool ImagePropertiesDialog::blackAndWhite() const
{
    return m_blackAndWhite->isChecked();
}
