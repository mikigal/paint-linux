#include "document/Document.h"
#include "document/History.h"
#include "document/UndoStack.h"

#include <QPainter>
#include <QSignalSpy>
#include <QTest>

using namespace paint;

namespace {

// A step that adds `amount` to a counter and holds `bytes` of memory.
class AddCommand : public UndoCommand
{
public:
    AddCommand(int &counter, int amount, qint64 bytes)
        : UndoCommand(QStringLiteral("Add %1").arg(amount))
        , m_counter(counter)
        , m_amount(amount)
        , m_bytes(bytes)
    {}

    void undo() override { m_counter -= m_amount; }
    void redo() override { m_counter += m_amount; }
    qint64 cost() const override { return m_bytes; }

private:
    int &m_counter;
    int m_amount;
    qint64 m_bytes;
};

} // namespace

class TestUndoStack : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void undoAndRedoWalkTheSteps()
    {
        UndoStack stack;
        int counter = 0;
        add(stack, counter, 1);
        add(stack, counter, 10);
        QCOMPARE(counter, 11);
        QCOMPARE(stack.undoText(), QStringLiteral("Add 10"));
        stack.undo();
        QCOMPARE(counter, 1);
        QCOMPARE(stack.index(), 1);
        QCOMPARE(stack.count(), 2);
        QCOMPARE(stack.redoText(), QStringLiteral("Add 10"));
        stack.redo();
        QCOMPARE(counter, 11);
        // A new step after undo drops what could have been redone.
        stack.undo();
        add(stack, counter, 100);
        QCOMPARE(stack.count(), 2);
        QVERIFY(!stack.canRedo());
        QCOMPARE(counter, 101);
    }

    void theOldestStepsGoPastTheLimits()
    {
        UndoStack stack;
        int counter = 0;
        stack.setLimits(3, 1000);
        for (int i = 1; i <= 5; ++i) {
            add(stack, counter, i);
        }
        QCOMPARE(stack.count(), 3);
        QCOMPARE(stack.index(), 3);
        // Undo stops where the oldest kept step began.
        while (stack.canUndo()) {
            stack.undo();
        }
        QCOMPARE(counter, 1 + 2);

        UndoStack budget;
        budget.setLimits(100, 250);
        for (int i = 0; i < 4; ++i) {
            add(budget, counter, 1, 100);
        }
        QCOMPARE(budget.count(), 2);
        QCOMPARE(budget.cost(), 200);
        // The newest step stays even when it alone is over budget.
        add(budget, counter, 1, 1000);
        QCOMPARE(budget.count(), 1);
    }

    void theSavedStateCanBeLost()
    {
        UndoStack stack;
        int counter = 0;
        QSignalSpy clean(&stack, &UndoStack::cleanChanged);
        QVERIFY(stack.isClean());
        add(stack, counter, 1);
        QVERIFY(!stack.isClean());
        stack.setClean();
        QVERIFY(stack.isClean());
        stack.undo();
        stack.redo();
        QVERIFY(stack.isClean());
        QCOMPARE(clean.count(), 4);

        // Undo below the saved state and push something else: the saved state is gone.
        stack.undo();
        add(stack, counter, 2);
        stack.undo();
        QVERIFY(!stack.isClean());

        // Steps leading to the saved state dropping off also lose it.
        UndoStack limited;
        limited.setLimits(2, 1000);
        add(limited, counter, 1);
        limited.setClean();
        add(limited, counter, 1);
        add(limited, counter, 1);
        QSignalSpy limitedIndex(&limited, &UndoStack::indexChanged);
        add(limited, counter, 1);
        // The index stays at 2 as the oldest step goes, but the signal still says something changed.
        QCOMPARE(limited.index(), 2);
        QCOMPARE(limitedIndex.count(), 1);
        while (limited.canUndo()) {
            limited.undo();
            QVERIFY(!limited.isClean());
        }
    }

    void historyStepsCostWhatChanged()
    {
        Document document(QSize(100, 50));
        document.addLayer(QStringLiteral("Layer 2"));
        History history(&document);
        // A pixel edit holds its area twice.
        const QImage before = document.layer(0).image.copy(QRect(0, 0, 10, 10));
        QPainter(&document.layer(0).image).fillRect(0, 0, 10, 10, Qt::red);
        history.recordPixelEdit(QStringLiteral("Paint"), 0, QRect(0, 0, 10, 10), before);
        QCOMPARE(history.stack()->cost(), 2 * 10 * 10 * 4);

        // A structural change holds the layers it replaced and their replacements, not the others.
        const Document snapshot = document;
        document.layer(1).image = document.layer(1).image.flipped(Qt::Horizontal);
        history.recordDocumentChange(QStringLiteral("Flip one"), snapshot);
        QCOMPARE(history.stack()->cost(), 2 * 10 * 10 * 4 + 2 * 100 * 50 * 4);
        QCOMPARE(History::MemoryLimit, 1024LL * 1024 * 1024);
    }

private:
    static void add(UndoStack &stack, int &counter, int amount, qint64 bytes = 1)
    {
        counter += amount; // steps are recorded after being made
        stack.push(std::make_unique<AddCommand>(counter, amount, bytes));
    }
};

QTEST_GUILESS_MAIN(TestUndoStack)
#include "tst_undostack.moc"
