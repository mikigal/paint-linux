#include "GoldenImage.h"
#include "editor/Editor.h"
#include "tools/TextTool.h"
#include "tools/ToolSettings.h"

#include <QTest>

using namespace paint;

// Drives the text tool through Editor, as the canvas does.
class TestText : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        m_settings = std::make_unique<ToolSettings>();
        m_editor = std::make_unique<Editor>(m_settings.get());
        m_editor->newDocument(QSize(200, 120));
        m_settings->setTool(Tool::Text);
        m_settings->setTextFormat(TextFormat{.family = QStringLiteral("Noto Sans"), .pointSize = 11});
    }

    void clickOpensABoxAndTypingDrawsText()
    {
        click({10.4, 10.6});
        QVERIFY(text()->isEditing());
        const QRectF box = text()->extent();
        QCOMPARE(box.topLeft(), QPointF(10, 10));
        QVERIFY(box.width() > 100 && box.height() > 15 && box.height() < 30);
        QCOMPARE(inkIn(box.toRect()), 0);

        type(QStringLiteral("Hello"));
        QCOMPARE(text()->text(), QStringLiteral("Hello"));
        QVERIFY(inkIn(box.toRect()) > 20);
        // The settings now name the real font, for the format bar.
        QCOMPARE(m_settings->textFormat().family, QStringLiteral("Noto Sans"));
        QVERIFY(!m_editor->history().canUndo());

        click({150, 100}); // outside the box
        QVERIFY(!text()->isActive());
        QVERIFY(m_editor->history().canUndo());
        m_editor->undo();
        QCOMPARE(inkIn(box.toRect()), 0);
    }

    void textWrapsAndTheBoxGrows()
    {
        drag({5, 5}, {80, 35});
        QCOMPARE(text()->extent(), QRectF(5, 5, 75, 30));
        type(QStringLiteral("one two three four five six seven"));
        const QRectF box = text()->extent();
        QCOMPARE(box.width(), 75.0);
        QVERIFY(box.height() > 40);
        // Deleting the text shrinks it back to the size it was dragged to.
        key(Qt::Key_A, Qt::ControlModifier);
        key(Qt::Key_Backspace);
        QCOMPARE(text()->extent(), QRectF(5, 5, 75, 30));
        // A box is never shorter than a line.
        text()->commit();
        drag({5, 5}, {80, 15});
        QVERIFY(text()->extent().height() > 20);
        key(Qt::Key_Return);
        key(Qt::Key_Return);
        QVERIFY(text()->extent().height() > 40);
    }

    void formatAppliesToTheSelection()
    {
        click({10, 10});
        type(QStringLiteral("ab"));
        key(Qt::Key_Left, Qt::ShiftModifier); // selects "b"
        setBold(true);
        key(Qt::Key_Home);
        key(Qt::Key_Right); // after "a"
        QVERIFY(!m_settings->textFormat().bold);
        key(Qt::Key_End); // after "b"
        QVERIFY(m_settings->textFormat().bold);
        QCOMPARE(text()->text(), QStringLiteral("ab"));
    }

    void formatWithoutSelectionAppliesToWhatIsTypedNext()
    {
        click({10, 10});
        type(QStringLiteral("a"));
        setBold(true);
        type(QStringLiteral("b"));
        QVERIFY(m_settings->textFormat().bold);
        key(Qt::Key_Left);
        QVERIFY(!m_settings->textFormat().bold);
        // A bigger size makes the box taller.
        const qreal height = text()->extent().height();
        key(Qt::Key_End);
        TextFormat big = m_settings->textFormat();
        big.pointSize = 30;
        m_settings->setTextFormat(big);
        type(QStringLiteral("c"));
        QVERIFY(text()->extent().height() > height + 10);
    }

    void colourAndBackgroundFollowTheSettings()
    {
        click({20, 20});
        type(QStringLiteral("W"));
        const QPoint corner = text()->extent().topLeft().toPoint() + QPoint(1, 1);
        QCOMPARE(pixel(corner).alpha(), 0);
        m_settings->setColor(ColorSlot::Secondary, Qt::red);
        m_settings->setTextBackground(true);
        QCOMPARE(pixel(corner), QColor(Qt::red));
        m_settings->setColor(ColorSlot::Secondary, Qt::yellow);
        QCOMPARE(pixel(corner), QColor(Qt::yellow));
        m_settings->setTextBackground(false);
        QCOMPARE(pixel(corner).alpha(), 0);
    }

    void undoUndoesTypingBeforeTheBox()
    {
        click({10, 10});
        type(QStringLiteral("ab"));
        m_editor->undo();
        QVERIFY(text()->isEditing());
        QVERIFY(text()->text().isEmpty());
        m_editor->redo();
        QCOMPARE(text()->text(), QStringLiteral("ab"));
        m_editor->undo();
        m_editor->undo(); // nothing typed left: the box goes
        QVERIFY(!text()->isActive());
        QVERIFY(!m_editor->history().canUndo());
    }

    void typingKeysGoToTheBoxBeforeShortcuts()
    {
        const KeyEvent letter{Qt::Key_P, {}, QStringLiteral("p")};
        QVERIFY(!text()->wantsKey(letter));
        click({10, 10});
        QVERIFY(text()->wantsKey(letter));
        QVERIFY(text()->wantsKey({Qt::Key_C, Qt::ControlModifier}));
        QVERIFY(text()->wantsKey({Qt::Key_Escape}));
        // Formatting shortcuts stay with the format bar's actions.
        QVERIFY(!text()->wantsKey({Qt::Key_B, Qt::ControlModifier, QStringLiteral("\x02")}));
        // AltGr (Ctrl+Alt) types characters.
        QVERIFY(text()->wantsKey({Qt::Key_A, Qt::ControlModifier | Qt::AltModifier, QStringLiteral("ą")}));
        key(Qt::Key_Escape);
        QVERIFY(!text()->isActive());
    }

    void borderMovesHandlesResizeAndRotate()
    {
        drag({20, 20}, {100, 45});
        QCOMPARE(text()->hitTest({40, 20}, 1.0), BoxHandle::Inside); // top border
        QCOMPARE(text()->hitTest({20, 32.5}, 1.0), BoxHandle::Left);
        QCOMPARE(text()->hitTest({60, 32}, 1.0), BoxHandle::None);
        QVERIFY(text()->isOverText({60, 32}, 1.0));
        drag({40, 20}, {50, 30});
        QCOMPARE(text()->extent(), QRectF(30, 30, 80, 25));

        // Right handle at (110, 42.5) out to (130, 42.5).
        drag({110, 42.5}, {130, 42.5});
        QCOMPARE(text()->extent(), QRectF(30, 30, 100, 25));

        const QList<QPointF> handles = text()->handlePositions(1.0);
        drag(handles.last(), {180, 42.5}); // above the centre to its right: a quarter turn
        QCOMPARE(text()->angle(), 90.0);
        QCOMPARE(text()->extent().size(), QSizeF(25, 100));
        QVERIFY(text()->isEditing());
    }

    void caretFollowsTyping()
    {
        click({10, 10});
        const QLineF start = *text()->caret();
        type(QStringLiteral("abc"));
        const QLineF after = *text()->caret();
        QVERIFY(after.x1() > start.x1() + 10);
        QCOMPARE(after.y1(), start.y1());
        key(Qt::Key_Return);
        QVERIFY(text()->caret()->y1() > start.y1() + 10);
    }

    void textMatchesGolden()
    {
        m_settings->setColor(ColorSlot::Primary, QColor(0x3f48cc));
        m_settings->setColor(ColorSlot::Secondary, QColor(0xfff200));
        m_settings->setTextBackground(true);
        drag({10, 10}, {190, 40});
        type(QStringLiteral("Paint "));
        setBold(true);
        type(QStringLiteral("for"));
        setBold(false);
        TextFormat italic = m_settings->textFormat();
        italic.italic = true;
        italic.pointSize = 16;
        m_settings->setTextFormat(italic);
        type(QStringLiteral(" Linux"));
        key(Qt::Key_Return);
        m_settings->setTextAlignment(Qt::AlignRight);
        type(QStringLiteral("right"));
        m_editor->commitTool();
        PAINT_COMPARE_GOLDEN(m_editor->document().composite(), QStringLiteral("tools/text"),
                             {.channelTolerance = 16, .allowedMismatchRatio = 0.002});
    }

