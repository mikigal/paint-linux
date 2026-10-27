#include "tools/ToolSettings.h"

#include <QSignalSpy>
#include <QTest>

using namespace paint;

class TestToolSettings : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultsMatchPaint()
    {
        const ToolSettings settings;
        QCOMPARE(settings.tool(), Tool::Pencil);
        QCOMPARE(settings.color(ColorSlot::Primary), QColor(Qt::black));
        QCOMPARE(settings.color(ColorSlot::Secondary), QColor(Qt::white));
        QCOMPARE(settings.activeColorSlot(), ColorSlot::Primary);
        QCOMPARE(settings.size(), 1);
        QCOMPARE(settings.outlineStyle(), StrokeStyle::Solid);
        QCOMPARE(settings.fillStyle(), StrokeStyle::None);
        QCOMPARE(settings.customColors().size(), ToolSettings::CustomColorCount);
    }

    void sizesAndOpacityArePerTool()
    {
        ToolSettings settings;
        QSignalSpy sizeSpy(&settings, &ToolSettings::sizeChanged);
        settings.setSize(7);
        QCOMPARE(settings.size(), 7);
        settings.setTool(Tool::Brush);
        QCOMPARE(settings.size(), 5);
        settings.setOpacity(40);
        settings.setTool(Tool::Pencil);
        QCOMPARE(settings.size(), 7);
        QCOMPARE(settings.opacity(), 100);
        QCOMPARE(settings.opacityFor(Tool::Brush), 40);
        QVERIFY(sizeSpy.count() >= 3);
    }

    void slidersAreClampedAndIgnoredForToolsWithoutThem()
    {
        ToolSettings settings;
        settings.setSize(1000);
        QCOMPARE(settings.size(), ToolSettings::MaxSize);
        settings.setSize(-3);
        QCOMPARE(settings.size(), ToolSettings::MinSize);

        settings.setTool(Tool::Fill);
        settings.setSize(50);
        settings.setOpacity(10);
        QCOMPARE(settings.sizeFor(Tool::Fill), 1);
        settings.setFillTolerance(150);
        QCOMPARE(settings.fillTolerance(), 100);
    }

    void selectionToolIsRemembered()
    {
        ToolSettings settings;
        settings.setTool(Tool::FreeformSelect);
        settings.setTool(Tool::Pencil);
        QCOMPARE(settings.selectionTool(), Tool::FreeformSelect);
    }

    void colorsEmitOnlyOnChange()
    {
        ToolSettings settings;
        QSignalSpy spy(&settings, &ToolSettings::colorChanged);
        settings.setColor(ColorSlot::Primary, Qt::black);
        QCOMPARE(spy.count(), 0);
        settings.setColor(ColorSlot::Secondary, Qt::red);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<ColorSlot>(), ColorSlot::Secondary);
        settings.setColor(ColorSlot::Primary, QColor());
        QCOMPARE(settings.color(ColorSlot::Primary), QColor(Qt::black));
    }

    void textFormatIsClampedAndEmitsOnlyOnChange()
    {
        ToolSettings settings;
        QSignalSpy format(&settings, &ToolSettings::textFormatChanged);
        QSignalSpy alignment(&settings, &ToolSettings::textAlignmentChanged);
        TextFormat big = settings.textFormat();
        big.pointSize = 9000;
        settings.setTextFormat(big);
        QCOMPARE(settings.textFormat().pointSize, ToolSettings::MaxTextSize);
        settings.setTextFormat(settings.textFormat());
        QCOMPARE(format.count(), 1);
        // Only the horizontal part of an alignment is kept.
        settings.setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        QCOMPARE(settings.textAlignment(), Qt::AlignRight);
        settings.setTextAlignment(Qt::AlignRight);
        QCOMPARE(alignment.count(), 1);
    }

    void customColorsFillThenShift()
    {
        ToolSettings settings;
        for (int i = 0; i < ToolSettings::CustomColorCount; ++i) {
            settings.addCustomColor(QColor(i, 0, 0));
        }
        QCOMPARE(settings.customColors().first(), QColor(0, 0, 0));
        settings.addCustomColor(Qt::blue);
        QCOMPARE(settings.customColors().first(), QColor(1, 0, 0));
        QCOMPARE(settings.customColors().last(), QColor(Qt::blue));
    }
};

QTEST_GUILESS_MAIN(TestToolSettings)
#include "tst_toolsettings.moc"
