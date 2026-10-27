#pragma once

#include <QColor>
#include <QDialog>
#include <QList>

class ColorSwatch;
class HueSaturationField;
class ValueSlider;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QLabel;

// Paint's "Edit colors": a hue × saturation field with a brightness slider, a preview, hex and
// RGB/HSV entry, 48 basic colours and 24 custom colours that the user can add to.
class EditColorsDialog : public QDialog
{
    Q_OBJECT

public:
    static constexpr int CustomColorCount = 24;

    EditColorsDialog(const QColor &initial, const QList<QColor> &customColors, QWidget *parent = nullptr);

    QColor color() const { return m_color; }
    // The custom colours (invalid colours are empty slots), to remember for next time.
    QList<QColor> customColors() const;
    static const QList<QColor> &basicColors();

    void setColor(const QColor &color);

private:
    void refresh(const QObject *source);
    void applySpinBoxes();
    void addCustomColor();

    QColor m_color;
    HueSaturationField *m_field;
    ValueSlider *m_value;
    QLabel *m_preview;
    QLineEdit *m_hex;
    QComboBox *m_mode;
    QList<QLabel *> m_channelLabels;
    QList<QSpinBox *> m_channels;
    QList<ColorSwatch *> m_custom;
    bool m_updating = false;
};
