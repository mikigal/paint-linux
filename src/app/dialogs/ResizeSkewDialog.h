#pragma once

#include <QDialog>

class QCheckBox;
class QRadioButton;
class QSpinBox;

// Paint's "Resize and skew" dialog: resize by percentage or pixels (optionally keeping the aspect
// ratio), then skew horizontally and vertically by an angle.
class ResizeSkewDialog : public QDialog
{
    Q_OBJECT

public:
    // `original` is the size of the selection, or of the image when nothing is selected.
    ResizeSkewDialog(QSize original, QWidget *parent = nullptr);

    QSize resultSize() const;
    int skewHorizontal() const;
    int skewVertical() const;
    bool changesAnything() const;

private:
    bool percentage() const;
    void switchUnits();
    void horizontalChanged(int value);
    void verticalChanged(int value);

    QSize m_original;
    QRadioButton *m_percentage;
    QRadioButton *m_pixels;
    QSpinBox *m_horizontal;
    QSpinBox *m_vertical;
    QCheckBox *m_keepRatio;
    QSpinBox *m_skewHorizontal;
    QSpinBox *m_skewVertical;
    bool m_syncing = false;
};
