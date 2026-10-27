#include "dialogs/EditColorsDialog.h"

#include "ribbon/ColorsPanel.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>

namespace {

constexpr QSize FieldSize(240, 180);
constexpr int SliderWidth = 22;
constexpr int BasicColumns = 12;

QColor fromHsv(int hue, int saturation, int value)
{
    return QColor::fromHsv(std::clamp(hue, 0, 359), std::clamp(saturation, 0, 255),
                           std::clamp(value, 0, 255));
}

} // namespace

// Hue runs across, saturation from full (top) to none (bottom), at full brightness.
class HueSaturationField : public QWidget
{
public:
    std::function<void(int hue, int saturation)> picked;

    explicit HueSaturationField(QWidget *parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("editColors.field"));
        setFixedSize(FieldSize);
        setCursor(Qt::CrossCursor);
    }

    void setPosition(int hue, int saturation)
    {
        m_hue = hue;
        m_saturation = saturation;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (m_spectrum.isNull()) {
            m_spectrum = QImage(FieldSize, QImage::Format_RGB32);
            for (int y = 0; y < FieldSize.height(); ++y) {
                for (int x = 0; x < FieldSize.width(); ++x) {
                    m_spectrum.setPixelColor(x, y, fromHsv(hueAt(x), saturationAt(y), 255));
                }
            }
        }
        QPainter painter(this);
        painter.drawImage(0, 0, m_spectrum);
        painter.setRenderHint(QPainter::Antialiasing);
        const QPointF marker((m_hue / 359.0) * (width() - 1), (1 - m_saturation / 255.0) * (height() - 1));
        painter.setPen(QPen(Qt::white, 2));
        painter.drawEllipse(marker, 6, 6);
        painter.setPen(QPen(Qt::black, 1));
        painter.drawEllipse(marker, 7.5, 7.5);
    }

    void mousePressEvent(QMouseEvent *event) override { pick(event->position()); }
    void mouseMoveEvent(QMouseEvent *event) override { pick(event->position()); }

private:
    int hueAt(qreal x) const { return std::clamp(int(std::round(x / (width() - 1) * 359)), 0, 359); }
    int saturationAt(qreal y) const
    {
        return std::clamp(int(std::round((1 - y / (height() - 1)) * 255)), 0, 255);
    }

    void pick(QPointF position)
    {
        if (picked) {
            picked(hueAt(position.x()), saturationAt(position.y()));
        }
    }

    QImage m_spectrum;
    int m_hue = 0;
    int m_saturation = 0;
};

// Brightness from full (top) to black (bottom) for the current hue and saturation.
class ValueSlider : public QWidget
{
public:
    std::function<void(int value)> picked;

    explicit ValueSlider(QWidget *parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("editColors.value"));
        setFixedSize(SliderWidth, FieldSize.height());
        setCursor(Qt::SizeVerCursor);
    }

    void setColor(int hue, int saturation, int value)
    {
        m_hue = hue;
        m_saturation = saturation;
        m_value = value;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        QLinearGradient gradient(0, 0, 0, height());
        gradient.setColorAt(0, fromHsv(m_hue, m_saturation, 255));
        gradient.setColorAt(1, Qt::black);
        painter.fillRect(rect().adjusted(4, 0, -4, 0), gradient);
        // A notch on each side marks the brightness.
        const qreal y = (1 - m_value / 255.0) * (height() - 1);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().color(QPalette::WindowText));
        painter.drawPolygon(QPolygonF{{0, y - 4}, {4, y}, {0, y + 4}});
        painter.drawPolygon(QPolygonF{{qreal(width()), y - 4}, {width() - 4.0, y}, {qreal(width()), y + 4}});
    }

    void mousePressEvent(QMouseEvent *event) override { pick(event->position().y()); }
    void mouseMoveEvent(QMouseEvent *event) override { pick(event->position().y()); }

private:
    void pick(qreal y)
    {
        if (picked) {
            picked(std::clamp(int(std::round((1 - y / (height() - 1)) * 255)), 0, 255));
        }
    }

    int m_hue = 0;
    int m_saturation = 0;
    int m_value = 0;
};

