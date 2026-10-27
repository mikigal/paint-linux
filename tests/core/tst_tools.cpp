#include "GoldenImage.h"
#include "editor/Editor.h"
#include "tools/ToolSettings.h"

#include <QSignalSpy>
#include <QTest>

#include <cmath>
#include <numbers>

using namespace paint;

// Drives the interactive tools through Editor, as the canvas does.
class TestTools : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        m_settings = std::make_unique<ToolSettings>();
        m_editor = std::make_unique<Editor>(m_settings.get());
        m_editor->newDocument(QSize(40, 30));
    }

    void pencilDrawsHardOnePixelLines()
    {
        stroke({{2.5, 2.5}, {10.7, 2.2}, {10.2, 9.9}});
        QCOMPARE(pixel(2, 2), QColor(Qt::black));
        QCOMPARE(pixel(10, 2), QColor(Qt::black));
        QCOMPARE(pixel(10, 9), QColor(Qt::black));
        QCOMPARE(pixel(11, 2).alpha(), 0);
        QCOMPARE(pixel(6, 3).alpha(), 0);
        // Hard edges: every touched pixel is fully opaque.
        const QImage &layer = m_editor->document().layer(0).image;
        for (int y = 0; y < layer.height(); ++y) {
            for (int x = 0; x < layer.width(); ++x) {
                const int alpha = layer.pixelColor(x, y).alpha();
                QVERIFY(alpha == 0 || alpha == 255);
            }
        }
    }

    void rightButtonUsesColorTwo()
    {
        m_settings->setColor(ColorSlot::Secondary, Qt::red);
        stroke({{5.5, 5.5}}, Qt::RightButton);
        QCOMPARE(pixel(5, 5), QColor(Qt::red));
    }

    void opacityDoesNotBuildUpWithinAStroke()
    {
        m_settings->setTool(Tool::Brush);
        m_settings->setSize(6);
        m_settings->setOpacity(50);
        // Back and forth over the same spot in one stroke.
        stroke({{5, 15}, {30, 15}, {5, 15}, {30, 15}});
        QVERIFY(qAbs(pixel(15, 15).alpha() - 128) <= 1);
        // A second stroke does build on the first.
        stroke({{5, 15}, {30, 15}});
        QVERIFY(pixel(15, 15).alpha() > 180);
    }

    void brushStrokeMatchesGolden()
    {
        m_settings->setTool(Tool::Brush);
        m_settings->setSize(5);
        m_settings->setColor(ColorSlot::Primary, QColor(0x3f48cc));
        stroke({{4, 25}, {12, 6}, {22, 22}, {36, 5}});
        PAINT_COMPARE_GOLDEN(m_editor->document().composite(), QStringLiteral("tools/brush-stroke"),
                             {.channelTolerance = 2});
    }

    void allBrushesMatchGolden()
    {
        // Each brush draws the same wave in its own cell.
        constexpr int CellWidth = 160;
        constexpr int CellHeight = 90;
        m_editor->newDocument(QSize(3 * CellWidth, 3 * CellHeight));
        m_settings->setTool(Tool::Brush);
        m_settings->setSize(14);
        m_settings->setColor(ColorSlot::Primary, QColor(0x3f48cc));
        for (qsizetype i = 0; i < qsizetype(AllBrushTypes.size()); ++i) {
            m_settings->setBrushType(AllBrushTypes.at(i));
            const QPointF origin(int(i % 3) * CellWidth + 20, int(i / 3) * CellHeight + 45);
            QList<QPointF> wave;
            for (int step = 0; step <= 40; ++step) {
                const qreal x = step * 3.0;
                wave.append(origin + QPointF(x, -22 * std::sin(x / 120 * 2 * std::numbers::pi)));
            }
            stroke(wave);
        }
        PAINT_COMPARE_GOLDEN(m_editor->document().composite(), QStringLiteral("tools/brushes"),
                             {.channelTolerance = 2});
    }

    void texturedBrushesShareThePaperGrain()
    {
        // Two crayon strokes over the same pixels give the same texture: the grain is the paper's.
        m_settings->setTool(Tool::Brush);
        m_settings->setBrushType(BrushType::Crayon);
        m_settings->setSize(10);
        stroke({{5, 10}, {35, 10}});
        const QImage first = m_editor->document().layer(0).image.copy(10, 6, 20, 8);
        m_editor->undo();
        stroke({{35, 10}, {5, 10}});
        QCOMPARE(m_editor->document().layer(0).image.copy(10, 6, 20, 8), first);
    }

    void brushStrokesAreReproducible()
    {
        m_settings->setTool(Tool::Brush);
        for (BrushType type : {BrushType::Airbrush, BrushType::OilBrush}) {
            m_settings->setBrushType(type);
            stroke({{5, 5}, {20, 15}, {35, 8}});
            const QImage first = m_editor->document().layer(0).image;
            m_editor->undo();
            stroke({{5, 5}, {20, 15}, {35, 8}});
            QCOMPARE(m_editor->document().layer(0).image, first);
            m_editor->undo();
        }
    }

    void eraserClearsToTransparency()
    {
        fillLayer(Qt::black);
        m_settings->setTool(Tool::Eraser);
        m_settings->setSize(4);
        stroke({{10, 10}, {20, 10}});
        QCOMPARE(pixel(15, 10).alpha(), 0);
        QCOMPARE(pixel(15, 20), QColor(Qt::black));
        // The background still shows through the erased layer.
        QCOMPARE(m_editor->document().composite().pixelColor(15, 10), QColor(Qt::white));

        m_settings->setOpacity(50);
        stroke({{10, 20}, {20, 20}});
        QVERIFY(qAbs(pixel(15, 20).alpha() - 128) <= 1);
    }

    void eraserIsSquare()
    {
        fillLayer(Qt::black);
        m_settings->setTool(Tool::Eraser);
        m_settings->setSize(6);
        stroke({{20, 15}});
        QCOMPARE(pixel(17, 12).alpha(), 0); // corner of the 6 px square
        QCOMPARE(pixel(22, 17).alpha(), 0);
        QCOMPARE(pixel(23, 15).alpha(), 255);
    }

    void fillToolFillsAndIsUndoable()
    {
        m_settings->setTool(Tool::Fill);
        m_settings->setColor(ColorSlot::Primary, Qt::green);
        click(QPointF(3, 3));
        QCOMPARE(pixel(39, 29), QColor(Qt::green));
        QVERIFY(m_editor->isModified());
        m_editor->undo();
        QCOMPARE(pixel(39, 29).alpha(), 0);
        QVERIFY(!m_editor->isModified());
        m_editor->redo();
        QCOMPARE(pixel(39, 29), QColor(Qt::green));
    }

    void colorPickerPicksAndReturnsToPreviousTool()
    {
        m_settings->setColor(ColorSlot::Primary, Qt::blue);
        stroke({{5.5, 5.5}});
        m_settings->setTool(Tool::ColorPicker);
        click(QPointF(20, 20), Qt::RightButton); // white background
        QCOMPARE(m_settings->color(ColorSlot::Secondary), QColor(Qt::white));
        QCOMPARE(m_settings->tool(), Tool::Pencil);

        m_settings->setColor(ColorSlot::Primary, Qt::red);
        m_settings->setTool(Tool::ColorPicker);
        click(QPointF(5.5, 5.5));
        QCOMPARE(m_settings->color(ColorSlot::Primary), QColor(Qt::blue));
    }

    void magnifierRequestsZoom()
    {
        m_settings->setTool(Tool::Magnifier);
        QSignalSpy spy(m_editor.get(), &Editor::zoomRequested);
        click(QPointF(7, 8));
        click(QPointF(7, 8), Qt::RightButton);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(1).toBool(), true);
        QCOMPARE(spy.at(1).at(1).toBool(), false);
    }

    void undoRedoRestoresExactPixels()
    {
        m_settings->setTool(Tool::Brush);
        m_settings->setSize(7);
        stroke({{3, 3}, {30, 20}});
        const QImage afterFirst = m_editor->document().layer(0).image.copy();
        m_settings->setColor(ColorSlot::Primary, Qt::red);
        stroke({{3, 20}, {30, 3}});
        const QImage afterSecond = m_editor->document().layer(0).image.copy();

        m_editor->undo();
        QCOMPARE(m_editor->document().layer(0).image, afterFirst);
        m_editor->undo();
        QCOMPARE(pixel(15, 10).alpha(), 0);
        m_editor->redo();
        m_editor->redo();
        QCOMPARE(m_editor->document().layer(0).image, afterSecond);
    }

    void undoDuringStrokeCancelsIt()
    {
        stroke({{2.5, 2.5}, {8.5, 2.5}});
        InteractiveTool *tool = m_editor->currentTool();
        tool->press({QPointF(2.5, 10.5), Qt::LeftButton, {}});
        tool->move({QPointF(20.5, 10.5), Qt::NoButton, {}});
        QCOMPARE(pixel(10, 10), QColor(Qt::black));
        m_editor->undo();
        QCOMPARE(pixel(10, 10).alpha(), 0);
        QCOMPARE(pixel(5, 2), QColor(Qt::black)); // the earlier stroke survives
        QVERIFY(!tool->isActive());
    }

    void switchingToolsFinishesStroke()
    {
        InteractiveTool *tool = m_editor->currentTool();
        tool->press({QPointF(2.5, 10.5), Qt::LeftButton, {}});
        tool->move({QPointF(20.5, 10.5), Qt::NoButton, {}});
        m_settings->setTool(Tool::Fill);
        QCOMPARE(pixel(10, 10), QColor(Qt::black));
        QVERIFY(m_editor->history().canUndo());
        QVERIFY(!tool->isActive());
    }

private:
    void stroke(const QList<QPointF> &points, Qt::MouseButton button = Qt::LeftButton)
    {
        InteractiveTool *tool = m_editor->currentTool();
        tool->press({points.first(), button, {}});
        for (qsizetype i = 1; i < points.size(); ++i) {
            tool->move({points.at(i), Qt::NoButton, {}});
        }
        tool->release({points.last(), button, {}});
    }

    void click(QPointF point, Qt::MouseButton button = Qt::LeftButton)
    {
        InteractiveTool *tool = m_editor->currentTool();
        tool->press({point, button, {}});
        tool->release({point, button, {}});
    }

    void fillLayer(const QColor &color) { m_editor->document().layer(0).image.fill(color); }

    QColor pixel(int x, int y) const { return m_editor->document().layer(0).image.pixelColor(x, y); }

    std::unique_ptr<ToolSettings> m_settings;
    std::unique_ptr<Editor> m_editor;
};

QTEST_GUILESS_MAIN(TestTools)
#include "tst_tools.moc"
