#include "ActionRegistry.h"
#include "MainWindow.h"
#include "canvas/CanvasView.h"
#include "editor/Editor.h"
#include "tools/SelectionTool.h"
#include "tools/ToolSettings.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <qpa/qplatformdrag.h>
#include <qpa/qwindowsysteminterface.h>

using namespace paint;

// Clipboard and drag-and-drop through the main window, with payloads shaped like other apps'.
class TestClipboard : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        QVERIFY(m_dir.isValid());
        QGuiApplication::clipboard()->clear();
        m_window = std::make_unique<MainWindow>();
        m_window->resize(1280, 800);
        m_window->show();
        QVERIFY(QTest::qWaitForWindowActive(m_window.get()));
        QPainter painter(&editor()->document().layer(0).image);
        painter.fillRect(QRect(10, 10, 20, 20), Qt::red);
    }

    void cleanup()
    {
        editor()->history().setClean();
        m_window.reset();
    }

    void copyPutsTheSelectionOnTheClipboard()
    {
        select(QRect(10, 10, 10, 5));
        action(ActionId::Copy)->trigger();
        const QImage copied = QGuiApplication::clipboard()->image();
        QCOMPARE(copied.size(), QSize(10, 5));
        QCOMPARE(copied.pixelColor(2, 2), QColor(Qt::red));

        // Without a selection, Copy takes the whole layer (transparent where nothing is drawn).
        editor()->selection()->deselect();
        action(ActionId::Copy)->trigger();
        const QImage layer = QGuiApplication::clipboard()->image();
        QCOMPARE(layer.size(), editor()->document().size());
        QCOMPARE(layer.pixelColor(0, 0).alpha(), 0);
    }

    void copyVisibleLayersIncludesTheBackground()
    {
        select(QRect(0, 0, 5, 5));
        action(ActionId::CopyVisibleLayers)->trigger();
        QCOMPARE(QGuiApplication::clipboard()->image().pixelColor(1, 1), QColor(Qt::white));
    }

    void cutLeavesTransparency()
    {
        select(QRect(10, 10, 20, 20));
        QVERIFY(action(ActionId::Cut)->isEnabled());
        action(ActionId::Cut)->trigger();
        QCOMPARE(layerPixel(15, 15).alpha(), 0);
        QCOMPARE(QGuiApplication::clipboard()->image().pixelColor(5, 5), QColor(Qt::red));
        QVERIFY(!action(ActionId::Cut)->isEnabled());
    }

    void pasteFloatsAtTheVisibleTopLeft()
    {
        QGuiApplication::clipboard()->setImage(solid(QSize(8, 6), Qt::magenta));
        QTRY_VERIFY(action(ActionId::Paste)->isEnabled());
        action(ActionId::Paste)->trigger();
        QTRY_VERIFY(editor()->selection()->isFloating());
        QCOMPARE(editor()->selection()->bounds(), QRect(0, 0, 8, 6));
        QCOMPARE(layerPixel(3, 3), QColor(Qt::magenta));
        QCOMPARE(toolSettings()->tool(), Tool::RectangleSelect);
        QCOMPARE(statusText(QStringLiteral("status.selection.text")), QStringLiteral("8 × 6px"));
    }

    void pasteFollowsTheClipboardContent()
    {
        QGuiApplication::clipboard()->setText(QStringLiteral("words"));
        QTRY_VERIFY(!action(ActionId::Paste)->isEnabled());
        auto *files = new QMimeData;
        files->setUrls({QUrl::fromLocalFile(savedImage(QStringLiteral("a.png"), Qt::blue))});
        QGuiApplication::clipboard()->setMimeData(files);
        QTRY_VERIFY(action(ActionId::Paste)->isEnabled());
        action(ActionId::Paste)->trigger();
        QTRY_COMPARE(layerPixel(1, 1), QColor(Qt::blue));
    }

    void largerImageOffersToEnlargeTheCanvas()
    {
        QGuiApplication::clipboard()->setImage(solid(QSize(1500, 100), Qt::green));
        answerPrompt(QMessageBox::AcceptRole);
        action(ActionId::Paste)->trigger();
        QTRY_COMPARE(editor()->document().size(), QSize(1500, 648));
        QCOMPARE(statusText(QStringLiteral("status.canvasSize.text")), QStringLiteral("1500 × 648px"));
        QCOMPARE(layerPixel(1400, 50), QColor(Qt::green));
    }

    void droppedFileLandsWhereItWasDropped()
    {
        QMimeData mime;
        mime.setUrls({QUrl::fromLocalFile(savedImage(QStringLiteral("drop.png"), Qt::yellow))});
        CanvasView *canvas = m_window->canvas();
        const QPointF target = canvas->documentToViewport(QRectF(100, 50, 0, 0)).topLeft();
        QVERIFY(dropOnWindow(&mime, canvas->viewport()->mapTo(m_window.get(), target.toPoint())));
        QTRY_VERIFY(editor()->selection()->isFloating());
        QCOMPARE(editor()->selection()->bounds().topLeft(), QPoint(100, 50));
        QCOMPARE(layerPixel(101, 51), QColor(Qt::yellow));
        QVERIFY(editor()->filePath().isEmpty());
    }

    void fileDroppedBesideTheImageIsOpened()
    {
        editor()->history().setClean();
        QMimeData mime;
        const QString path = savedImage(QStringLiteral("open-me.png"), Qt::cyan, QSize(30, 20));
        mime.setUrls({QUrl::fromLocalFile(path)});
        // The menu bar area is outside the canvas.
        QVERIFY(dropOnWindow(&mime, QPoint(m_window->width() - 20, 10)));
        QTRY_COMPARE(editor()->filePath(), path);
        QCOMPARE(editor()->document().size(), QSize(30, 20));
    }

    void cropAndSelectAllFromTheMenus()
    {
        action(ActionId::SelectAll)->trigger();
        QCOMPARE(statusText(QStringLiteral("status.selection.text")), QStringLiteral("1152 × 648px"));
        select(QRect(10, 10, 20, 20));
        action(ActionId::Crop)->trigger();
        QCOMPARE(editor()->document().size(), QSize(20, 20));
        QCOMPARE(statusText(QStringLiteral("status.canvasSize.text")), QStringLiteral("20 × 20px"));
        QTest::keyClick(m_window.get(), Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(editor()->document().size(), QSize(1152, 648));
    }

private:
    Editor *editor() const { return m_window->editor(); }
    ToolSettings *toolSettings() const { return m_window->toolSettings(); }
    QAction *action(ActionId id) const { return m_window->actionRegistry()->get(id); }
    QColor layerPixel(int x, int y) const { return editor()->document().layer(0).image.pixelColor(x, y); }

    QString statusText(const QString &name) const { return m_window->findChild<QLabel *>(name)->text(); }

    // Drags out a rectangle selection with the selection tool, as a user would.
    void select(const QRect &area)
    {
        // A press inside an existing selection would move it instead.
        editor()->selection()->deselect();
        toolSettings()->setTool(Tool::RectangleSelect);
        InteractiveTool *tool = editor()->currentTool();
        tool->press({QPointF(area.topLeft()), Qt::LeftButton, {}, 1});
        tool->move({QPointF(area.topLeft() + QPoint(area.width(), area.height())), Qt::NoButton, {}, 1});
        tool->release({QPointF(area.topLeft() + QPoint(area.width(), area.height())), Qt::LeftButton, {}, 1});
    }

    // Delivers a drag and drop through the platform layer, as a compositor does for a drag from
    // another application. Returns whether the target accepted the drag.
    bool dropOnWindow(const QMimeData *mime, QPoint position)
    {
        QWindow *window = m_window->windowHandle();
        const Qt::DropActions actions = Qt::CopyAction | Qt::MoveAction;
        const QPlatformDragQtResponse drag = QWindowSystemInterface::handleDrag(
            window, mime, position, actions, Qt::LeftButton, Qt::NoModifier);
        if (!drag.isAccepted()) {
            return false;
        }
        QWindowSystemInterface::handleDrop(window, mime, position, actions, Qt::LeftButton, Qt::NoModifier);
        return true;
    }

    static QImage solid(QSize size, const QColor &color)
    {
        QImage image(size, QImage::Format_ARGB32);
        image.fill(color);
        return image;
    }

    QString savedImage(const QString &name, const QColor &color, QSize size = QSize(12, 8))
    {
        const QString path = m_dir.filePath(name);
        solid(size, color).save(path);
        return path;
    }

    // Answers the next message box once it shows up.
    void answerPrompt(QMessageBox::ButtonRole role)
    {
        auto *timer = new QTimer(this);
        timer->setInterval(10);
        connect(timer, &QTimer::timeout, this, [timer, role] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!box) {
                return;
            }
            timer->deleteLater();
            for (QAbstractButton *button : box->buttons()) {
                if (box->buttonRole(button) == role) {
                    button->click();
                    return;
                }
            }
        });
        timer->start();
    }

    QTemporaryDir m_dir;
    std::unique_ptr<MainWindow> m_window;
};

QTEST_MAIN(TestClipboard)
#include "tst_clipboard.moc"
