#include "tools/ClickTools.h"

#include "document/Document.h"
#include "document/History.h"
#include "ops/FloodFill.h"
#include "tools/ToolSettings.h"

#include <QCoreApplication>

#include <cmath>

namespace paint {

namespace {

ColorSlot slotFor(Qt::MouseButton button)
{
    return button == Qt::RightButton ? ColorSlot::Secondary : ColorSlot::Primary;
}

QPoint pixelAt(QPointF position)
{
    return QPoint(int(std::floor(position.x())), int(std::floor(position.y())));
}

} // namespace

void FillTool::press(const PointerEvent &event)
{
    if (event.button != Qt::LeftButton && event.button != Qt::RightButton) {
        return;
    }
    Document &document = m_context.document();
    const QPoint seed = pixelAt(event.position);
    if (!document.rect().contains(seed)) {
        return;
    }
    const qsizetype layer = document.activeLayerIndex();
    QImage &image = document.layer(layer).image;
    const QImage before = image; // shared until the fill writes
    const QRect changed = floodFill(image, seed, m_context.settings().color(slotFor(event.button)),
                                    m_context.settings().fillTolerance());
    if (changed.isEmpty()) {
        return;
    }
    m_context.history().recordPixelEdit(QCoreApplication::translate("paint::Tool", "Fill"), layer, changed,
                                        before.copy(changed));
    m_context.documentChanged(changed);
}

void ColorPickerTool::press(const PointerEvent &event)
{
    if (event.button != Qt::LeftButton && event.button != Qt::RightButton) {
        return;
    }
    Document &document = m_context.document();
    const QPoint pixel = pixelAt(event.position);
    if (!document.rect().contains(pixel)) {
        return;
    }
    const QColor color = document.composite(QRect(pixel, QSize(1, 1))).pixelColor(0, 0);
    // A fully transparent spot (hidden background) has no colour to take.
    if (color.alpha() == 0) {
        return;
    }
    ToolSettings &settings = m_context.settings();
    settings.setColor(slotFor(event.button), QColor(color.rgb()));
    settings.setTool(settings.previousTool() == Tool::ColorPicker ? Tool::Pencil : settings.previousTool());
}

void MagnifierTool::press(const PointerEvent &event)
{
    if (event.button == Qt::LeftButton || event.button == Qt::RightButton) {
        m_context.requestZoom(event.position, event.button == Qt::LeftButton);
    }
}

} // namespace paint
