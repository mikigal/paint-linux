#pragma once

#include <QDialog>

class QDoubleSpinBox;

// Paint's "Custom rotate": turn the selection or the image by any angle.
class CustomRotateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CustomRotateDialog(QWidget *parent = nullptr);

    // Degrees, clockwise.
    double angle() const;

private:
    QDoubleSpinBox *m_angle;
};
