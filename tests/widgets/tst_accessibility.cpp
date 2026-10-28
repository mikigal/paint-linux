#include "MainWindow.h"
#include "tools/ToolSettings.h"

#include <QAbstractButton>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QAccessible>
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QScrollBar>
#include <QSet>
#include <QTest>

// Screen readers and keyboard users reach every control: each one has a name, and Tab moves
// through the window's controls.
class TestAccessibility : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        m_window = std::make_unique<MainWindow>();
        m_window->resize(1280, 800);
        m_window->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_window.get()));
    }

    void cleanup() { m_window.reset(); }

    void everyControlHasAName()
    {
        QStringList unnamed;
        for (QWidget *widget : m_window->findChildren<QWidget *>()) {
            const bool control =
                qobject_cast<QAbstractButton *>(widget) || qobject_cast<QAbstractSlider *>(widget) ||
                qobject_cast<QAbstractSpinBox *>(widget) || qobject_cast<QComboBox *>(widget) ||
                (widget->focusPolicy() & Qt::TabFocus && !qobject_cast<QLineEdit *>(widget));
            // Scroll bars are known by their role.
            if (!control || qobject_cast<QScrollBar *>(widget) || !widget->isVisibleTo(m_window.get())) {
                continue;
            }
            QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(widget);
            if (!accessible || accessible->text(QAccessible::Name).trimmed().isEmpty()) {
                unnamed.append(QStringLiteral("%1 (%2)").arg(
                    widget->objectName(), QString::fromLatin1(widget->metaObject()->className())));
            }
        }
        QVERIFY2(unnamed.isEmpty(), qPrintable(unnamed.join(QStringLiteral(", "))));
    }

    void tabReachesTheRibbonPaletteAndZoom()
    {
        QWidget *canvas = m_window->findChild<QWidget *>(QStringLiteral("canvasView"));
        canvas->setFocus();
        QSet<QString> reached;
        for (int i = 0; i < 300; ++i) {
            QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
            QWidget *focus = QApplication::focusWidget();
            QVERIFY(focus);
            reached.insert(focus->objectName());
            if (focus == canvas) {
                break; // round the whole window
            }
        }
        const QStringList names = reached.values();
        for (const QString &name :
             {QStringLiteral("canvasView"), QStringLiteral("ribbon.select"), QStringLiteral("ribbon.color1"),
              QStringLiteral("ribbon.palette.3"), QStringLiteral("status.zoom.slider")}) {
            QVERIFY2(reached.contains(name),
                     qPrintable(name + QStringLiteral(" not in ") + names.join(u' ')));
        }

        // Space picks a palette colour, as a click does.
        QWidget *red = m_window->findChild<QWidget *>(QStringLiteral("ribbon.palette.3"));
        red->setFocus();
        QTest::keyClick(red, Qt::Key_Space);
        QCOMPARE(m_window->toolSettings()->color(paint::ColorSlot::Primary), QColor(0xed1c24));
    }

private:
    std::unique_ptr<MainWindow> m_window;
};

QTEST_MAIN(TestAccessibility)
#include "tst_accessibility.moc"
