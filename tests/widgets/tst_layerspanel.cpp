#include "ActionRegistry.h"
#include "MainWindow.h"
#include "editor/Editor.h"
#include "panels/LayersDock.h"

#include <QAction>
#include <QListView>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

using namespace paint;

// The layers panel driven with the mouse, and layered projects through the window.
class TestLayersPanel : public QObject
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
        action(ActionId::LayersPanel)->trigger();
        QVERIFY(dock()->isVisible());
    }

    void cleanup()
    {
        m_window->editor()->history().setClean();
        m_window.reset();
    }

    void addButtonAddsAndTilesPickTheLayer()
    {
        QVERIFY(!action(ActionId::DeleteLayer)->isEnabled());
        QVERIFY(!action(ActionId::MergeLayerDown)->isEnabled());
        m_window->findChild<QToolButton *>(QStringLiteral("layers.add"))->click();
        QCOMPARE(document().layerCount(), 2);
        QCOMPARE(document().activeLayerIndex(), 1);
        QCOMPARE(list()->model()->rowCount(), 2);
        QVERIFY(action(ActionId::DeleteLayer)->isEnabled());
        QVERIFY(action(ActionId::MergeLayerDown)->isEnabled());

        const int steps = m_window->editor()->history().stack()->index();
        clickTile(1); // the bottom tile is layer 0
        QCOMPARE(document().activeLayerIndex(), 0);
        QVERIFY(!action(ActionId::MergeLayerDown)->isEnabled());
        QCOMPARE(m_window->editor()->history().stack()->index(), steps); // picking a layer is not an edit
    }

    void eyeHidesTheLayer()
    {
        const QRect tile = tileRect(0);
        // The eye sits in the tile's top-right corner.
        QTest::mouseClick(list()->viewport(), Qt::LeftButton, {}, tile.topRight() + QPoint(-10, 12));
        QVERIFY(!document().layer(0).visible);
        QCOMPARE(action(ActionId::ToggleLayerVisibility)->text(), QStringLiteral("&Show layer"));
        action(ActionId::ToggleLayerVisibility)->trigger();
        QVERIFY(document().layer(0).visible);
    }

    void draggingATileReordersLayers()
    {
        action(ActionId::AddLayer)->trigger();
        action(ActionId::AddLayer)->trigger();
        // Top to bottom: Layer 3, Layer 2, Layer 1. Drag the top tile below the bottom one.
        QWidget *viewport = list()->viewport();
        const QPoint start = list()->visualRect(list()->model()->index(0, 0)).center();
        const QRect last = list()->visualRect(list()->model()->index(2, 0));
        QTest::mousePress(viewport, Qt::LeftButton, {}, start);
        QTest::mouseMove(viewport, start + QPoint(0, 10));
        QTest::mouseMove(viewport, last.bottomLeft() + QPoint(20, -2));
        QTest::mouseRelease(viewport, Qt::LeftButton, {}, last.bottomLeft() + QPoint(20, -2));
        QCOMPARE(document().layer(0).name, QStringLiteral("Layer 3"));
        QCOMPARE(document().layer(1).name, QStringLiteral("Layer 1"));
        QCOMPARE(document().layer(2).name, QStringLiteral("Layer 2"));
        QCOMPARE(document().activeLayerIndex(), 0);
        QTest::keyClick(m_window.get(), Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(document().layer(2).name, QStringLiteral("Layer 3"));
    }

    void backgroundActionsHideAndShow()
    {
        QTest::keyClick(m_window.get(), Qt::Key_H, Qt::ControlModifier | Qt::ShiftModifier);
        QVERIFY(!document().isBackgroundVisible());
        QCOMPARE(action(ActionId::ToggleBackground)->text(), QStringLiteral("Show &background"));
        QCOMPARE(m_window->documentPixel(5, 5), QStringLiteral("#00000000"));
    }

    void projectsKeepLayersThroughTheWindow()
    {
        action(ActionId::AddLayer)->trigger();
        action(ActionId::AddLayer)->trigger();
        m_window->editor()->setLayerVisible(1, false);
        const QString path = m_dir.filePath(QStringLiteral("drawing.ora"));
        QVERIFY(m_window->saveFile(path));
        QVERIFY(!m_window->isWindowModified());

        m_window->editor()->newDocument();
        QCOMPARE(document().layerCount(), 1);
        QVERIFY(m_window->openFile(path));
        QCOMPARE(document().layerCount(), 3);
        QVERIFY(!document().layer(1).visible);
        QCOMPARE(list()->model()->rowCount(), 3);
        QVERIFY(m_window->windowTitle().startsWith(QStringLiteral("drawing.ora")));
    }

private:
    const Document &document() const { return m_window->editor()->document(); }
    QAction *action(ActionId id) const { return m_window->actionRegistry()->get(id); }
    LayersDock *dock() const { return m_window->findChild<LayersDock *>(QStringLiteral("layersDock")); }
    QListView *list() const { return dock()->list(); }

    QRect tileRect(int row) const
    {
        // Tiles are 80 px wide, centred in their row.
        const QRect item = list()->visualRect(list()->model()->index(row, 0));
        const QImage thumbnail =
            list()->model()->index(row, 0).data(LayerModel::ThumbnailRole).value<QImage>();
        QRect tile(QPoint(), thumbnail.size().scaled(QSize(80, 120), Qt::KeepAspectRatio));
        tile.moveCenter(item.center());
        return tile;
    }

    void clickTile(int row)
    {
        QTest::mouseClick(list()->viewport(), Qt::LeftButton, {}, tileRect(row).center());
    }

    QTemporaryDir m_dir;
    std::unique_ptr<MainWindow> m_window;
};

QTEST_MAIN(TestLayersPanel)
#include "tst_layerspanel.moc"
