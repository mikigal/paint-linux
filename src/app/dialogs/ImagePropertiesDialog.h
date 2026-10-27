#pragma once

#include <QDateTime>
#include <QDialog>

class QDoubleSpinBox;
class QRadioButton;

// Paint's "Image properties": file facts, units, colour mode and the canvas size.
class ImagePropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    struct Facts
    {
        QSize size;
        int dotsPerMeter;
        QDateTime lastSaved; // invalid when never saved
        qint64 sizeOnDisk;   // -1 when never saved
        QSize defaultSize;
    };

    ImagePropertiesDialog(const Facts &facts, QWidget *parent = nullptr);

    QSize resultSize() const;
    bool blackAndWhite() const;

private:
    enum class Unit {
        Inches,
        Centimeters,
        Pixels,
    };

    Unit unit() const;
    double fromPixels(int pixels) const;
    int toPixels(double value) const;
    void showSize(QSize size);

    Facts m_facts;
    QRadioButton *m_inches;
    QRadioButton *m_centimeters;
    QRadioButton *m_pixels;
    QRadioButton *m_blackAndWhite;
    QRadioButton *m_color;
    QDoubleSpinBox *m_width;
    QDoubleSpinBox *m_height;
    QSize m_size; // kept in pixels so switching units loses nothing
    bool m_syncing = false;
};
