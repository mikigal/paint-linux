#pragma once

#include <QWidget>

class ActionRegistry;
class CanvasView;
class QComboBox;
class QSlider;

// The status bar's zoom controls: fit to window, the zoom box, zoom out, slider, zoom in.
class ZoomControls : public QWidget
{
    Q_OBJECT

public:
    ZoomControls(CanvasView *canvas, ActionRegistry *actions, QWidget *parent = nullptr);

    // The slider is logarithmic between the minimum and maximum zoom.
    static int sliderPosition(double zoom);
    static double zoomForSliderPosition(int position);

private:
    void syncZoom(double zoom);
    void applyTypedZoom();

    CanvasView *m_canvas;
    QComboBox *m_box;
    QSlider *m_slider;
};

// Formats a zoom factor the way Paint does ("58.5%", "100%").
QString formatZoom(double zoom);
