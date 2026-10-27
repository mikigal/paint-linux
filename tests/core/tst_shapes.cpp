#include "GoldenImage.h"
#include "editor/Editor.h"
#include "shapes/ShapeGeometry.h"
#include "shapes/ShapeStyles.h"
#include "tools/ShapeTool.h"
#include "tools/ToolSettings.h"

#include <QPainter>
#include <QSignalSpy>
#include <QTest>

using namespace paint;

// Drives the shape tool through Editor, as the canvas does, plus the outline and fill styles.
class TestShapes : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        m_settings = std::make_unique<ToolSettings>();
        m_editor = std::make_unique<Editor>(m_settings.get());
        m_editor->newDocument(QSize(40, 30));
        m_settings->setTool(Tool::Shape);
        m_settings->setSize(1);
        m_settings->setShapeType(ShapeType::Rectangle);
    }

    void rectangleFillsTheDraggedPixels()
    {
        drag({5.3, 5.6}, {14.2, 12.9});
        QVERIFY(shape()->isEditing());
        QCOMPARE(shape()->extent(), QRectF(5, 5, 10, 8));
        // The outline runs along the outermost pixels of the box, crisp at 1 px.
        for (const QPoint corner :
             {QPoint(5, 5), QPoint(14, 5), QPoint(14, 12), QPoint(5, 12), QPoint(10, 5)}) {
            QCOMPARE(pixel(corner), QColor(Qt::black));
        }
        QCOMPARE(pixel({10, 9}).alpha(), 0); // no fill by default
        QCOMPARE(pixel({4, 5}).alpha(), 0);
        QCOMPARE(pixel({15, 12}).alpha(), 0);
        // Not an undo step until the shape is committed.
        QVERIFY(!m_editor->history().canUndo());
        m_editor->commitTool();
        QVERIFY(!shape()->isActive());
        QVERIFY(m_editor->history().canUndo());
        m_editor->undo();
        QCOMPARE(pixel({5, 5}).alpha(), 0);
    }

    void outlineUsesColorOneAndFillColorTwo()
    {
        m_settings->setColor(ColorSlot::Primary, Qt::blue);
        m_settings->setColor(ColorSlot::Secondary, Qt::red);
        m_settings->setFillStyle(StrokeStyle::Solid);
        drag({5, 5}, {15, 15});
        QCOMPARE(pixel({5, 5}), QColor(Qt::blue));
        QCOMPARE(pixel({10, 10}), QColor(Qt::red));
        m_editor->commitTool();

        // The right button swaps them.
        drag({20, 5}, {30, 15}, {}, Qt::RightButton);
        QCOMPARE(pixel({20, 5}), QColor(Qt::red));
        QCOMPARE(pixel({25, 10}), QColor(Qt::blue));
    }

    void shiftDrawsASquare()
    {
        drag({5, 5}, {20, 10}, Qt::ShiftModifier);
        QCOMPARE(shape()->extent(), QRectF(5, 5, 16, 16));
    }

    void clickWithoutDragDrawsNothing()
    {
        drag({10, 10}, {10.4, 10.2});
        QVERIFY(!shape()->isActive());
        QVERIFY(!m_editor->history().canUndo());
        QCOMPARE(pixel({10, 10}).alpha(), 0);
    }

    void editedShapeFollowsTheSettings()
    {
        drag({5, 5}, {25, 25});
        m_settings->setFillStyle(StrokeStyle::Solid);
        QCOMPARE(pixel({15, 15}), QColor(Qt::white));
        m_settings->setColor(ColorSlot::Primary, Qt::green);
        QCOMPARE(pixel({5, 5}), QColor(Qt::green));
        m_settings->setSize(5);
        // A wider outline still fits in the box, growing inwards.
        QCOMPARE(pixel({9, 15}), QColor(Qt::green));
        QCOMPARE(pixel({4, 15}).alpha(), 0);
        m_settings->setOutlineStyle(StrokeStyle::None);
        QCOMPARE(pixel({5, 15}), QColor(Qt::white));
        // Restyling is part of the one shape, not undo steps of its own.
        QVERIFY(!m_editor->history().canUndo());
        m_editor->commitTool();
        QCOMPARE(m_editor->history().stack()->count(), 1);
    }

    void draggingInsideMovesTheShape()
    {
        drag({5, 5}, {15, 15});
        QCOMPARE(shape()->hitTest({10, 10}, 1.0), BoxHandle::Inside);
        drag({10, 10}, {20.4, 13.2});
        QCOMPARE(shape()->extent(), QRectF(15, 8, 11, 11));
        QCOMPARE(pixel({5, 5}).alpha(), 0);
        QCOMPARE(pixel({15, 8}), QColor(Qt::black));
        QVERIFY(shape()->isEditing());
    }

    void handlesResizeAndRotate()
    {
        drag({10, 10}, {29.5, 19.5}); // 20 × 10
        // Bottom-right handle at (30, 20) out to (35, 25).
        drag({30, 20}, {35, 25});
        QCOMPARE(shape()->extent(), QRectF(10, 10, 25, 15));
        m_editor->undo(); // drops the edited shape
        QVERIFY(!shape()->isActive());
        QCOMPARE(pixel({10, 10}).alpha(), 0);

        drag({10, 10}, {29.5, 19.5});
        const QList<QPointF> handles = shape()->handlePositions(1.0);
        QCOMPARE(handles.size(), 9);
        QCOMPARE(shape()->hitTest(handles.last(), 1.0), BoxHandle::Rotate);
        // From above the centre (20, 15) to its right: a quarter turn clockwise.
        drag(handles.last(), {49, 15});
        QCOMPARE(shape()->angle(), 90.0);
        const QRectF extent = shape()->extent();
        QVERIFY(qAbs(extent.width() - 10) < 0.01 && qAbs(extent.height() - 20) < 0.01);
        QVERIFY(pixel({20, 5}).alpha() > 200);
        QCOMPARE(pixel({11, 15}).alpha(), 0);
    }

    void clickingOutsideCommitsAndStartsANewShape()
    {
        drag({2, 2}, {10, 10});
        drag({20, 15}, {30, 25});
        QCOMPARE(m_editor->history().stack()->count(), 1);
        QVERIFY(shape()->isEditing());
        QCOMPARE(pixel({2, 2}), QColor(Qt::black));
        QCOMPARE(pixel({20, 15}), QColor(Qt::black));
    }

    void keysNudgeCommitAndDelete()
    {
        drag({5, 5}, {15, 15});
        QVERIFY(m_editor->currentTool()->keyPress({Qt::Key_Right}));
        QCOMPARE(shape()->extent(), QRectF(6, 5, 11, 11));
        QVERIFY(m_editor->currentTool()->keyPress({Qt::Key_Delete}));
        QVERIFY(!shape()->isActive());
        QCOMPARE(pixel({6, 5}).alpha(), 0);
        QVERIFY(!m_editor->history().canUndo());

        drag({5, 5}, {15, 15});
        QVERIFY(m_editor->currentTool()->keyPress({Qt::Key_Return}));
        QVERIFY(!shape()->isActive());
        QVERIFY(m_editor->history().canUndo());
    }

    void switchingToolOrShapeCommits()
    {
        drag({5, 5}, {15, 15});
        m_settings->setShapeType(ShapeType::Oval);
        QVERIFY(!shape()->isActive());
        QCOMPARE(m_editor->history().stack()->count(), 1);

        drag({20, 5}, {30, 15});
        m_settings->setTool(Tool::Pencil);
        QVERIFY(!shape()->isActive());
        QCOMPARE(m_editor->history().stack()->count(), 2);
    }

    void linesSnapWithShiftAndMoveTheirEnds()
    {
        m_settings->setShapeType(ShapeType::Line);
        drag({2.2, 3.7}, {30.9, 3.1});
        for (int x = 2; x <= 30; ++x) {
            QCOMPARE(pixel({x, 3}), QColor(Qt::black));
        }
        QCOMPARE(pixel({31, 3}).alpha(), 0);
        QCOMPARE(shape()->handlePositions(1.0), (QList<QPointF>{{2.5, 3.5}, {30.5, 3.5}}));
        QCOMPARE(shape()->hitTest({30.5, 3.5}, 1.0), BoxHandle::Right);
        QCOMPARE(shape()->hitTest({16, 3.6}, 1.0), BoxHandle::Inside);
        QCOMPARE(shape()->hitTest({16, 12}, 1.0), BoxHandle::None);

        // Dragging an end moves just that end.
        drag({30.5, 3.5}, {30.5, 20.2});
        QVERIFY(pixel({30, 20}).alpha() > 200);
        QCOMPARE(pixel({8, 3}).alpha(), 0);
        m_editor->commitTool();

        // Shift keeps the line at 45°.
        drag({5, 5}, {15.3, 13.8}, Qt::ShiftModifier);
        QCOMPARE(shape()->handlePositions(1.0).last(), QPointF(15.5, 15.5));
        QCOMPARE(shape()->angle(), 45.0);
    }

    void curvesBendTwiceBeforeEditing()
    {
        m_settings->setShapeType(ShapeType::Curve);
        drag({5, 20}, {35, 20});
        QVERIFY(shape()->isActive());
        QVERIFY(!shape()->isEditing());
        // The first bend pulls the middle of the curve three quarters of the way to the pointer.
        drag({20, 20}, {20, 0});
        QVERIFY(pixel({20, 5}).alpha() > 100);
        QCOMPARE(pixel({20, 20}).alpha(), 0);
        QVERIFY(!shape()->isEditing());
        drag({28, 24}, {28, 24});
        QVERIFY(shape()->isEditing());
        QCOMPARE(shape()->handlePositions(1.0).size(), 9);
        m_editor->commitTool();
        QCOMPARE(m_editor->history().stack()->count(), 1);
    }

    void polygonsCloseOnTheirFirstCorner()
    {
        m_settings->setShapeType(ShapeType::Polygon);
        m_settings->setColor(ColorSlot::Secondary, Qt::red);
        m_settings->setFillStyle(StrokeStyle::Solid);
        drag({5, 5}, {25, 5});
        drag({25, 20}, {25, 20});
        QVERIFY(!shape()->isEditing());
        QCOMPARE(pixel({20, 10}).alpha(), 0); // not filled while open
        drag({5.2, 5.3}, {5.2, 5.3});
        QVERIFY(shape()->isEditing());
        QCOMPARE(pixel({20, 10}), QColor(Qt::red));
        QVERIFY(std::max(pixel({15, 12}).alpha(), pixel({15, 13}).alpha()) > 100); // closing side
        QCOMPARE(shape()->extent(), QRectF(5, 5, 21, 16));
    }

    void polygonsCloseOnDoubleClick()
    {
        m_settings->setShapeType(ShapeType::Polygon);
        drag({5, 5}, {25, 5});
        drag({25, 20}, {25, 20});
        PointerEvent second{{25, 20}, Qt::LeftButton, {}};
        second.doubleClick = true;
        m_editor->currentTool()->press(second);
        m_editor->currentTool()->release(second);
        QVERIFY(shape()->isEditing());
        QCOMPARE(shape()->extent(), QRectF(5, 5, 21, 16));
    }

    void undoWhileDrawingAPolygonDropsIt()
    {
        m_settings->setShapeType(ShapeType::Polygon);
        drag({5, 5}, {25, 5});
        m_editor->undo();
        QVERIFY(!shape()->isActive());
        QCOMPARE(pixel({15, 5}).alpha(), 0);
        QVERIFY(!m_editor->history().canUndo());
    }

    void texturedStylesTravelWithTheShape()
    {
        for (StrokeStyle style : {StrokeStyle::Crayon, StrokeStyle::Oil, StrokeStyle::Watercolor}) {
            QPainterPath path;
            path.addRect(QRectF(4, 4, 20, 12));
            QImage first(40, 30, QImage::Format_ARGB32_Premultiplied);
            first.fill(Qt::transparent);
            QImage second = first.copy();
            {
                QPainter painter(&first);
                paintStyledArea(painter, path, style, Qt::blue, {4, 4});
            }
            {
                QPainter painter(&second);
                paintStyledArea(painter, path.translated(7, 3), style, Qt::blue, {11, 7});
            }
            QCOMPARE(second.copy(9, 5, 24, 16), first.copy(2, 2, 24, 16));
        }
    }

    void stylesMatchGolden()
    {
        constexpr int Cell = 64;
        const QList<StrokeStyle> styles(AllStrokeStyles.begin() + 1, AllStrokeStyles.end());
        QImage sheet(int(styles.size()) * Cell, 2 * Cell, QImage::Format_ARGB32_Premultiplied);
        sheet.fill(Qt::white);
        QPainter painter(&sheet);
        for (qsizetype i = 0; i < styles.size(); ++i) {
            const QRectF cell(int(i) * Cell + 6, 6, Cell - 12, Cell - 12);
            ShapeStyle outline{.outline = styles.at(i), .outlineColor = QColor(0x3f48cc), .width = 8};
            paintShape(painter, shapePath(ShapeType::Oval, cell), true, outline, cell.topLeft());
            const QRectF lower = cell.translated(0, Cell);
            ShapeStyle fill{
                .outline = StrokeStyle::None, .fill = styles.at(i), .fillColor = QColor(0xed1c24)};
            paintShape(painter, shapePath(ShapeType::Heart, lower), true, fill, lower.topLeft());
        }
        painter.end();
        PAINT_COMPARE_GOLDEN(sheet, QStringLiteral("shapes/styles"), {.channelTolerance = 2});
    }

private:
    void drag(QPointF from, QPointF to, Qt::KeyboardModifiers modifiers = {},
              Qt::MouseButton button = Qt::LeftButton)
    {
        InteractiveTool *tool = m_editor->currentTool();
        tool->press({from, button, modifiers});
        tool->move({(from + to) / 2, Qt::NoButton, modifiers});
        tool->move({to, Qt::NoButton, modifiers});
        tool->release({to, button, modifiers});
    }

    ShapeTool *shape() const { return m_editor->shape(); }
    QColor pixel(QPoint point) const { return m_editor->document().layer(0).image.pixelColor(point); }

    std::unique_ptr<ToolSettings> m_settings;
    std::unique_ptr<Editor> m_editor;
};

QTEST_GUILESS_MAIN(TestShapes)
#include "tst_shapes.moc"
