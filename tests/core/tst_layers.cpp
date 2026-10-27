#include "editor/Editor.h"
#include "io/ImageIO.h"
#include "io/OpenRaster.h"
#include "tools/SelectionTool.h"
#include "tools/ToolSettings.h"

#include <QBuffer>
#include <QFile>
#include <QPainter>
#include <QTemporaryDir>
#include <QTest>

#include <zip.h>

using namespace paint;

// Layer commands through Editor, and OpenRaster projects.
class TestLayers : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        QVERIFY(m_dir.isValid());
        m_settings = std::make_unique<ToolSettings>();
        m_editor = std::make_unique<Editor>(m_settings.get());
        m_editor->newDocument(QSize(20, 10));
    }

    void addedLayersGoAboveTheActiveOne()
    {
        m_editor->addLayer();
        QCOMPARE(document().layerCount(), 2);
        QCOMPARE(document().activeLayerIndex(), 1);
        QCOMPARE(document().layer(1).name, QStringLiteral("Layer 2"));
        m_editor->setActiveLayer(0);
        m_editor->addLayer();
        // Between the two, and active.
        QCOMPARE(document().activeLayerIndex(), 1);
        QCOMPARE(document().layer(1).name, QStringLiteral("Layer 3"));
        QCOMPARE(document().layer(2).name, QStringLiteral("Layer 2"));
        m_editor->undo();
        QCOMPARE(document().layerCount(), 2);
        QCOMPARE(m_editor->history().stack()->index(), 1); // picking a layer is not an edit
    }

    void hiddenLayersDoNotShow()
    {
        fill(0, Qt::red);
        m_editor->addLayer();
        fill(1, Qt::blue);
        QCOMPARE(composite(5, 5), QColor(Qt::blue));
        m_editor->setLayerVisible(1, false);
        QCOMPARE(composite(5, 5), QColor(Qt::red));
        m_editor->undo();
        QCOMPARE(composite(5, 5), QColor(Qt::blue));
    }

    void deletingActivatesTheLayerBelow()
    {
        m_editor->addLayer();
        m_editor->addLayer();
        m_editor->setActiveLayer(1);
        m_editor->deleteLayer();
        QCOMPARE(document().layerCount(), 2);
        QCOMPARE(document().activeLayerIndex(), 0);
        QCOMPARE(document().layer(1).name, QStringLiteral("Layer 3"));
        m_editor->deleteLayer();
        m_editor->deleteLayer(); // the last layer stays
        QCOMPARE(document().layerCount(), 1);
    }

    void duplicateAndMergeDown()
    {
        fill(0, Qt::red, QRect(0, 0, 5, 5));
        m_editor->duplicateLayer();
        QCOMPARE(document().layerCount(), 2);
        QCOMPARE(pixel(1, 2, 2), QColor(Qt::red));
        fill(1, Qt::blue, QRect(10, 0, 5, 5));
        m_editor->mergeLayerDown();
        QCOMPARE(document().layerCount(), 1);
        QCOMPARE(pixel(0, 2, 2), QColor(Qt::red));
        QCOMPARE(pixel(0, 12, 2), QColor(Qt::blue));
        m_editor->undo();
        QCOMPARE(document().layerCount(), 2);
        QCOMPARE(pixel(0, 12, 2).alpha(), 0);
    }

    void movingChangesWhatIsOnTop()
    {
        fill(0, Qt::red);
        m_editor->addLayer();
        fill(1, Qt::blue);
        m_editor->moveLayer(1, 0);
        QCOMPARE(composite(5, 5), QColor(Qt::red));
        QCOMPARE(document().activeLayerIndex(), 0); // the moved layer stays active
        QCOMPARE(document().layer(0).name, QStringLiteral("Layer 2"));
    }

    void backgroundHidesAndChangesColour()
    {
        m_editor->setBackgroundColor(Qt::green);
        QCOMPARE(composite(5, 5), QColor(Qt::green));
        m_editor->setBackgroundVisible(false);
        QCOMPARE(composite(5, 5).alpha(), 0);
        m_editor->undo();
        m_editor->undo();
        QCOMPARE(composite(5, 5), QColor(Qt::white));
    }

    void switchingLayersPlacesAMovedSelection()
    {
        fill(0, Qt::red, QRect(0, 0, 8, 8));
        m_editor->addLayer();
        m_editor->setActiveLayer(0);
        m_settings->setTool(Tool::RectangleSelect);
        InteractiveTool *tool = m_editor->currentTool();
        tool->press({{0, 0}, Qt::LeftButton, {}});
        tool->release({{8, 8}, Qt::LeftButton, {}});
        tool->press({{4, 4}, Qt::LeftButton, {}});
        tool->move({{14, 4}, Qt::NoButton, {}});
        tool->release({{14, 4}, Qt::LeftButton, {}});
        QVERIFY(m_editor->selection()->isFloating());
        m_editor->setActiveLayer(1);
        QVERIFY(!m_editor->selection()->hasSelection());
        QCOMPARE(pixel(0, 12, 2), QColor(Qt::red));
        QCOMPARE(pixel(1, 12, 2).alpha(), 0);
    }

    void projectsRoundTrip()
    {
        fill(0, Qt::red, QRect(0, 0, 5, 5));
        m_editor->addLayer();
        fill(1, QColor(0, 0, 255, 128), QRect(5, 0, 5, 5));
        m_editor->addLayer();
        fill(2, Qt::yellow, QRect(10, 0, 5, 5));
        m_editor->setLayerVisible(2, false);
        m_editor->setActiveLayer(1);
        m_editor->setBackgroundColor(QColor(0x12, 0x34, 0x56));
        m_editor->setResolution(11811); // 300 DPI
        const QString path = m_dir.filePath(QStringLiteral("project.ora"));
        QCOMPARE(m_editor->save(path, projectFormat()), QString());
        QVERIFY(!m_editor->isModified());

        Editor reopened(m_settings.get());
        QCOMPARE(reopened.open(path), QString());
        const Document &loaded = reopened.document();
        QCOMPARE(loaded.size(), QSize(20, 10));
        QCOMPARE(loaded.layerCount(), 3);
        QCOMPARE(loaded.activeLayerIndex(), 1);
        QCOMPARE(loaded.dotsPerMeter(), 11811);
        QCOMPARE(loaded.backgroundColor(), QColor(0x12, 0x34, 0x56));
        QVERIFY(loaded.isBackgroundVisible());
        for (qsizetype i = 0; i < 3; ++i) {
            QCOMPARE(loaded.layer(i).name, document().layer(i).name);
            QCOMPARE(loaded.layer(i).visible, document().layer(i).visible);
            QCOMPARE(loaded.layer(i).image, document().layer(i).image);
        }
        // Other programs see the flattened picture too.
        QVERIFY(entry(path, QStringLiteral("mergedimage.png")).size() > 0);
        QCOMPARE(entry(path, QStringLiteral("mimetype")), QByteArray("image/openraster"));
    }

    void foreignProjectsAreFlattenedIntoLayers()
    {
        // A nested, half-transparent stack with an offset layer, and no background of ours.
        QImage tile(4, 4, QImage::Format_ARGB32);
        tile.fill(Qt::red);
        const QByteArray stack = R"(<?xml version="1.0"?>
<image w="10" h="8" xres="72">
  <stack>
    <stack opacity="0.5" x="2" y="1">
      <layer name="Inner" src="data/inner.png" x="3" y="2"/>
    </stack>
    <layer name="Hidden" src="data/inner.png" visibility="hidden"/>
  </stack>
</image>)";
        const QString path = m_dir.filePath(QStringLiteral("foreign.ora"));
        writeZip(path,
                 {{"mimetype", "image/openraster"}, {"stack.xml", stack}, {"data/inner.png", png(tile)}});

        QVERIFY(m_editor->open(path).isEmpty());
        QCOMPARE(document().size(), QSize(10, 8));
        QCOMPARE(document().layerCount(), 2);
        QVERIFY(!document().isBackgroundVisible());
        QCOMPARE(document().dotsPerMeter(), 2835); // 72 DPI
        QCOMPARE(document().layer(0).name, QStringLiteral("Hidden"));
        QVERIFY(!document().layer(0).visible);
        const QColor inner = pixel(1, 5, 3); // offset (2+3, 1+2)
        QCOMPARE(inner.red(), 255);
        QVERIFY(qAbs(inner.alpha() - 128) <= 1);
        QCOMPARE(pixel(1, 4, 3).alpha(), 0);
    }

    void brokenProjectsReportAnError()
    {
        const QString path = m_dir.filePath(QStringLiteral("broken.ora"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not a zip at all");
        file.close();
        QVERIFY(!m_editor->open(path).isEmpty());

        writeZip(path, {{"mimetype", "image/openraster"},
                        {"stack.xml", "<image w=\"5\" h=\"5\"><stack>"
                                      "<layer src=\"missing.png\"/></stack></image>"}});
        QVERIFY(!m_editor->open(path).isEmpty());
        writeZip(path, {{"mimetype", "image/openraster"}, {"stack.xml", "<image w=\"90000\" h=\"90000\"/>"}});
        QVERIFY(!m_editor->open(path).isEmpty());
        QCOMPARE(document().size(), QSize(20, 10)); // the open document is untouched
    }

private:
    const Document &document() const { return m_editor->document(); }

    void fill(qsizetype layer, const QColor &color, const QRect &area = {})
    {
        QImage &image = m_editor->document().layer(layer).image;
        QPainter painter(&image);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(area.isNull() ? image.rect() : area, color);
    }

    QColor pixel(qsizetype layer, int x, int y) const
    {
        return document().layer(layer).image.pixelColor(x, y);
    }
    QColor composite(int x, int y) const { return document().composite(QRect(x, y, 1, 1)).pixelColor(0, 0); }

    static QByteArray png(const QImage &image)
    {
        QByteArray data;
        QBuffer buffer(&data);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        return data;
    }

    static void writeZip(const QString &path, const QList<std::pair<QByteArray, QByteArray>> &entries)
    {
        QFile::remove(path);
        int error = 0;
        zip_t *archive = zip_open(QFile::encodeName(path).constData(), ZIP_CREATE, &error);
        QVERIFY(archive);
        for (const auto &[name, data] : entries) {
            zip_source_t *source = zip_source_buffer(archive, data.constData(), zip_uint64_t(data.size()), 0);
            QVERIFY(zip_file_add(archive, name.constData(), source, 0) >= 0);
        }
        QCOMPARE(zip_close(archive), 0);
    }

    static QByteArray entry(const QString &path, const QString &name)
    {
        int error = 0;
        zip_t *archive = zip_open(QFile::encodeName(path).constData(), ZIP_RDONLY, &error);
        if (!archive) {
            return {};
        }
        QByteArray data;
        zip_stat_t stat;
        if (zip_stat(archive, name.toUtf8().constData(), 0, &stat) == 0) {
            zip_file_t *file = zip_fopen(archive, name.toUtf8().constData(), 0);
            data.resize(qsizetype(stat.size));
            zip_fread(file, data.data(), stat.size);
            zip_fclose(file);
        }
        zip_discard(archive);
        return data;
    }

    QTemporaryDir m_dir;
    std::unique_ptr<ToolSettings> m_settings;
    std::unique_ptr<Editor> m_editor;
};

QTEST_MAIN(TestLayers)
#include "tst_layers.moc"
