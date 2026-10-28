#include "MainWindow.h"
#include "canvas/CanvasView.h"
#include "editor/Editor.h"
#include "io/ImageIO.h"
#include "tools/ToolSettings.h"

#include <QPainter>
#include <QTemporaryDir>
#include <QTest>

using namespace paint;

// How the editor copes with a 4K picture (3840 × 2160, three layers): the operations a user
// waits for. Run with: cmake --preset bench && cmake --build --preset bench && ctest --preset bench
class BenchLarge : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        m_picture = QImage(Size, QImage::Format_ARGB32_Premultiplied);
        QPainter painter(&m_picture);
        QLinearGradient gradient(0, 0, Size.width(), Size.height());
        gradient.setColorAt(0, QColor(20, 90, 200));
        gradient.setColorAt(1, QColor(240, 200, 40));
        painter.fillRect(m_picture.rect(), gradient);
        painter.end();
        m_path = m_dir.filePath(QStringLiteral("large.png"));
        QVERIFY(m_picture.save(m_path));
    }

    void init()
    {
        m_settings = std::make_unique<ToolSettings>();
        m_editor = std::make_unique<Editor>(m_settings.get());
        QCOMPARE(m_editor->open(m_path), QString());
        m_editor->addLayer();
        m_editor->addLayer();
    }

    void brushStroke()
    {
        m_settings->setTool(Tool::Brush);
        m_settings->setSize(30);
        QBENCHMARK
        {
            stroke();
            m_editor->undo();
        }
    }

    void crayonStroke()
    {
        m_settings->setTool(Tool::Brush);
        m_settings->setBrushType(BrushType::Crayon);
        m_settings->setSize(30);
        QBENCHMARK
        {
            stroke();
            m_editor->undo();
        }
    }

    void fillWholeLayer()
    {
        m_settings->setTool(Tool::Fill);
        m_editor->setActiveLayer(0);
        QBENCHMARK
        {
            click({1000, 1000});
            m_editor->undo();
        }
    }

    void compositeThreeLayers()
    {
        QBENCHMARK
        {
            const QImage composite = m_editor->document().composite();
            QVERIFY(!composite.isNull());
        }
    }

    void rotateAndUndo()
    {
        QBENCHMARK_ONCE
        {
            m_editor->rotate(RotateBy::Right90);
            m_editor->undo();
        }
    }

    void savePng()
    {
        const QString path = m_dir.filePath(QStringLiteral("saved.png"));
        QBENCHMARK_ONCE
        {
            QCOMPARE(m_editor->save(path, *saveFormatForPath(path)), QString());
        }
    }

    void openPng()
    {
        QBENCHMARK_ONCE
        {
            QCOMPARE(m_editor->open(m_path), QString());
        }
    }

    void canvasRepaint_data()
    {
        QTest::addColumn<double>("zoom");
        QTest::newRow("fit") << 0.0;
        QTest::newRow("100%") << 1.0;
        QTest::newRow("400%") << 4.0;
    }

    void canvasRepaint()
    {
        QFETCH(double, zoom);
        MainWindow window;
        window.resize(1920, 1080);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(window.openFile(m_path));
        zoom > 0 ? window.canvas()->setZoom(zoom) : window.canvas()->zoomToFit();
        QBENCHMARK
        {
            window.canvas()->viewport()->repaint();
        }
    }

private:
    static constexpr QSize Size{3840, 2160};

    void stroke()
    {
        InteractiveTool *tool = m_editor->currentTool();
        tool->press({{100, 100}, Qt::LeftButton, {}});
        for (int i = 1; i <= 300; ++i) {
            tool->move({{100 + i * 12.0, 100 + 1900 * (i % 2 ? 0.3 : 0.6) + i}, Qt::NoButton, {}});
        }
        tool->release({{3700, 1500}, Qt::LeftButton, {}});
    }

    void click(QPointF point)
    {
        InteractiveTool *tool = m_editor->currentTool();
        tool->press({point, Qt::LeftButton, {}});
        tool->release({point, Qt::LeftButton, {}});
    }

    QTemporaryDir m_dir;
    QImage m_picture;
    QString m_path;
    std::unique_ptr<ToolSettings> m_settings;
    std::unique_ptr<Editor> m_editor;
};

QTEST_MAIN(BenchLarge)
#include "bench_large.moc"
