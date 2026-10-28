#include "canvas/Ruler.h"

#include "canvas/CanvasView.h"

#include <QPainter>

#include <algorithm>
#include <cmath>

namespace {

// Labels need about this much room.
constexpr double LabelSpacing = 56;
constexpr double MinorSpacing = 5;

} // namespace

Ruler::Ruler(Qt::Orientation orientation, CanvasView *canvas)
    : QWidget(canvas)
    , m_orientation(orientation)
    , m_canvas(canvas)
{
    setObjectName(orientation == Qt::Horizontal ? QStringLiteral("canvasView.horizontalRuler")
                                                : QStringLiteral("canvasView.verticalRuler"));
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAccessibleName(orientation == Qt::Horizontal ? tr("Horizontal ruler") : tr("Vertical ruler"));
}

void Ruler::setOffset(int offset)
{
    if (offset != m_offset) {
        m_offset = offset;
        update();
    }
}

void Ruler::setMarker(std::optional<qreal> position)
{
    if (position != m_marker) {
        m_marker = position;
        update();
    }
}

int Ruler::step(double zoom, double minimumSpacing)
{
    for (int magnitude = 1;; magnitude *= 10) {
        for (int factor : {1, 2, 5}) {
            const int candidate = factor * magnitude;
            if (candidate * zoom >= minimumSpacing || magnitude >= 1'000'000) {
                return candidate;
            }
        }
    }
}

void Ruler::paintEvent(QPaintEvent *)
{
    const bool horizontal = m_orientation == Qt::Horizontal;
    const double zoom = m_canvas->zoom();
    // Document coordinate 0 along this ruler, in this widget's coordinates.
    const QRectF canvas = m_canvas->canvasRect();
    const double origin = m_offset + (horizontal ? canvas.left() : canvas.top());
    const int length = horizontal ? width() : height();

    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Base));
    QColor line = palette().color(QPalette::WindowText);
    line.setAlphaF(0.45);
    // A hairline along the edge that faces the canvas.
    painter.setPen(line);
    if (horizontal) {
        painter.drawLine(0, height() - 1, width(), height() - 1);
    } else {
        painter.drawLine(width() - 1, 0, width() - 1, height());
    }

    const int major = step(zoom, LabelSpacing);
    // Minor ticks split a major step into tenths, fifths or halves, whichever leaves room.
    int minor = major;
    for (int parts : {10, 5, 2}) {
        const int candidate = major / parts; // whole, as `parts` divides `major`
        if (major % parts == 0 && candidate * zoom >= MinorSpacing) {
            minor = candidate;
            break;
        }
    }
    QFont font = this->font();
    font.setPointSizeF(std::max(6.0, font.pointSizeF() * 0.8));
    painter.setFont(font);
    const double first = std::floor((m_offset - origin) / zoom / minor) * minor;
    for (double document = first;; document += minor) {
        const double position = std::round(origin + document * zoom) + 0.5;
        if (position > length) {
            break;
        }
        const int value = int(std::lround(document));
        const bool labelled = value % major == 0;
        const bool half = value % std::max(1, major / 2) == 0;
        const int tick = labelled ? Thickness - 4 : half ? 8 : 4;
        painter.setPen(line);
        if (horizontal) {
            painter.drawLine(QPointF(position, Thickness - tick), QPointF(position, Thickness));
        } else {
            painter.drawLine(QPointF(Thickness - tick, position), QPointF(Thickness, position));
        }
        if (labelled) {
            painter.setPen(palette().color(QPalette::WindowText));
            const QString label = QString::number(value);
            if (horizontal) {
                painter.drawText(QPointF(position + 3, font.pointSizeF() + 3), label);
            } else {
                painter.save();
                painter.translate(font.pointSizeF() + 3, position + 3);
                painter.rotate(90);
                painter.drawText(QPointF(), label);
                painter.restore();
            }
        }
    }

    if (m_marker) {
        // The pointer's position, in the accent colour.
        const double position = std::floor(origin + *m_marker * zoom) + 0.5;
        painter.setPen(QPen(palette().color(QPalette::Highlight), 1));
        if (horizontal) {
            painter.drawLine(QPointF(position, 0), QPointF(position, Thickness));
        } else {
            painter.drawLine(QPointF(0, position), QPointF(Thickness, position));
        }
    }
}
