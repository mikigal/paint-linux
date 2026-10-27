#include "editor/Editor.h"
#include "tools/SelectionTool.h"
#include "tools/ToolSettings.h"

#include <QPainter>
#include <QSignalSpy>
#include <QTest>

using namespace paint;

class TestSelection : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        m_settings = std::make_unique<ToolSettings>();
        m_editor = std::make_unique<Editor>(m_settings.get());
        m_editor->newDocument(QSize(40, 30));
        // A red square at (5,5)-(14,14) on the transparent layer.
        QPainter painter(&layer());
        painter.fillRect(QRect(5, 5, 10, 10), Qt::red);
        painter.end();
        m_editor->history().setClean();
        m_settings->setTool(Tool::RectangleSelect);
    }

    void draggingDefinesAPixelAlignedRectangle()
    {
        QSignalSpy changed(m_editor.get(), &Editor::selectionStateChanged);
        drag({4.6, 4.2}, {15.3, 15.1});
        QVERIFY(selection()->hasSelection());
        QCOMPARE(selection()->bounds(), QRect(4, 4, 12, 12));
        QVERIFY(changed.count() > 0);
        QVERIFY(!m_editor->isModified());

        // A plain click outside deselects.
        drag({30, 25}, {30, 25});
        QVERIFY(!selection()->hasSelection());
    }

    void movingLiftsPixelsAndLeavesTransparency()
    {
        drag({5, 5}, {15, 15});
        drag({10, 10}, {30, 18}); // move by (20, 8)
        QVERIFY(selection()->isFloating());
        QCOMPARE(pixel(25, 13), QColor(Qt::red));
        QCOMPARE(pixel(8, 8).alpha(), 0);

        selection()->commit();
        QVERIFY(!selection()->hasSelection());
        QVERIFY(m_editor->isModified());
        m_editor->undo();
        QCOMPARE(pixel(8, 8), QColor(Qt::red));
        QCOMPARE(pixel(25, 13).alpha(), 0);
    }

    void ctrlDragDuplicates()
    {
        drag({5, 5}, {15, 15});
        drag({10, 10}, {30, 10}, Qt::ControlModifier);
        QCOMPARE(pixel(8, 8), QColor(Qt::red));
        QCOMPARE(pixel(28, 8), QColor(Qt::red));
    }

    void undoWhileFloatingPutsPixelsBack()
    {
        drag({5, 5}, {15, 15});
        drag({10, 10}, {30, 20});
        m_editor->undo();
        QVERIFY(!selection()->hasSelection());
        QCOMPARE(pixel(8, 8), QColor(Qt::red));
        QCOMPARE(pixel(28, 18).alpha(), 0);
        QVERIFY(!m_editor->history().canUndo());
    }

    void handlesResizeTheContent()
    {
        drag({5, 5}, {15, 15});
        // Drag the bottom-right handle from (15,15) to (25,25): the square doubles.
        drag({15, 15}, {25, 25});
        QCOMPARE(selection()->bounds(), QRect(5, 5, 20, 20));
        QCOMPARE(pixel(22, 22), QColor(Qt::red));
    }

    void rotateHandleTurnsTheContent()
    {
        drag({5, 5}, {25, 10}); // a 20×5 strip including part of the square
        const QList<QPointF> handles = selection()->handlePositions(1.0);
        const QPointF rotate = handles.last();
        // Drag the rotate handle to the right of the centre: a quarter turn, snapped with Shift.
        const QPointF centre(15, 7.5);
        drag(rotate, centre + QPointF(30, 0.5), Qt::ShiftModifier);
        QCOMPARE(selection()->angle(), 90.0);
        const QRect box = selection()->bounds();
        QVERIFY2(box.height() > box.width(), "a wide strip turned by 90° is tall");
    }

    void transparentSelectionDropsColorTwo()
    {
        layer().fill(Qt::white);
        QPainter painter(&layer());
        painter.fillRect(QRect(5, 5, 4, 4), Qt::blue);
        painter.fillRect(QRect(20, 0, 15, 15), Qt::green);
        painter.end();
        m_settings->setTransparentSelection(true);
        drag({3, 3}, {11, 11});
        drag({7, 7}, {27, 7});
        // The white (colour 2) around the blue square is not carried along: green shows through.
        QCOMPARE(pixel(24, 4), QColor(Qt::green));
        QCOMPARE(pixel(26, 6), QColor(Qt::blue));
    }

    void arrowKeysNudge()
    {
        drag({5, 5}, {15, 15});
        QVERIFY(selection()->keyPress({Qt::Key_Right}));
        QVERIFY(selection()->keyPress({Qt::Key_Down}));
        QCOMPARE(selection()->bounds(), QRect(6, 6, 10, 10));
        QCOMPARE(pixel(15, 15), QColor(Qt::red));
        QVERIFY(selection()->keyPress({Qt::Key_Escape}));
        QVERIFY(!selection()->hasSelection());
    }

    void freeformSelectionCopiesOnlyItsInside()
    {
        layer().fill(Qt::green);
        m_settings->setTool(Tool::FreeformSelect);
        InteractiveTool *tool = m_editor->currentTool();
        tool->press({QPointF(10, 2), Qt::LeftButton, {}, 1});
        for (QPointF point : {QPointF(18, 10), QPointF(10, 18), QPointF(2, 10)}) {
            tool->move({point, Qt::NoButton, {}, 1});
        }
        tool->release({QPointF(10, 2), Qt::LeftButton, {}, 1});
        const QImage copied = m_editor->copySelection(false);
        QCOMPARE(copied.size(), QSize(16, 16));
        QCOMPARE(copied.pixelColor(8, 8), QColor(Qt::green)); // the centre of the diamond
        QCOMPARE(copied.pixelColor(0, 0).alpha(), 0);         // a corner outside it
    }

    void deleteClearsAndSelectAllCoversTheCanvas()
    {
        drag({5, 5}, {10, 10});
        m_editor->deleteSelection();
        QCOMPARE(pixel(7, 7).alpha(), 0);
        QCOMPARE(pixel(12, 12), QColor(Qt::red));
        QVERIFY(!selection()->hasSelection());

        m_settings->setTool(Tool::Pencil);
        m_editor->selectAll();
        QVERIFY(isSelectionTool(m_settings->tool()));
        QCOMPARE(selection()->bounds(), QRect(0, 0, 40, 30));
        QCOMPARE(m_editor->copySelection(false).size(), QSize(40, 30));
    }

    void invertSelectsTheRest()
    {
        drag({0, 0}, {20, 30});
        m_editor->invertSelection();
        QCOMPARE(selection()->bounds(), QRect(20, 0, 20, 30));
    }

    void pasteFloatsAtTheGivenPlace()
    {
        QImage image(4, 3, QImage::Format_ARGB32);
        image.fill(Qt::magenta);
        m_settings->setTool(Tool::Pencil);
        m_editor->paste(image, QPointF(30, 20));
        QCOMPARE(m_settings->tool(), Tool::RectangleSelect);
        QVERIFY(selection()->isFloating());
        QCOMPARE(pixel(31, 21), QColor(Qt::magenta));
        // Commit, then one undo removes the paste.
        selection()->commit();
        m_editor->undo();
        QCOMPARE(pixel(31, 21).alpha(), 0);
    }

    void pasteCanEnlargeTheCanvas()
    {
        QImage big(60, 10, QImage::Format_ARGB32);
        big.fill(Qt::cyan);
        QSignalSpy restructured(m_editor.get(), &Editor::documentRestructured);
        m_editor->paste(big, QPointF(0, 0), true);
        QCOMPARE(m_editor->document().size(), QSize(60, 30));
        QCOMPARE(restructured.count(), 1);
        QCOMPARE(pixel(55, 5), QColor(Qt::cyan));
    }

    void cropKeepsTheSelectionAndIsUndoable()
    {
        drag({5, 5}, {15, 15});
        m_editor->cropToSelection();
        QCOMPARE(m_editor->document().size(), QSize(10, 10));
        QCOMPARE(pixel(0, 0), QColor(Qt::red));
        QVERIFY(!selection()->hasSelection());
        m_editor->undo();
        QCOMPARE(m_editor->document().size(), QSize(40, 30));
        m_editor->redo();
        QCOMPARE(m_editor->document().size(), QSize(10, 10));
    }

    void switchingToolsPlacesTheSelection()
    {
        drag({5, 5}, {15, 15});
        drag({10, 10}, {30, 10});
        m_settings->setTool(Tool::Pencil);
        QVERIFY(!selection()->hasSelection());
        QCOMPARE(pixel(28, 8), QColor(Qt::red));
        QVERIFY(m_editor->history().canUndo());
    }

private:
    SelectionTool *selection() const { return m_editor->selection(); }
    QImage &layer() { return m_editor->document().layer(0).image; }
    QColor pixel(int x, int y) { return layer().pixelColor(x, y); }

    void drag(QPointF from, QPointF to, Qt::KeyboardModifiers modifiers = {})
    {
        InteractiveTool *tool = m_editor->currentTool();
        tool->press({from, Qt::LeftButton, modifiers, 1});
        tool->move({(from + to) / 2, Qt::NoButton, modifiers, 1});
        tool->move({to, Qt::NoButton, modifiers, 1});
        tool->release({to, Qt::LeftButton, modifiers, 1});
    }

    std::unique_ptr<ToolSettings> m_settings;
    std::unique_ptr<Editor> m_editor;
};

QTEST_GUILESS_MAIN(TestSelection)
#include "tst_selection.moc"