private:
    void click(QPointF point) { drag(point, point); }

    void drag(QPointF from, QPointF to)
    {
        InteractiveTool *tool = m_editor->currentTool();
        tool->press({from, Qt::LeftButton, {}});
        tool->move({to, Qt::NoButton, {}});
        tool->release({to, Qt::LeftButton, {}});
    }

    void key(int key, Qt::KeyboardModifiers modifiers = {}) { QVERIFY(text()->keyPress({key, modifiers})); }

    void type(const QString &string)
    {
        for (const QChar c : string) {
            QVERIFY(text()->keyPress({Qt::Key_unknown, {}, QString(c)}));
        }
    }

    void setBold(bool bold)
    {
        TextFormat format = m_settings->textFormat();
        format.bold = bold;
        m_settings->setTextFormat(format);
    }

    int inkIn(const QRect &area) const
    {
        const QImage &layer = m_editor->document().layer(0).image;
        int count = 0;
        for (int y = area.top(); y <= area.bottom(); ++y) {
            for (int x = area.left(); x <= area.right(); ++x) {
                count += qAlpha(layer.pixel(x, y)) > 0;
            }
        }
        return count;
    }

    QColor pixel(QPoint point) const { return m_editor->document().layer(0).image.pixelColor(point); }
    TextTool *text() const { return m_editor->text(); }

    std::unique_ptr<ToolSettings> m_settings;
    std::unique_ptr<Editor> m_editor;
};

QTEST_MAIN(TestText)
#include "tst_text.moc"
