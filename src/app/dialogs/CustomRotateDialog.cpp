#include "dialogs/CustomRotateDialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

CustomRotateDialog::CustomRotateDialog(QWidget *parent)
    : QDialog(parent)
    , m_angle(new QDoubleSpinBox(this))
{
    setObjectName(QStringLiteral("dialog.customRotate"));
    setWindowTitle(tr("Custom rotate"));
    m_angle->setObjectName(QStringLiteral("customRotate.angle"));
    m_angle->setRange(-360, 360);
    m_angle->setDecimals(1);
    m_angle->setSuffix(QStringLiteral("°"));
    m_angle->setValue(0);

    auto *form = new QFormLayout;
    form->addRow(tr("&Angle:"), m_angle);
    auto *hint = new QLabel(tr("Positive angles turn clockwise."), this);
    hint->setForegroundRole(QPalette::PlaceholderText);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(hint);
    layout->addWidget(buttons);
    layout->setSizeConstraint(QLayout::SetFixedSize);
}

double CustomRotateDialog::angle() const
{
    return m_angle->value();
}
