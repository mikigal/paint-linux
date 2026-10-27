#include "shapes/ShapeStyles.h"

#include <QImage>
#include <QPainter>
#include <QPainterPathStroker>

#include <algorithm>
#include <cmath>

namespace paint {

namespace {

// Deterministic per-pixel noise in [0, 1].
float whiteNoise(int x, int y)
{
    quint32 h = quint32(x) * 374761393u + quint32(y) * 668265263u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    h ^= h >> 16u;
    return float(h & 0xffffu) / 65535.f;
}

// Smooth noise in [0, 1] with features about one unit apart.
float valueNoise(float x, float y)
{
    const float fx = std::floor(x);
    const float fy = std::floor(y);
    const int x0 = int(fx);
    const int y0 = int(fy);
    const auto smooth = [](float t) { return t * t * (3 - 2 * t); };
    const float tx = smooth(x - fx);
    const float ty = smooth(y - fy);
    const float top = whiteNoise(x0, y0) + (whiteNoise(x0 + 1, y0) - whiteNoise(x0, y0)) * tx;
    const float bottom = whiteNoise(x0, y0 + 1) + (whiteNoise(x0 + 1, y0 + 1) - whiteNoise(x0, y0 + 1)) * tx;
    return top + (bottom - top) * ty;
}

QImage coverageMask(const QPainterPath &region, const QRect &area)
{
    QImage mask(area.size(), QImage::Format_Alpha8);
    mask.fill(0);
    QPainter painter(&mask);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(-area.topLeft());
    painter.fillPath(region, Qt::black);
    return mask;
}

// A thin band just inside the region's edge, where watercolour pools.
QImage edgeMask(const QPainterPath &region, const QRect &area, const QImage &coverage)
{
    QImage mask(area.size(), QImage::Format_Alpha8);
    mask.fill(0);
    QPainter painter(&mask);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(-area.topLeft());
    painter.strokePath(region, QPen(Qt::black, 4));
    painter.resetTransform();
    painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    painter.drawImage(0, 0, coverage);
    return mask;
}

int channel(float value)
{
    return std::clamp(int(std::lround(value)), 0, 255);
}

} // namespace

void paintStyledArea(QPainter &painter, const QPainterPath &region, StrokeStyle style, const QColor &color,
                     QPointF grainOrigin)
{
    if (style == StrokeStyle::None || region.isEmpty()) {
        return;
    }
    if (style == StrokeStyle::Solid) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillPath(region, color);
        painter.restore();
        return;
    }
    QRect area = region.boundingRect().toAlignedRect().adjusted(-2, -2, 2, 2);
    if (const QPaintDevice *device = painter.device()) {
        area = area.intersected(QRect(0, 0, device->width(), device->height()));
    }
    if (area.isEmpty()) {
        return;
    }
    const QImage coverage = coverageMask(region, area);
    // Outline areas overlap themselves at every corner; only their outer edge is an edge.
    const QImage edges =
        style == StrokeStyle::Watercolor ? edgeMask(region.simplified(), area, coverage) : QImage();
    const QRgb rgb = color.rgb();
    const float red = float(qRed(rgb));
    const float green = float(qGreen(rgb));
    const float blue = float(qBlue(rgb));
    const float opacity = float(color.alphaF());
    const int originX = int(std::floor(grainOrigin.x()));
    const int originY = int(std::floor(grainOrigin.y()));

    QImage result(area.size(), QImage::Format_ARGB32);
    result.fill(Qt::transparent);
    for (int y = 0; y < area.height(); ++y) {
        const uchar *cover = coverage.constScanLine(y);
        const uchar *edge = edges.isNull() ? nullptr : edges.constScanLine(y);
        auto *line = reinterpret_cast<QRgb *>(result.scanLine(y));
        // Grain coordinates relative to the shape.
        const int gy = area.top() + y - originY;
        for (int x = 0; x < area.width(); ++x) {
            if (cover[x] == 0) {
                continue;
            }
            const int gx = area.left() + x - originX;
            float alpha = cover[x] / 255.f;
            float r = red;
            float g = green;
            float b = blue;
            switch (style) {
            case StrokeStyle::Crayon: {
                // Wax catches the paper's tooth: dense colour with bare speckles.
                const float grain = 0.6f * whiteNoise(gx, gy) + 0.4f * valueNoise(gx / 3.f, gy / 3.f);
                alpha *= grain < 0.33f ? 0.12f : 0.6f + 0.4f * grain;
                break;
            }
            case StrokeStyle::NaturalPencil: {
                // Fine, light graphite grain.
                const float grain = 0.5f * whiteNoise(gx, gy) + 0.5f * valueNoise(gx / 1.5f, gy / 1.5f);
                alpha *= 0.3f + 0.6f * grain;
                break;
            }
            case StrokeStyle::Marker:
                alpha *= 0.6f;
                break;
            case StrokeStyle::Oil: {
                // Thick paint with lighter and darker strokes in it.
                const float shade =
                    0.55f * valueNoise(gx / 7.f, gy / 7.f) + 0.45f * valueNoise(gx / 2.5f, gy / 2.5f) - 0.5f;
                const float target = shade > 0 ? 255.f : 0.f;
                const float amount = std::abs(shade) * 0.55f;
                r += (target - r) * amount;
                g += (target - g) * amount;
                b += (target - b) * amount;
                alpha *= 0.9f + 0.1f * whiteNoise(gx, gy);
                break;
            }
            case StrokeStyle::Watercolor: {
                // A thin wash, darker where the paint dried at the edge.
                const float wash = 0.35f + 0.15f * valueNoise(gx / 9.f, gy / 9.f);
                alpha = alpha * wash + (edge[x] / 255.f) * 0.3f;
                break;
            }
            case StrokeStyle::None:
            case StrokeStyle::Solid:
                break;
            }
            line[x] = qRgba(channel(r), channel(g), channel(b), channel(alpha * opacity * 255.f));
        }
    }
    painter.drawImage(area.topLeft(), result);
}

void paintShape(QPainter &painter, const QPainterPath &path, bool closed, const ShapeStyle &style,
                QPointF grainOrigin)
{
    if (closed) {
        paintStyledArea(painter, path, style.fill, style.fillColor, grainOrigin);
    }
    if (style.outline == StrokeStyle::None) {
        return;
    }
    // Filling the stroke's area rather than drawing with a pen: Qt draws 1 px pens with a thin-line
    // rasteriser that leaves corners unjoined.
    QPainterPathStroker stroker(QPen(style.outlineColor, style.width, Qt::SolidLine, style.cap, style.join));
    stroker.setMiterLimit(MiterLimit);
    paintStyledArea(painter, stroker.createStroke(path), style.outline, style.outlineColor, grainOrigin);
}

QRectF shapePaintBounds(const QPainterPath &path, const ShapeStyle &style)
{
    // Miter joins can reach past half the width at sharp corners.
    const qreal reach = style.outline == StrokeStyle::None ? 1 : style.width * MiterLimit / 2 + 2;
    return path.controlPointRect().adjusted(-reach, -reach, reach, reach);
}

} // namespace paint
