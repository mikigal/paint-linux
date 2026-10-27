#include "MainWindow.h"
#include "PaintConfig.h"

#include <QApplication>
#include <QDir>
#include <QJsonArray>
#include <QMenuBar>
#include <QStyle>
#include <QTest>

class TestMainWindow : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { QApplication::setApplicationDisplayName(QStringLiteral(PAINT_APP_NAME)); }

    // Guards the deterministic test environment: if this fails, screenshots taken by
    // other tests would depend on the machine instead of the checked-in KDE settings.
    void usesDeterministicBreezeEnvironment()
    {
        QCOMPARE(QApplication::style()->name(), QStringLiteral("breeze"));
#ifdef PAINT_TEST_DARK_THEME
        const QColor window(32, 35, 38);
#else
        const QColor window(239, 240, 241);
#endif
        QCOMPARE(QApplication::palette().color(QPalette::Window), window);
        QCOMPARE(QApplication::font().family(), QStringLiteral("Noto Sans"));
    }

    void showsUntitledDocument()
    {
        MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        // The platform plugin appends the display name ("Untitled — Paint for Linux").
        QCOMPARE(window.windowTitle(), QStringLiteral("Untitled[*]"));
        QCOMPARE(QGuiApplication::applicationDisplayName(), QStringLiteral("Paint for Linux"));
        QVERIFY(!window.isWindowModified());
    }

    void hasNativeMenuBar()
    {
        MainWindow window;
        QStringList menus;
        for (QAction *action : window.menuBar()->actions()) {
            menus.append(action->menu()->objectName());
        }
        QCOMPARE(menus, (QStringList{QStringLiteral("menu.file"), QStringLiteral("menu.edit"),
                                     QStringLiteral("menu.image"), QStringLiteral("menu.view"),
                                     QStringLiteral("menu.help")}));
        QVERIFY(window.findChild<QAction *>(QStringLiteral("action.file.quit")));
        QVERIFY(window.findChild<QAction *>(QStringLiteral("action.help.about")));
    }

    void reportsAutomationState()
    {
        MainWindow window;
        const QJsonObject state = window.automationState();
        QCOMPARE(state.value(QStringLiteral("layers")).toArray().size(), 1);
        QCOMPARE(state.value(QStringLiteral("backgroundColor")).toString(), QStringLiteral("#ffffffff"));
    }
};

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
