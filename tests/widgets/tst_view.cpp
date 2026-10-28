#include "ActionRegistry.h"
#include "MainWindow.h"
#include "canvas/CanvasView.h"
#include "canvas/Ruler.h"
#include "widgets/FullScreenView.h"

#include <QAction>
#include <QApplication>
#include <QScrollBar>
#include <QTest>

// The View menu's options: rulers, gridlines, the thumbnail and the full screen picture.
class TestView : public QObject
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

    void stepsFollowTheZoom()
    {
        // Labelled ruler steps leave room for their numbers; gridlines stay 8 screen pixels apart.
        QCOMPARE(Ruler::step(1.0, 56), 100);
        QCOMPARE(Ruler::step(8.0, 56), 10);
        QCOMPARE(Ruler::step(0.125, 56), 500);
        QCOMPARE(CanvasView::gridSpacing(1.0), 10);
        QCOMPARE(CanvasView::gridSpacing(4.0), 2);
        QCOMPARE(CanvasView::gridSpacing(8.0), 1);
        QCOMPARE(CanvasView::gridSpacing(0.5), 20);
    }

    void rulersTakeAStripAlongTheCanvas()
    {
        CanvasView *canvas = m_window->canvas();
        const QRect before = canvas->viewport()->geometry();
        QTest::keyClick(m_window.get(), Qt::Key_R, Qt::ControlModifier);
        QVERIFY(canvas->rulersVisible());
        QCOMPARE(canvas->viewport()->geometry().topLeft(), before.topLeft() + QPoint(22, 22));
        auto *horizontal = canvas->findChild<QWidget *>(QStringLiteral("canvasView.horizontalRuler"));
        auto *vertical = canvas->findChild<QWidget *>(QStringLiteral("canvasView.verticalRuler"));
        QVERIFY(horizontal->isVisible() && vertical->isVisible());
        QCOMPARE(horizontal->geometry().bottom() + 1, canvas->viewport()->geometry().top());
        QCOMPARE(vertical->geometry().right() + 1, canvas->viewport()->geometry().left());
        QTest::keyClick(m_window.get(), Qt::Key_R, Qt::ControlModifier);
        QCOMPARE(canvas->viewport()->geometry().topLeft(), before.topLeft());
        QVERIFY(!horizontal->isVisible());
    }

    void gridlinesDrawOverThePicture()
    {
        CanvasView *canvas = m_window->canvas();
        const QPoint corner = canvas->canvasRect().topLeft().toPoint();
        // A white pixel just inside a gridline at 10 px from the corner, and one between lines.
        const auto pixelAt = [&](QPoint offset) {
            return canvas->viewport()->grab(QRect(corner + offset, QSize(1, 1))).toImage().pixelColor(0, 0);
        };
        QCOMPARE(pixelAt(QPoint(10, 5)), QColor(Qt::white));
        QTest::keyClick(m_window.get(), Qt::Key_G, Qt::ControlModifier);
        QVERIFY(canvas->gridVisible());
        QVERIFY(pixelAt(QPoint(10, 5)).lightness() < 240);
        QCOMPARE(pixelAt(QPoint(15, 5)), QColor(Qt::white));
    }

    void thumbnailMovesTheView()
    {
        CanvasView *canvas = m_window->canvas();
        canvas->setZoom(4.0);
        m_window->actionRegistry()->get(ActionId::Thumbnail)->trigger();
        auto *thumbnail = canvas->findChild<QWidget *>(QStringLiteral("canvasView.thumbnail"));
        QVERIFY(thumbnail->isVisible());
        // In the top-right corner of the canvas area.
        const QRect area = canvas->viewport()->geometry();
        QVERIFY(thumbnail->geometry().right() < area.right() &&
                thumbnail->geometry().right() > area.right() - 40);
        QVERIFY(thumbnail->geometry().top() < area.top() + 40);

        // A click near its bottom-right corner brings that corner of the picture into view.
        const QRectF before = canvas->visibleDocumentRect();
        QTest::mouseClick(thumbnail, Qt::LeftButton, {},
                          QPoint(thumbnail->width() - 8, thumbnail->height() - 8));
        const QRectF after = canvas->visibleDocumentRect();
        QVERIFY(after.left() > before.left() + 100 && after.top() > before.top() + 100);
        QVERIFY(after.contains(QPointF(1100, 600)));
    }

    void fullScreenShowsThePictureUntilAKey()
    {
        QAction *action = m_window->actionRegistry()->get(ActionId::FullScreen);
        QTest::keyClick(m_window.get(), Qt::Key_F11);
        auto *view = m_window->findChild<FullScreenView *>(QStringLiteral("fullScreenView"));
        QVERIFY(view && view->isVisible());
        QVERIFY(view->windowState() & Qt::WindowFullScreen);
        QVERIFY(action->isChecked());
        // The main window itself stays as it was.
        QVERIFY(!(m_window->windowState() & Qt::WindowFullScreen));
        QTest::keyClick(view, Qt::Key_Escape);
        QTRY_VERIFY(!m_window->findChild<FullScreenView *>(QStringLiteral("fullScreenView")));
        QVERIFY(!action->isChecked());
    }

private:
    std::unique_ptr<MainWindow> m_window;
};

QTEST_MAIN(TestView)
#include "tst_view.moc"
