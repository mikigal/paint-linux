#pragma once

#include "tools/ToolTypes.h"

#include <QFrame>

class QAbstractScrollArea;
class QLabel;
class QSlider;

namespace paint {
class ToolSettings;
}

// One floating card of the side panel: an icon over a vertical slider.
class SliderCard : public QFrame
{
    Q_OBJECT

public:
    explicit SliderCard(const QString &name, QWidget *parent = nullptr);

    QSlider *slider() const { return m_slider; }
    void setIcon(const QString &iconName, const QString &toolTip);
    // Value shown next to the handle while dragging, e.g. "40%" or "12 px".
    void setValueFormat(const QString &format) { m_format = format; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void showValue(int value);

    QLabel *m_icon;
    QSlider *m_slider;
    QString m_iconName;
    QString m_format;
};

// Paint's floating panel on the left of the workspace: size and opacity sliders, or the
// fill tolerance, depending on the active tool.
class SidePanel : public QWidget
{
    Q_OBJECT

public:
    // Floats over the left edge of `area`'s viewport and follows its size.
    SidePanel(paint::ToolSettings *settings, QAbstractScrollArea *area);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void syncTool(paint::Tool tool);
    void reposition();

    QAbstractScrollArea *m_area;

    paint::ToolSettings *m_settings;
    SliderCard *m_size;
    SliderCard *m_second;
};
