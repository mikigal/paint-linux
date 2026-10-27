#include "ops/FloodFill.h"

#include <QList>

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace paint {

namespace {

int channelDistance(QRgb a, QRgb b)
{
    return std::max({std::abs(qAlpha(a) - qAlpha(b)), std::abs(qRed(a) - qRed(b)),
                     std::abs(qGreen(a) - qGreen(b)), std::abs(qBlue(a) - qBlue(b))});
}

} // namespace

QRect floodFill(QImage &image, QPoint seed, const QColor &color, int tolerance)
{
    Q_ASSERT(image.format() == QImage::Format_ARGB32_Premultiplied);
    if (!image.rect().contains(seed)) {
        return {};
    }
    const int width = image.width();
    const int height = image.height();
    // Raw premultiplied values (QImage::pixel() would unpremultiply).
    const QRgb target = reinterpret_cast<const QRgb *>(image.constScanLine(seed.y()))[seed.x()];
    const QRgb fill = qPremultiply(color.rgba());
    const int threshold = std::clamp(tolerance, 0, 100) * 255 / 100;
    if (target == fill && threshold == 0) {
        return {};
    }

    // Filled pixels are marked visited, so every pixel that is still compared has its original
    // colour and the fill colour itself never extends the region.
    std::vector<quint8> visited(size_t(width) * size_t(height), 0);
    const auto row = [&image](int y) { return reinterpret_cast<QRgb *>(image.scanLine(y)); };
    const auto matches = [&](const QRgb *line, const quint8 *seen, int x) {
        return !seen[x] && channelDistance(line[x], target) <= threshold;
    };

    int left = seed.x(), right = seed.x(), top = seed.y(), bottom = seed.y();
    QList<QPoint> stack{seed};
    while (!stack.isEmpty()) {
        const QPoint point = stack.takeLast();
        const int y = point.y();
        QRgb *line = row(y);
        quint8 *seen = visited.data() + size_t(y) * width;
        if (!matches(line, seen, point.x())) {
            continue;
        }
        // Scan the whole horizontal run, then queue the runs above and below it.
        int x0 = point.x();
        while (x0 > 0 && matches(line, seen, x0 - 1)) {
            --x0;
        }
        int x1 = point.x();
        while (x1 + 1 < width && matches(line, seen, x1 + 1)) {
            ++x1;
        }
        std::fill(line + x0, line + x1 + 1, fill);
        std::fill(seen + x0, seen + x1 + 1, quint8(1));
        left = std::min(left, x0);
        right = std::max(right, x1);
        top = std::min(top, y);
        bottom = std::max(bottom, y);
        for (int ny : {y - 1, y + 1}) {
            if (ny < 0 || ny >= height) {
                continue;
            }
            const QRgb *neighbour = row(ny);
            const quint8 *neighbourSeen = visited.data() + size_t(ny) * width;
            bool inRun = false;
            for (int x = x0; x <= x1; ++x) {
                const bool match = matches(neighbour, neighbourSeen, x);
                if (match && !inRun) {
                    stack.append(QPoint(x, ny));
                }
                inRun = match;
            }
        }
    }
    return QRect(QPoint(left, top), QPoint(right, bottom));
}

} // namespace paint
