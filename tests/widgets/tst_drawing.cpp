#include "ActionRegistry.h"
#include "MainWindow.h"
#include "canvas/CanvasView.h"
#include "editor/Editor.h"
#include "tools/ToolSettings.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

using namespace paint;

// Drawing through real mouse input on the canvas, plus the document commands around it.
class TestDrawing : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        QVERIFY(m_dir.isValid());
        m_window = std::make_unique<MainWindow>();
        m_window->resize(1280, 800);
        m_window->show();
        QVERIFY(QTest::qWaitForWindowActive(m_window.get()));
    }

    void cleanup()
    {
        // Nothing to save at the end of a test.
        m_window->editor()->history().setClean();
        m_window.reset();
    }

    void pencilDragDrawsAndUndoRemovesIt()
    {
        drag({QPoint(10, 10), QPoint(60, 10)});
        QCOMPARE(pixel(30, 10), QColor(Qt::black));
        QVERIFY(m_window->isWindowModified());
        QVERIFY(action(ActionId::Undo)->isEnabled());

        QTest::keyClick(m_window.get(), Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(pixel(30, 10), QColor(Qt::white));
        QVERIFY(!m_window->isWindowModified());
        QVERIFY(action(ActionId::Redo)->isEnabled());
        QTest::keyClick(m_window.get(), Qt::Key_Y, Qt::ControlModifier);
        QCOMPARE(pixel(30, 10), QColor(Qt::black));
    }

    void rightButtonDrawsWithColorTwo()
    {
        settings()->setColor(ColorSlot::Secondary, Qt::red);
        drag({QPoint(10, 20), QPoint(40, 20)}, Qt::RightButton);
        QCOMPARE(pixel(25, 20), QColor(Qt::red));
    }

    void escapeAbandonsTheStroke()
    {
        QWidget *viewport = m_window->canvas()->viewport();
        QTest::mousePress(viewport, Qt::LeftButton, {}, toViewport(QPoint(10, 30)));
        QTest::mouseMove(viewport, toViewport(QPoint(50, 30)));
        QCOMPARE(pixel(30, 30), QColor(Qt::black));
        QTest::keyClick(m_window->canvas(), Qt::Key_Escape);
        QCOMPARE(pixel(30, 30), QColor(Qt::white));
        QTest::mouseRelease(viewport, Qt::LeftButton, {}, toViewport(QPoint(50, 30)));
        QVERIFY(!m_window->editor()->history().canUndo());
    }

    void fillPickerAndMagnifierWorkFromTheCanvas()
    {
        settings()->setTool(Tool::Fill);
        settings()->setColor(ColorSlot::Primary, QColor(0x22b14c));
        QTest::mouseClick(m_window->canvas()->viewport(), Qt::LeftButton, {}, toViewport(QPoint(100, 100)));
        QCOMPARE(pixel(500, 300), QColor(0x22b14c));

        settings()->setTool(Tool::ColorPicker);
        QTest::mouseClick(m_window->canvas()->viewport(), Qt::RightButton, {}, toViewport(QPoint(5, 5)));
        QCOMPARE(settings()->color(ColorSlot::Secondary), QColor(0x22b14c));
        QCOMPARE(settings()->tool(), Tool::Fill);

        settings()->setTool(Tool::Magnifier);
        QTest::mouseClick(m_window->canvas()->viewport(), Qt::LeftButton, {}, toViewport(QPoint(200, 150)));
        QCOMPARE(m_window->canvas()->zoom(), 2.0);
    }

    void saveAndOpenUpdateTitleAndStatusBar()
    {
        drag({QPoint(10, 10), QPoint(20, 10)});
        const QString path = m_dir.filePath(QStringLiteral("drawing.png"));
        QVERIFY(m_window->saveFile(path));
        QCOMPARE(m_window->windowTitle(), QStringLiteral("drawing.png[*]"));
        QVERIFY(!m_window->isWindowModified());
        auto *fileSize = m_window->findChild<QLabel *>(QStringLiteral("status.fileSize.text"));
        QVERIFY(fileSize->isVisible());
        QVERIFY(fileSize->text().startsWith(QLatin1String("Size: ")));

        QImage other(300, 200, QImage::Format_RGB32);
        other.fill(Qt::magenta);
        const QString otherPath = m_dir.filePath(QStringLiteral("other.jpg"));
        QVERIFY(other.save(otherPath));
        QVERIFY(m_window->openFile(otherPath));
        QCOMPARE(m_window->editor()->document().size(), QSize(300, 200));
        QCOMPARE(m_window->windowTitle(), QStringLiteral("other.jpg[*]"));
        QCOMPARE(m_window->findChild<QLabel *>(QStringLiteral("status.canvasSize.text"))->text(),
                 QStringLiteral("300 × 200px"));
        QVERIFY(!action(ActionId::Undo)->isEnabled());
    }

    void unsavedWorkPromptsBeforeNew()
    {
        drag({QPoint(10, 10), QPoint(20, 10)});

        // Cancel keeps the drawing.
        answerPrompt(QMessageBox::RejectRole);
        action(ActionId::New)->trigger();
        QCOMPARE(pixel(15, 10), QColor(Qt::black));

        // "Don't save" starts over.
        answerPrompt(QMessageBox::DestructiveRole);
        action(ActionId::New)->trigger();
        QCOMPARE(pixel(15, 10), QColor(Qt::white));
        QVERIFY(!m_window->isWindowModified());
    }

    void brokenFileShowsAnError()
    {
        const QString path = m_dir.filePath(QStringLiteral("broken.png"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("nope");
        file.close();
        bool warned = false;
        QTimer::singleShot(0, this, [&warned] {
            if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                warned = box->text().contains(QLatin1String("can't be opened"));
                box->close();
            }
        });
        QVERIFY(!m_window->openFile(path));
        QVERIFY(warned);
    }

private:
    ToolSettings *settings() const { return m_window->toolSettings(); }
    QAction *action(ActionId id) const { return m_window->actionRegistry()->get(id); }

    QColor pixel(int x, int y) const
    {
        return m_window->editor()->document().composite(QRect(x, y, 1, 1)).pixelColor(0, 0);
    }

    QPoint toViewport(QPoint documentPoint) const
    {
        const QRectF canvas = m_window->canvas()->canvasRect();
        const double zoom = m_window->canvas()->zoom();
        // Integer viewport points: at 100 % each maps to the top-left corner of its pixel.
        return (canvas.topLeft() + QPointF(documentPoint) * zoom).toPoint();
    }

    void drag(const QList<QPoint> &points, Qt::MouseButton button = Qt::LeftButton)
    {
        QWidget *viewport = m_window->canvas()->viewport();
        QTest::mousePress(viewport, button, {}, toViewport(points.first()));
        for (const QPoint &point : points) {
            QTest::mouseMove(viewport, toViewport(point));
        }
        QTest::mouseRelease(viewport, button, {}, toViewport(points.last()));
    }

    void answerPrompt(QMessageBox::ButtonRole answer)
    {
        QTimer::singleShot(0, this, [answer] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY2(box, "expected the save prompt");
            for (QAbstractButton *button : box->buttons()) {
                if (box->buttonRole(button) == answer) {
                    button->click();
                    return;
                }
            }
            QFAIL("the prompt has no such button");
        });
    }

    QTemporaryDir m_dir;
    std::unique_ptr<MainWindow> m_window;
};

QTEST_MAIN(TestDrawing)
#include "tst_drawing.moc"
