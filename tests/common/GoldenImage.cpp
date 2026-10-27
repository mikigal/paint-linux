#include "GoldenImage.h"

#include <QDir>
#include <QFileInfo>
#include <QtGlobal>

#include <cstdlib>

namespace paint::test {

namespace {

QImage normalized(const QImage &image)
{
    return image.convertToFormat(QImage::Format_ARGB32);
}

} // namespace

qsizetype countDifferentPixels(const QImage &a, const QImage &b, int channelTolerance, QImage *diff)
{
    Q_ASSERT(a.size() == b.size());
    const QImage left = normalized(a);
    const QImage right = normalized(b);
    if (diff) {
        *diff = QImage(left.size(), QImage::Format_ARGB32);
    }

    qsizetype different = 0;
    for (int y = 0; y < left.height(); ++y) {
        const auto *l = reinterpret_cast<const QRgb *>(left.constScanLine(y));
        const auto *r = reinterpret_cast<const QRgb *>(right.constScanLine(y));
        auto *d = diff ? reinterpret_cast<QRgb *>(diff->scanLine(y)) : nullptr;
        for (int x = 0; x < left.width(); ++x) {
            const int delta =
                std::max({std::abs(qRed(l[x]) - qRed(r[x])), std::abs(qGreen(l[x]) - qGreen(r[x])),
                          std::abs(qBlue(l[x]) - qBlue(r[x])), std::abs(qAlpha(l[x]) - qAlpha(r[x]))});
            const bool mismatch = delta > channelTolerance;
            different += mismatch ? 1 : 0;
            if (d) {
                // Matching pixels are shown as a faded greyscale copy so the mismatches stand out.
                const int grey = 200 + qGray(r[x]) / 5;
                d[x] = mismatch ? qRgb(255, 0, 0) : qRgb(grey, grey, grey);
            }
        }
    }
    return different;
}

GoldenResult compareWithGolden(const QImage &actual, const QString &name, const GoldenOptions &options)
{
    const QString goldenPath = QDir(options.goldenDir).filePath(name + QStringLiteral(".png"));
    const QString actualPath = QDir(options.outputDir).filePath(name + QStringLiteral(".actual.png"));
    const QString diffPath = QDir(options.outputDir).filePath(name + QStringLiteral(".diff.png"));
    QDir().mkpath(QFileInfo(actualPath).absolutePath());

    if (qEnvironmentVariableIntValue("PAINT_UPDATE_GOLDEN") == 1) {
        QDir().mkpath(QFileInfo(goldenPath).absolutePath());
        if (!normalized(actual).save(goldenPath, "PNG")) {
            return {false, QStringLiteral("cannot write golden image %1").arg(goldenPath)};
        }
        qWarning("Updated golden image %s", qPrintable(goldenPath));
        return {true, {}};
    }

    const QImage golden(goldenPath);
    if (golden.isNull()) {
        actual.save(actualPath, "PNG");
        return {false, QStringLiteral("missing golden image %1 (actual saved to %2; rerun with "
                                      "PAINT_UPDATE_GOLDEN=1 to accept it)")
                           .arg(goldenPath, actualPath)};
    }
    if (golden.size() != actual.size()) {
        actual.save(actualPath, "PNG");
        return {false, QStringLiteral("size mismatch for %1: golden %2x%3, actual %4x%5 (actual saved to %6)")
                           .arg(name)
                           .arg(golden.width())
                           .arg(golden.height())
                           .arg(actual.width())
                           .arg(actual.height())
                           .arg(actualPath)};
    }

    QImage diff;
    const qsizetype different = countDifferentPixels(actual, golden, options.channelTolerance, &diff);
    const qsizetype total = qsizetype(actual.width()) * actual.height();
    if (double(different) <= options.allowedMismatchRatio * double(total)) {
        return {true, {}};
    }
    actual.save(actualPath, "PNG");
    diff.save(diffPath, "PNG");
    return {false, QStringLiteral("%1: %2 of %3 pixels differ (see %4 and %5)")
                       .arg(name)
                       .arg(different)
                       .arg(total)
                       .arg(actualPath, diffPath)};
}

} // namespace paint::test
