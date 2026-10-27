#include "ActionRegistry.h"
#include "MainWindow.h"
#include "canvas/SidePanel.h"
#include "ribbon/ColorsPanel.h"
#include "ribbon/Ribbon.h"
#include "ribbon/RibbonButton.h"
#include "ribbon/RibbonHost.h"
#include "tools/ToolSettings.h"

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QSlider>
#include <QTest>
#include <QTimer>

using namespace paint;

class TestRibbon : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        m_window = std::make_unique<MainWindow>();
        m_window->resize(1600, 900);
        m_window->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_window.get()));
    }

    void cleanup() { m_window.reset(); }

    void toolButtonsSelectTools()
    {
        click(QStringLiteral("ribbon.fill"));
        QCOMPARE(settings()->tool(), Tool::Fill);
        QVERIFY(m_window->actionRegistry()->toolAction(Tool::Fill)->isChecked());
        QVERIFY(!m_window->actionRegistry()->toolAction(Tool::Pencil)->isChecked());
        QVERIFY(widget<RibbonButton>(QStringLiteral("ribbon.fill"))->isChecked());
    }

    void selectionButtonFollowsLastSelectionTool()
    {
        auto *select = widget<RibbonButton>(QStringLiteral("ribbon.select"));
        QTest::mouseClick(select, Qt::LeftButton, {}, QPoint(select->width() / 2, 20));
        QCOMPARE(settings()->tool(), Tool::RectangleSelect);

        m_window->actionRegistry()->toolAction(Tool::FreeformSelect)->trigger();
        QCOMPARE(select->defaultAction(), m_window->actionRegistry()->toolAction(Tool::FreeformSelect));
        QVERIFY2(select->menu(), "switching the default action must keep the drop-down menu");

        settings()->setTool(Tool::Pencil);
        QTest::mouseClick(select, Qt::LeftButton, {}, QPoint(select->width() / 2, 20));
        QCOMPARE(settings()->tool(), Tool::FreeformSelect);
    }

    void selectionChevronOpensMenu()
    {
        auto *select = widget<RibbonButton>(QStringLiteral("ribbon.select"));
        QStringList seen;
        QTimer::singleShot(0, this, [&seen] {
            if (auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget())) {
                for (QAction *action : menu->actions()) {
                    if (!action->isSeparator()) {
                        seen.append(action->objectName());
                    }
                }
                menu->close();
            }
        });
        QTest::mouseClick(select, Qt::LeftButton, {}, QPoint(select->width() / 2, select->height() - 18));
        QCOMPARE(seen, (QStringList{
                           QStringLiteral("action.tool.rectangleSelect"),
                           QStringLiteral("action.tool.freeformSelect"),
                           QStringLiteral("action.edit.selectAll"),
                           QStringLiteral("action.edit.invertSelection"),
                           QStringLiteral("action.edit.transparentSelection"),
                           QStringLiteral("action.edit.delete"),
                       }));
        QCOMPARE(settings()->tool(), Tool::Pencil);
    }

    void brushesAndShapesSelectTheirTools()
    {
        ActionRegistry *actions = m_window->actionRegistry();
        actions->brushAction(BrushType::Airbrush)->trigger();
        QCOMPARE(settings()->tool(), Tool::Brush);
        QCOMPARE(settings()->brushType(), BrushType::Airbrush);

        click(QStringLiteral("ribbon.shape.oval"));
        QCOMPARE(settings()->tool(), Tool::Shape);
        QCOMPARE(settings()->shapeType(), ShapeType::Oval);
        QVERIFY(actions->shapeAction(ShapeType::Oval)->isChecked());
        QVERIFY(widget<QWidget>(QStringLiteral("ribbon.outline"))->isEnabled());
        QVERIFY(actions->fillAction(StrokeStyle::Crayon)->isEnabled());

        // Leaving the shape tool clears the gallery highlight and disables the shape options.
        click(QStringLiteral("ribbon.pencil"));
        QVERIFY(!actions->shapeAction(ShapeType::Oval)->isChecked());
        QVERIFY(!widget<QWidget>(QStringLiteral("ribbon.outline"))->isEnabled());
        QVERIFY(!actions->fillAction(StrokeStyle::Crayon)->isEnabled());
    }

    void paletteSetsColors()
    {
        click(QStringLiteral("ribbon.palette.3"));
        QCOMPARE(settings()->color(ColorSlot::Primary), QColor(0xed1c24));
        auto *green = widget<ColorSwatch>(QStringLiteral("ribbon.palette.6"));
        QTest::mouseClick(green, Qt::RightButton);
        QCOMPARE(settings()->color(ColorSlot::Secondary), QColor(0x22b14c));

        click(QStringLiteral("ribbon.color2"));
        QCOMPARE(settings()->activeColorSlot(), ColorSlot::Secondary);
        click(QStringLiteral("ribbon.palette.0"));
        QCOMPARE(settings()->color(ColorSlot::Secondary), QColor(Qt::black));
        QCOMPARE(settings()->color(ColorSlot::Primary), QColor(0xed1c24));

        // Empty custom slots do nothing.
        click(QStringLiteral("ribbon.customColor.0"));
        QCOMPARE(settings()->color(ColorSlot::Secondary), QColor(Qt::black));
    }

    void groupsCollapseInPaintOrder()
    {
        const QStringList order{QStringLiteral("Shapes"), QStringLiteral("Image"),
                                QStringLiteral("Quick access"), QStringLiteral("Colors")};
        Ribbon *ribbon = m_window->ribbonHost()->ribbon();
        QCOMPARE(compactGroups(ribbon), QStringList());

        int collapsed = 0;
        for (int width = 1600; width >= m_window->minimumWidth(); width -= 20) {
            m_window->resize(width, 900);
            QCoreApplication::processEvents();
            const QStringList compact = compactGroups(ribbon);
            QVERIFY(compact.size() >= collapsed);
            collapsed = int(compact.size());
            // Whatever is collapsed is always a prefix of Paint's order.
            QCOMPARE(QSet<QString>(compact.begin(), compact.end()),
                     QSet<QString>(order.begin(), order.begin() + collapsed));
        }
        QCOMPARE(collapsed, order.size());
        QVERIFY(ribbon->minimumSizeHint().width() <= m_window->minimumWidth());
    }

    void sidePanelFollowsTool()
    {
        auto *panel = widget<SidePanel>(QStringLiteral("sidePanel"));
        auto *size = widget<QWidget>(QStringLiteral("sidePanel.size"));
        auto *second = widget<QWidget>(QStringLiteral("sidePanel.opacity"));
        QVERIFY(panel->isVisible() && size->isVisible() && second->isVisible());

        settings()->setTool(Tool::Fill);
        QVERIFY(!size->isVisible());
        QVERIFY(second->isVisible());
        settings()->setTool(Tool::Text);
        QVERIFY(!panel->isVisible());
        settings()->setTool(Tool::Shape);
        QVERIFY(size->isVisible() && !second->isVisible());

        auto *slider = widget<QSlider>(QStringLiteral("sidePanel.size.slider"));
        slider->setValue(12);
        QCOMPARE(settings()->size(), 12);
        settings()->setSize(30);
        QCOMPARE(slider->value(), 30);
    }

    void autoHideShowsRibbonOnDemand()
    {
        RibbonHost *host = m_window->ribbonHost();
        m_window->actionRegistry()->get(ActionId::ToolbarAutoHide)->trigger();
        QVERIFY(host->autoHide());
        QVERIFY(!host->ribbon()->isVisible());

        click(QStringLiteral("ribbon.showToolbar"));
        QTRY_VERIFY(host->isRibbonPopupVisible());
        QVERIFY(host->ribbon()->isVisible());

        // Running a command puts the ribbon away again.
        m_window->actionRegistry()->toolAction(Tool::Eraser)->trigger();
        QTRY_VERIFY(!host->isRibbonPopupVisible());

        m_window->actionRegistry()->get(ActionId::ToolbarAlwaysShow)->trigger();
        QVERIFY(!host->autoHide());
        QTRY_VERIFY(host->ribbon()->isVisible());
    }

private:
    ToolSettings *settings() const { return m_window->toolSettings(); }

    template<typename T>
    T *widget(const QString &name) const
    {
        const QList<T *> found = m_window->findChildren<T *>(name);
        for (T *candidate : found) {
            if (candidate->isVisible()) {
                return candidate;
            }
        }
        if (found.isEmpty()) {
            qFatal("no widget %s", qPrintable(name));
        }
        return found.first();
    }

    void click(const QString &name) const { QTest::mouseClick(widget<QWidget>(name), Qt::LeftButton); }

    static QStringList compactGroups(Ribbon *ribbon)
    {
        QStringList result;
        for (RibbonGroup *group : ribbon->groups()) {
            if (group->isCompact()) {
                result.append(group->title());
            }
        }
        return result;
    }

    std::unique_ptr<MainWindow> m_window;
};

QTEST_MAIN(TestRibbon)
#include "tst_ribbon.moc"
