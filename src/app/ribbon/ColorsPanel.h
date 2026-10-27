#pragma once

#include <QAbstractButton>
#include <QList>

class QAction;
class QGridLayout;

namespace paint {
class ToolSettings;
}

// A round colour button: Paint's palette swatches and the Colour 1 / Colour 2 wells.
class ColorSwatch : public QAbstractButton
{
    Q_OBJECT

public:
    ColorSwatch(int diameter, QWidget *parent = nullptr);

    QColor color() const { return m_color; }
    void setColor(const QColor &color); // invalid = empty custom slot
    // Shows the accent ring that marks the active colour slot.
    void setActive(bool active);

    QSize sizeHint() const override;

Q_SIGNALS:
    void rightClicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    int m_diameter;
    QColor m_color;
    bool m_active = false;
};

// The Colors group: the two colour wells, the preset palette, custom colours and Edit colors.
// The compact form keeps the wells and moves the rest into a drop-down.
class ColorsPanel : public QWidget
{
    Q_OBJECT

public:
    enum class Mode {
        Full,
        Compact,
    };

    ColorsPanel(paint::ToolSettings *settings, QAction *editColors, Mode mode = Mode::Full,
                QWidget *parent = nullptr);

private:
    QWidget *createCompactDropDown(paint::ToolSettings *settings, QAction *editColors);
    QGridLayout *createPaletteGrid();
    void refresh();

    paint::ToolSettings *m_settings;
    ColorSwatch *m_primary;
    ColorSwatch *m_secondary;
    QList<ColorSwatch *> m_custom;
};