const QList<QColor> &EditColorsDialog::basicColors()
{
    // Four rows of twelve: pure hues, light tints, dark shades and a grey ramp.
    static const QList<QColor> colors = [] {
        QList<QColor> result;
        for (const auto [saturation, value] : {std::pair{255, 255}, {110, 255}, {255, 150}}) {
            for (int i = 0; i < BasicColumns; ++i) {
                // RGB spec, like every colour the dialog hands out (QColor compares specs too).
                result.append(QColor::fromHsv(i * 30, saturation, value).toRgb());
            }
        }
        for (int i = 0; i < BasicColumns; ++i) {
            const int level = 255 - i * 255 / (BasicColumns - 1);
            result.append(QColor(level, level, level));
        }
        return result;
    }();
    return colors;
}

EditColorsDialog::EditColorsDialog(const QColor &initial, const QList<QColor> &customColors, QWidget *parent)
    : QDialog(parent)
    , m_color(initial.isValid() ? initial.toRgb() : QColor(Qt::black))
    , m_field(new HueSaturationField(this))
    , m_value(new ValueSlider(this))
    , m_preview(new QLabel(this))
    , m_hex(new QLineEdit(this))
    , m_mode(new QComboBox(this))
{
    setObjectName(QStringLiteral("dialog.editColors"));
    setWindowTitle(tr("Edit colors"));
    m_preview->setObjectName(QStringLiteral("editColors.preview"));
    m_preview->setFixedSize(56, FieldSize.height());
    m_preview->setAutoFillBackground(true);
    m_hex->setObjectName(QStringLiteral("editColors.hex"));
    m_hex->setValidator(
        new QRegularExpressionValidator(QRegularExpression(QStringLiteral("#?[0-9A-Fa-f]{0,6}")), this));
    m_hex->setMaxLength(7);
    m_mode->setObjectName(QStringLiteral("editColors.mode"));
    m_mode->addItems({tr("RGB"), tr("HSV")});

    auto *top = new QHBoxLayout;
    top->addWidget(m_field);
    top->addWidget(m_value);
    top->addWidget(m_preview);

    auto *entry = new QGridLayout;
    auto *hexLabel = new QLabel(tr("He&x"), this);
    hexLabel->setBuddy(m_hex);
    entry->addWidget(hexLabel, 0, 0);
    entry->addWidget(m_hex, 1, 0);
    entry->addWidget(m_mode, 1, 1);
    for (int i = 0; i < 3; ++i) {
        auto *label = new QLabel(this);
        auto *box = new QSpinBox(this);
        box->setObjectName(QStringLiteral("editColors.channel%1").arg(i));
        m_channelLabels.append(label);
        m_channels.append(box);
        entry->addWidget(label, 0, 2 + i);
        entry->addWidget(box, 1, 2 + i);
        connect(box, &QSpinBox::valueChanged, this, &EditColorsDialog::applySpinBoxes);
    }
    entry->setColumnStretch(0, 1);

    const auto swatchGrid = [this](const QList<QColor> &colors, int columns, const QString &prefix,
                                   QList<ColorSwatch *> *keep) {
        auto *grid = new QGridLayout;
        grid->setSpacing(0);
        for (qsizetype i = 0; i < colors.size(); ++i) {
            auto *swatch = new ColorSwatch(16, this);
            swatch->setObjectName(prefix + QString::number(i));
            swatch->setColor(colors.at(i));
            swatch->setToolTip(colors.at(i).isValid() ? colors.at(i).name().toUpper() : tr("Empty"));
            grid->addWidget(swatch, int(i / columns), int(i % columns));
            connect(swatch, &QAbstractButton::clicked, this, [this, swatch] {
                if (swatch->color().isValid()) {
                    setColor(swatch->color());
                }
            });
            if (keep) {
                keep->append(swatch);
            }
        }
        return grid;
    };

    QList<QColor> custom = customColors;
    custom.resize(CustomColorCount);
    auto *add = new QToolButton(this);
    add->setObjectName(QStringLiteral("editColors.addCustom"));
    add->setText(QStringLiteral("+"));
    add->setToolTip(tr("Add to custom colors"));
    add->setAutoRaise(true);
    connect(add, &QToolButton::clicked, this, &EditColorsDialog::addCustomColor);
    auto *customRow = new QHBoxLayout;
    customRow->addLayout(swatchGrid(custom, BasicColumns, QStringLiteral("editColors.custom."), &m_custom));
    customRow->addWidget(add, 0, Qt::AlignTop);
    customRow->addStretch(1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addLayout(entry);
    layout->addWidget(new QLabel(tr("Basic colors"), this));
    auto *basic = new QHBoxLayout;
    basic->addLayout(swatchGrid(basicColors(), BasicColumns, QStringLiteral("editColors.basic."), nullptr));
    basic->addStretch(1);
    layout->addLayout(basic);
    layout->addWidget(new QLabel(tr("Custom colors"), this));
    layout->addLayout(customRow);
    layout->addWidget(buttons);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    m_field->picked = [this](int hue, int saturation) {
        // Picking a hue from black would stay black, so start from full brightness then.
        m_color = fromHsv(hue, saturation, m_color.value() == 0 ? 255 : m_color.value()).toRgb();
        refresh(m_field);
    };
    m_value->picked = [this](int value) {
        m_color = fromHsv(std::max(m_color.hsvHue(), 0), m_color.hsvSaturation(), value).toRgb();
        refresh(m_value);
    };
    connect(m_hex, &QLineEdit::textEdited, this, [this](const QString &text) {
        const QString digits = text.startsWith(QLatin1Char('#')) ? text.mid(1) : text;
        if (digits.size() == 6) {
            m_color = QColor(QLatin1Char('#') + digits);
            refresh(m_hex);
        }
    });
    connect(m_mode, &QComboBox::currentIndexChanged, this, [this] { refresh(nullptr); });
    refresh(nullptr);
}

void EditColorsDialog::setColor(const QColor &color)
{
    m_color = color.toRgb();
    refresh(nullptr);
}

void EditColorsDialog::refresh(const QObject *source)
{
    // Every view of the colour follows the one that changed it.
    m_updating = true;
    const int hue = std::max(m_color.hsvHue(), 0);
    if (source != m_field) {
        m_field->setPosition(hue, m_color.hsvSaturation());
    }
    m_value->setColor(hue, m_color.hsvSaturation(), m_color.value());
    QPalette preview = m_preview->palette();
    preview.setColor(QPalette::Window, m_color);
    m_preview->setPalette(preview);
    if (source != m_hex) {
        m_hex->setText(m_color.name().toUpper());
    }
    const bool rgb = m_mode->currentIndex() == 0;
    const QStringList names =
        rgb ? QStringList{tr("Red"), tr("Green"), tr("Blue")} : QStringList{tr("Hue"), tr("Sat"), tr("Val")};
    const QList<int> maxima = rgb ? QList<int>{255, 255, 255} : QList<int>{359, 100, 100};
    const QList<int> values = rgb ? QList<int>{m_color.red(), m_color.green(), m_color.blue()}
                                  : QList<int>{hue, int(std::round(m_color.hsvSaturation() * 100 / 255.0)),
                                               int(std::round(m_color.value() * 100 / 255.0))};
    for (int i = 0; i < 3; ++i) {
        m_channelLabels.at(i)->setText(names.at(i));
        m_channels.at(i)->setRange(0, maxima.at(i));
        if (source != m_channels.at(i)) {
            m_channels.at(i)->setValue(values.at(i));
        }
    }
    m_updating = false;
}

void EditColorsDialog::applySpinBoxes()
{
    if (m_updating) {
        return;
    }
    const int a = m_channels.at(0)->value();
    const int b = m_channels.at(1)->value();
    const int c = m_channels.at(2)->value();
    m_color = m_mode->currentIndex() == 0
                  ? QColor(a, b, c)
                  : fromHsv(a, int(std::round(b * 2.55)), int(std::round(c * 2.55))).toRgb();
    refresh(sender());
}

void EditColorsDialog::addCustomColor()
{
    // The first empty slot, or make room by dropping the oldest colour.
    for (ColorSwatch *swatch : std::as_const(m_custom)) {
        if (!swatch->color().isValid()) {
            swatch->setColor(m_color);
            swatch->setToolTip(m_color.name().toUpper());
            return;
        }
    }
    for (qsizetype i = 0; i + 1 < m_custom.size(); ++i) {
        m_custom.at(i)->setColor(m_custom.at(i + 1)->color());
        m_custom.at(i)->setToolTip(m_custom.at(i)->color().name().toUpper());
    }
    m_custom.last()->setColor(m_color);
    m_custom.last()->setToolTip(m_color.name().toUpper());
}

QList<QColor> EditColorsDialog::customColors() const
{
    QList<QColor> colors;
    for (const ColorSwatch *swatch : m_custom) {
        colors.append(swatch->color());
    }
    return colors;
}
