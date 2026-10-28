#include "editor/Editor.h"
#include "ops/ImageOps.h"
#include "ops/Wallpaper.h"
#include "tools/SelectionTool.h"
#include "tools/ToolSettings.h"

#include <QPainter>
#include <QTest>

using namespace paint;

class TestImageOps : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        m_settings = std::make_unique<ToolSettings>();
        m_editor = std::make_unique<Editor>(m_settings.get());
        m_editor->newDocument(QSize(40, 20));
        // Red top-left corner pixel block, blue bottom-right block.
        QPainter painter(&layer());
        painter.fillRect(QRect(0, 0, 4, 2), Qt::red);
        painter.fillRect(QRect(36, 18, 4, 2), Qt::blue);
    }

    void quarterTurnsMovePixelsExactly()
    {
        const ImageOp right = rotate(layer(), RotateBy::Right90);
        QCOMPARE(right.image.size(), QSize(20, 40));
        QCOMPARE(right.image.pixelColor(19, 0), QColor(Qt::red)); // top-left went to top-right
        QCOMPARE(right.mapping.map(QPointF(0, 0)), QPointF(20, 0));

        const ImageOp left = rotate(layer(), RotateBy::Left90);
        QCOMPARE(left.image.pixelColor(0, 39), QColor(Qt::red)); // top-left went to bottom-left

        const ImageOp half = rotate(layer(), RotateBy::Half);
        QCOMPARE(half.image.pixelColor(39, 19), QColor(Qt::red));
        QCOMPARE(half.image.pixelColor(0, 0), QColor(Qt::blue));
    }

    void flipsMirror()
    {
        QCOMPARE(flip(layer(), Qt::Horizontal).image.pixelColor(39, 0), QColor(Qt::red));
        QCOMPARE(flip(layer(), Qt::Vertical).image.pixelColor(0, 19), QColor(Qt::red));
    }

    void arbitraryRotationGrowsTheCanvas()
    {
        const ImageOp turned = rotate(layer(), 45);
        QCOMPARE(turned.image.size(), rotatedSize(QSize(40, 20), 45));
        QVERIFY(turned.image.width() > 40 && turned.image.height() > 20);
        QCOMPARE(turned.image.pixelColor(0, 0).alpha(), 0);
    }

    void skewGrowsAndShears()
    {
        const ImageOp skewed = skew(layer(), 45, 0);
        QCOMPARE(skewed.image.size(), QSize(60, 20));
        QCOMPARE(skewedSize(QSize(40, 20), 45, 0), QSize(60, 20));
        // The bottom row shifts right by its height.
        QCOMPARE(skewed.image.pixelColor(58, 19), QColor(Qt::blue));
    }

    void invertKeepsTransparency()
    {
        QImage image = layer();
        invertColors(image);
        QCOMPARE(image.pixelColor(1, 1), QColor(Qt::cyan));
        QCOMPARE(image.pixelColor(20, 10).alpha(), 0);
    }

    void blackAndWhiteThresholds()
    {
        QImage image(2, 1, Document::LayerFormat);
        image.setPixelColor(0, 0, QColor(200, 200, 40));
        image.setPixelColor(1, 0, QColor(40, 40, 200));
        const QImage result = toBlackAndWhite(image);
        QCOMPARE(result.pixelColor(0, 0), QColor(Qt::white));
        QCOMPARE(result.pixelColor(1, 0), QColor(Qt::black));
    }

    void wholeImageRotationIsOneUndoStep()
    {
        m_editor->rotate(RotateBy::Right90);
        QCOMPARE(m_editor->document().size(), QSize(20, 40));
        QCOMPARE(layer().pixelColor(19, 0), QColor(Qt::red));
        m_editor->undo();
        QCOMPARE(m_editor->document().size(), QSize(40, 20));
        QCOMPARE(layer().pixelColor(0, 0), QColor(Qt::red));
    }

    void selectionCommandsOnlyTouchTheSelection()
    {
        select(QRect(0, 0, 10, 4));
        m_editor->flip(Qt::Horizontal);
        QVERIFY(m_editor->selection()->isFloating());
        QCOMPARE(layer().pixelColor(9, 0), QColor(Qt::red));
        QCOMPARE(layer().pixelColor(0, 0).alpha(), 0);
        QCOMPARE(layer().pixelColor(38, 19), QColor(Qt::blue)); // outside: untouched
        QCOMPARE(m_editor->document().size(), QSize(40, 20));

        m_editor->rotate(RotateBy::Right90);
        QCOMPARE(m_editor->selection()->bounds().size(), QSize(4, 10));

        m_editor->invertColors();
        m_editor->selection()->commit();
        QVERIFY(m_editor->history().canUndo());
    }

    void resizeAndSkewOfTheWholeImage()
    {
        m_editor->resizeAndSkew(QSize(80, 40), 0, 0);
        QCOMPARE(m_editor->document().size(), QSize(80, 40));
        QCOMPARE(layer().pixelColor(2, 1), QColor(Qt::red));
        m_editor->resizeAndSkew(QSize(80, 40), 0, 45);
        QCOMPARE(m_editor->document().size(), QSize(80, 120));
    }

    void canvasCanGrowToTheLeft()
    {
        m_editor->resizeCanvas(QRect(-10, -5, 50, 25));
        QCOMPARE(m_editor->document().size(), QSize(50, 25));
        QCOMPARE(layer().pixelColor(10, 5), QColor(Qt::red));
        QCOMPARE(layer().pixelColor(0, 0).alpha(), 0);
        m_editor->undo();
        QCOMPARE(m_editor->document().size(), QSize(40, 20));
    }

    void invertWithoutSelectionUsesTheLayer()
    {
        m_editor->invertColors();
        QCOMPARE(layer().pixelColor(0, 0), QColor(Qt::cyan));
        m_editor->undo();
        QCOMPARE(layer().pixelColor(0, 0), QColor(Qt::red));
    }

    void blackAndWhiteAndResolution()
    {
        m_editor->convertToBlackAndWhite();
        QCOMPARE(layer().pixelColor(0, 0), QColor(Qt::black)); // red is dark
        QCOMPARE(m_editor->document().dotsPerMeter(), Document::DefaultDotsPerMeter);
        m_editor->setResolution(11811); // 300 DPI
        QCOMPARE(m_editor->document().composite().dotsPerMeterX(), 11811);
        m_editor->undo();
        QCOMPARE(m_editor->document().dotsPerMeter(), Document::DefaultDotsPerMeter);
    }

    void wallpaperModesLayOutThePicture()
    {
        // A picture twice as wide as tall: red on the left half, blue on the right.
        QImage picture(100, 50, QImage::Format_ARGB32_Premultiplied);
        picture.fill(Qt::blue);
        QPainter(&picture).fillRect(0, 0, 50, 50, Qt::red);

        // Fill covers a square screen: the height fits, the sides are cropped.
        const QImage fill = wallpaperImage(picture, QSize(200, 200), WallpaperMode::Fill);
        QCOMPARE(fill.size(), QSize(200, 200));
        QCOMPARE(fill.pixelColor(10, 100), QColor(Qt::red));
        QCOMPARE(fill.pixelColor(190, 100), QColor(Qt::blue));
        QCOMPARE(fill.pixelColor(60, 1), QColor(Qt::red)); // no black bars at the top

        // Tile repeats it from the top-left corner.
        const QImage tile = wallpaperImage(picture, QSize(250, 120), WallpaperMode::Tile);
        QCOMPARE(tile.pixelColor(110, 60), QColor(Qt::red));
        QCOMPARE(tile.pixelColor(160, 110), QColor(Qt::blue));

        // Center keeps its size, on black.
        const QImage center = wallpaperImage(picture, QSize(300, 150), WallpaperMode::Center);
        QCOMPARE(center.pixelColor(150 - 25, 75), QColor(Qt::red));
        QCOMPARE(center.pixelColor(150 + 25, 75), QColor(Qt::blue));
        QCOMPARE(center.pixelColor(10, 10), QColor(Qt::black));
        QCOMPARE(center.pixelColor(150, 140), QColor(Qt::black));
    }

private:
    QImage &layer() { return m_editor->document().layer(0).image; }

    void select(const QRect &area)
    {
        m_settings->setTool(Tool::RectangleSelect);
        InteractiveTool *tool = m_editor->currentTool();
        const QPointF end = QPointF(area.topLeft()) + QPointF(area.width(), area.height());
        tool->press({QPointF(area.topLeft()), Qt::LeftButton, {}, 1});
        tool->move({end, Qt::NoButton, {}, 1});
        tool->release({end, Qt::LeftButton, {}, 1});
    }

    std::unique_ptr<ToolSettings> m_settings;
    std::unique_ptr<Editor> m_editor;
};

QTEST_GUILESS_MAIN(TestImageOps)
#include "tst_imageops.moc"
