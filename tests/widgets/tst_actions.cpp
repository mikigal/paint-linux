#include "ActionRegistry.h"
#include "MainWindow.h"
#include "canvas/CanvasView.h"
#include "tools/ToolSettings.h"
#include "widgets/ZoomControls.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QSet>
#include <QStatusBar>
#include <QTest>
#include <QTimer>

using namespace paint;

class TestActions : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        m_window = std::make_unique<MainWindow>();
        m_window->resize(1280, 800);
        m_window->show();
        QVERIFY(QTest::qWaitForWindowActive(m_window.get()));
    }

    void cleanup() { m_window.reset(); }

    // Two actions with the same shortcut make Qt ignore both ("ambiguous shortcut").
    void shortcutsAreUnique()
    {
        QHash<QKeySequence, QString> owners;
        for (QAction *action : m_window->actionRegistry()->allActions()) {
            QVERIFY2(action->objectName().startsWith(QLatin1String("action.")),
                     qPrintable(action->objectName()));
            for (const QKeySequence &shortcut : action->shortcuts()) {
                QVERIFY2(
                    !owners.contains(shortcut),
                    qPrintable(QStringLiteral("%1 used by %2 and %3")
                                   .arg(shortcut.toString(), owners.value(shortcut), action->objectName())));
                owners.insert(shortcut, action->objectName());
            }
        }
    }

    void objectNamesAreUnique()
    {
        QSet<QString> names;
        for (QAction *action : m_window->actionRegistry()->allActions()) {
            QVERIFY2(!names.contains(action->objectName()), qPrintable(action->objectName()));
            names.insert(action->objectName());
        }
    }

    // UI scenarios address menus by object name, so names must not repeat.
    void menuNamesAreUnique()
    {
        QSet<QString> names;
        for (QMenu *menu : m_window->findChildren<QMenu *>()) {
            if (menu->objectName().isEmpty()) {
                continue;
            }
            QVERIFY2(!names.contains(menu->objectName()), qPrintable(menu->objectName()));
            names.insert(menu->objectName());
        }
        QVERIFY(names.contains(QStringLiteral("menu.image")));
        QVERIFY(names.contains(QStringLiteral("menu.ribbon.image")));
    }

    void paintShortcutsWork()
    {
        QTest::keyClick(m_window.get(), Qt::Key_B);
        QCOMPARE(m_window->toolSettings()->tool(), Tool::Fill);
        QTest::keyClick(m_window.get(), Qt::Key_E);
        QCOMPARE(m_window->toolSettings()->tool(), Tool::Eraser);
        QTest::keyClick(m_window.get(), Qt::Key_S);
        QCOMPARE(m_window->toolSettings()->tool(), Tool::RectangleSelect);
        QTest::keyClick(m_window.get(), Qt::Key_S);
        QCOMPARE(m_window->toolSettings()->tool(), Tool::FreeformSelect);

        QTest::keyClick(m_window.get(), Qt::Key_P);
        QTest::keyClick(m_window.get(), Qt::Key_Plus, Qt::ControlModifier);
        QTest::keyClick(m_window.get(), Qt::Key_Plus, Qt::ControlModifier);
        QCOMPARE(m_window->toolSettings()->size(), 3);
        QTest::keyClick(m_window.get(), Qt::Key_Minus, Qt::ControlModifier);
        QCOMPARE(m_window->toolSettings()->size(), 2);
    }

    void initialEnabledStateMatchesFreshPaint()
    {
        ActionRegistry *actions = m_window->actionRegistry();
        for (ActionId id :
             {ActionId::Undo, ActionId::Redo, ActionId::Cut, ActionId::Crop, ActionId::DeleteSelection,
              ActionId::InvertSelection, ActionId::DeleteLayer, ActionId::MergeLayerDown}) {
            QVERIFY2(!actions->get(id)->isEnabled(), qPrintable(actions->get(id)->objectName()));
        }
        for (ActionId id : {ActionId::Save, ActionId::Copy, ActionId::SelectAll, ActionId::ResizeSkew,
                            ActionId::AddLayer}) {
            QVERIFY2(actions->get(id)->isEnabled(), qPrintable(actions->get(id)->objectName()));
        }
    }

    void menusContainPaintCommands()
    {
        QStringList file;
        for (QAction *action : m_window->findChild<QMenu *>(QStringLiteral("menu.file"))->actions()) {
            file.append(action->menu() ? action->menu()->objectName() : action->objectName());
        }
        file.removeAll(QString());
        QCOMPARE(file, (QStringList{
                           QStringLiteral("action.file.new"),
                           QStringLiteral("action.file.open"),
                           QStringLiteral("menu.import"),
                           QStringLiteral("menu.recent"),
                           QStringLiteral("action.file.save"),
                           QStringLiteral("menu.saveAs"),
                           QStringLiteral("action.file.saveAsProject"),
                           QStringLiteral("menu.print"),
                           QStringLiteral("menu.wallpaper"),
                           QStringLiteral("action.file.imageProperties"),
                           QStringLiteral("action.file.quit"),
                       }));
    }

    void unimplementedCommandsSaySo()
    {
        m_window->actionRegistry()->get(ActionId::Print)->trigger();
        QVERIFY(m_window->statusBar()->currentMessage().contains(QLatin1String("not available yet")));
    }

    void zoomActionsDriveCanvasAndStatusBar()
    {
        CanvasView *canvas = m_window->canvas();
        ActionRegistry *actions = m_window->actionRegistry();
        auto *box = m_window->findChild<QComboBox *>(QStringLiteral("status.zoom.box"));
        QCOMPARE(canvas->zoom(), 1.0);
        actions->get(ActionId::ZoomIn)->trigger();
        QCOMPARE(canvas->zoom(), 2.0);
        QCOMPARE(box->currentText(), QStringLiteral("200%"));
        actions->get(ActionId::ZoomOut)->trigger();
        actions->get(ActionId::ZoomOut)->trigger();
        QCOMPARE(canvas->zoom(), 0.75);
        QCOMPARE(box->currentText(), QStringLiteral("75%"));
        actions->get(ActionId::ZoomFit)->trigger();
        QVERIFY(canvas->zoom() < 1.0);
        QVERIFY(canvas->canvasRect().width() <= canvas->viewport()->width());
        actions->get(ActionId::ZoomActualSize)->trigger();
        QCOMPARE(canvas->zoom(), 1.0);

        box->setEditText(QStringLiteral("58.5%"));
        QTest::keyClick(box, Qt::Key_Return);
        QCOMPARE(canvas->zoom(), 0.585);
        QCOMPARE(formatZoom(0.125), QStringLiteral("12.5%"));
        // The logarithmic slider has 1000 steps: round trips are close, not exact.
        QVERIFY(qAbs(ZoomControls::zoomForSliderPosition(ZoomControls::sliderPosition(3.0)) - 3.0) < 0.01);
    }

    void ctrlWheelZoomsAroundPointer()
    {
        CanvasView *canvas = m_window->canvas();
        const QPointF anchor = canvas->canvasRect().center() + QPointF(100, 50);
        const QPointF before = canvas->viewportToDocument(anchor);
        QWheelEvent wheel(anchor, canvas->viewport()->mapToGlobal(anchor), QPoint(), QPoint(0, 120),
                          Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(canvas->viewport(), &wheel);
        QVERIFY(canvas->zoom() > 1.0);
        const QPointF after = canvas->viewportToDocument(anchor);
        QVERIFY2((after - before).manhattanLength() < 1.0, "the point under the pointer must stay put");
    }

    void statusBarTracksPointer()
    {
        CanvasView *canvas = m_window->canvas();
        auto *label = m_window->findChild<QLabel *>(QStringLiteral("status.cursor.text"));
        const QPoint inside = canvas->canvasRect().topLeft().toPoint() + QPoint(10, 20);
        QTest::mouseMove(canvas->viewport(), inside);
        QTRY_COMPARE(label->text(), QStringLiteral("10, 20px"));
        QTest::mouseMove(canvas->viewport(), QPoint(2, 2));
        QTRY_COMPARE(label->text(), QString());
        QCOMPARE(m_window->findChild<QLabel *>(QStringLiteral("status.canvasSize.text"))->text(),
                 QStringLiteral("1152 × 648px"));
    }

    void contextMenuOnlyForSelectionTools()
    {
        CanvasView *canvas = m_window->canvas();
        bool shown = false;
        const auto expectMenu = [&shown] {
            if (auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget())) {
                shown = menu->objectName() == QLatin1String("menu.canvasContext");
                menu->close();
            }
        };
        QTimer::singleShot(0, this, expectMenu);
        QContextMenuEvent pencil(QContextMenuEvent::Mouse, QPoint(50, 50),
                                 canvas->viewport()->mapToGlobal(QPoint(50, 50)));
        QApplication::sendEvent(canvas->viewport(), &pencil);
        QVERIFY(!shown);

        m_window->toolSettings()->setTool(Tool::RectangleSelect);
        QTimer::singleShot(0, this, expectMenu);
        QContextMenuEvent select(QContextMenuEvent::Mouse, QPoint(50, 50),
                                 canvas->viewport()->mapToGlobal(QPoint(50, 50)));
        QApplication::sendEvent(canvas->viewport(), &select);
        QVERIFY(shown);
    }

private:
    std::unique_ptr<MainWindow> m_window;
};

QTEST_MAIN(TestActions)
#include "tst_actions.moc"
