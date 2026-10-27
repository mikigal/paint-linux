#pragma once

#include <QImage>
#include <QString>

namespace paint::test {

struct GoldenOptions
{
    // Largest per-channel difference that still counts as equal.
    int channelTolerance = 0;
    // Fraction of pixels (0..1) allowed to exceed the tolerance.
    double allowedMismatchRatio = 0.0;
    QString goldenDir = QStringLiteral(PAINT_GOLDEN_DIR);
    QString outputDir = QStringLiteral(PAINT_TEST_OUTPUT_DIR);
};

struct GoldenResult
{
    bool passed = false;
    QString message;
};

// Compares `actual` with `<goldenDir>/<name>.png`. On mismatch it writes
// `<outputDir>/<name>.actual.png` and `<name>.diff.png` (differing pixels in red)
// for inspection. With PAINT_UPDATE_GOLDEN=1 in the environment the golden file
// is (re)written from `actual` instead and the comparison passes.
GoldenResult compareWithGolden(const QImage &actual, const QString &name, const GoldenOptions &options = {});

// Number of pixels whose channels differ by more than `channelTolerance`.
qsizetype countDifferentPixels(const QImage &a, const QImage &b, int channelTolerance,
                               QImage *diff = nullptr);

} // namespace paint::test

#define PAINT_COMPARE_GOLDEN(image, name, ...)                                                               \
    do {                                                                                                     \
        const auto goldenResult_ =                                                                           \
            ::paint::test::compareWithGolden((image), (name)__VA_OPT__(, ) __VA_ARGS__);                     \
        QVERIFY2(goldenResult_.passed, qPrintable(goldenResult_.message));                                   \
    } while (false)
