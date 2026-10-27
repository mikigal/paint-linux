#include "brushes/Textures.h"

#include <algorithm>
#include <cmath>

namespace paint {

namespace {

int channel(float value)
{
    return std::clamp(int(std::lround(value)), 0, 255);
}

// For every pixel, the smallest coverage within `radius` of it (outside the image counts as
// uncovered); a mark minus its erosion is a band along its edge.
QImage eroded(const QImage &coverage, int radius)
{
    const int width = coverage.width();
    const int height = coverage.height();
    QImage across(coverage.size(), QImage::Format_Alpha8);
    for (int y = 0; y < height; ++y) {
        const uchar *source = coverage.constScanLine(y);
        uchar *target = across.scanLine(y);
        for (int x = 0; x < width; ++x) {
            uchar smallest = 255;
            for (int dx = -radius; dx <= radius; ++dx) {
                const int sx = x + dx;
                smallest = std::min<uchar>(smallest, sx < 0 || sx >= width ? 0 : source[sx]);
            }
            target[x] = smallest;
        }
    }
    QImage result(coverage.size(), QImage::Format_Alpha8);
    for (int y = 0; y < height; ++y) {
        uchar *target = result.scanLine(y);
        for (int x = 0; x < width; ++x) {
            uchar smallest = 255;
            for (int dy = -radius; dy <= radius; ++dy) {
                const int sy = y + dy;
                smallest =
                    std::min<uchar>(smallest, sy < 0 || sy >= height ? 0 : across.constScanLine(sy)[x]);
            }
            target[x] = smallest;
        }
    }
    return result;
}

} // namespace

float whiteNoise(int x, int y)
{
    quint32 h = quint32(x) * 374761393u + quint32(y) * 668265263u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    h ^= h >> 16u;
    return float(h & 0xffffu) / 65535.f;
}

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

QImage textured(const QImage &coverage, StrokeStyle style, const QColor &color, QPoint grainTopLeft)
{
    Q_ASSERT(coverage.format() == QImage::Format_Alpha8);
    const QImage inner = style == StrokeStyle::Watercolor ? eroded(coverage, WatercolorEdge) : QImage();
    const QRgb rgb = color.rgb();
    const float red = float(qRed(rgb));
    const float green = float(qGreen(rgb));
    const float blue = float(qBlue(rgb));
    const float opacity = float(color.alphaF());

    QImage result(coverage.size(), QImage::Format_ARGB32);
    result.fill(Qt::transparent);
    for (int y = 0; y < coverage.height(); ++y) {
        const uchar *cover = coverage.constScanLine(y);
        const uchar *core = inner.isNull() ? nullptr : inner.constScanLine(y);
        auto *line = reinterpret_cast<QRgb *>(result.scanLine(y));
        const int gy = grainTopLeft.y() + y;
        for (int x = 0; x < coverage.width(); ++x) {
            if (cover[x] == 0) {
                continue;
            }
            const int gx = grainTopLeft.x() + x;
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
                const float edge = core ? (cover[x] - core[x]) / 255.f : 0.f;
                alpha = alpha * wash + edge * 0.3f;
                break;
            }
            case StrokeStyle::None:
            case StrokeStyle::Solid:
                break;
            }
            line[x] = qRgba(channel(r), channel(g), channel(b), channel(alpha * opacity * 255.f));
        }
    }
    return result;
}

} // namespace paint
