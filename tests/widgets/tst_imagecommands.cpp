#include "ActionRegistry.h"
#include "MainWindow.h"
#include "canvas/CanvasView.h"
#include "dialogs/EditColorsDialog.h"
#include "dialogs/ImagePropertiesDialog.h"
#include "dialogs/ResizeSkewDialog.h"
#include "editor/Editor.h"
#include "ribbon/ColorsPanel.h"
#include "tools/ToolSettings.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
#include <QTest>
#include <QTimer>
#include <QToolButton>

using namespace paint;

// M4: image commands from the menus, the canvas handles and the dialogs' logic.
class TestImageCommands : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        QSettings().clear();
        m_window = std::make_unique<MainWindow>();
        m_window->resize(1280, 800);
        m_window->show();
        QVERIFY(QTest::qWaitForWindowActive(m_window.get()));
        QPainter(&editor()->document().layer(0).image).fillRect(QRect(0, 0, 4, 2), Qt::red);
    }

    void cleanup()
    {
        editor()->history().setClean();
        m_window.reset();
    }

    void rotateAndFlipFromTheMenus()
    {
        action(ActionId::RotateRight)->trigger();
        QCOMPARE(editor()->document().size(), QSize(648, 1152));
        QCOMPARE(statusText(QStringLiteral("status.canvasSize.text")), QStringLiteral("648 × 1152px"));
        QCOMPARE(pixel(647, 0), QColor(Qt::red));
        action(ActionId::FlipHorizontal)->trigger();
        QCOMPARE(pixel(0, 0), QColor(Qt::red));
        action(ActionId::InvertColors)->trigger();
        QCOMPARE(pixel(0, 0), QColor(Qt::cyan));
        QTest::keyClick(m_window.get(), Qt::Key_Z, Qt::ControlModifier);
        QTest::keyClick(m_window.get(), Qt::Key_Z, Qt::ControlModifier);
        QTest::keyClick(m_window.get(), Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(editor()->document().size(), QSize(1152, 648));
        QCOMPARE(pixel(0, 0), QColor(Qt::red));
    }

    void draggingTheCornerHandleResizesTheCanvas()
    {
        CanvasView *canvas = m_window->canvas();
        const QPointF corner = canvas->canvasRect().bottomRight() + QPointF(3, 3);
        QWidget *viewport = canvas->viewport();
        QTest::mousePress(viewport, Qt::LeftButton, {}, corner.toPoint());
        QTest::mouseMove(viewport, (corner + QPointF(-152, -48)).toPoint());
        QCOMPARE(statusText(QStringLiteral("status.canvasSize.text")), QStringLiteral("1000 × 600px"));
        QCOMPARE(editor()->document().size(), QSize(1152, 648)); // only a preview so far
        QTest::mouseRelease(viewport, Qt::LeftButton, {}, (corner + QPointF(-152, -48)).toPoint());
        QCOMPARE(editor()->document().size(), QSize(1000, 600));
    }

    void leftHandleAddsRoomOnTheLeft()
    {
        m_window->canvas()->setZoom(0.5);
        CanvasView *canvas = m_window->canvas();
        const QRectF rect = canvas->canvasRect();
        const QPointF left(rect.left() - 3, rect.center().y());
        QTest::mousePress(canvas->viewport(), Qt::LeftButton, {}, left.toPoint());
        QTest::mouseMove(canvas->viewport(), (left - QPointF(50, 0)).toPoint());
        QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, {}, (left - QPointF(50, 0)).toPoint());
        QCOMPARE(editor()->document().size(), QSize(1252, 648)); // 50 screen px at 50 % = 100 px
        QCOMPARE(pixel(100, 0), QColor(Qt::red));
    }

    void escapeCancelsAHandleDrag()
    {
        CanvasView *canvas = m_window->canvas();
        const QPointF corner = canvas->canvasRect().bottomRight() + QPointF(3, 3);
        QTest::mousePress(canvas->viewport(), Qt::LeftButton, {}, corner.toPoint());
        QTest::mouseMove(canvas->viewport(), (corner + QPointF(40, 40)).toPoint());
        QTest::keyClick(canvas, Qt::Key_Escape);
        QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, {}, (corner + QPointF(40, 40)).toPoint());
        QCOMPARE(editor()->document().size(), QSize(1152, 648));
        QCOMPARE(statusText(QStringLiteral("status.canvasSize.text")), QStringLiteral("1152 × 648px"));
    }

    void resizeSkewKeepsTheAspectRatio()
    {
        ResizeSkewDialog dialog(QSize(200, 100));
        auto *horizontal = dialog.findChild<QSpinBox *>(QStringLiteral("resizeSkew.horizontal"));
        auto *vertical = dialog.findChild<QSpinBox *>(QStringLiteral("resizeSkew.vertical"));
        horizontal->setValue(50);
        QCOMPARE(vertical->value(), 50);
        QCOMPARE(dialog.resultSize(), QSize(100, 50));

        dialog.findChild<QRadioButton *>(QStringLiteral("resizeSkew.pixels"))->setChecked(true);
        QCOMPARE(horizontal->value(), 100);
        QCOMPARE(vertical->value(), 50);
        horizontal->setValue(300);
        QCOMPARE(vertical->value(), 150);

        dialog.findChild<QCheckBox *>(QStringLiteral("resizeSkew.keepRatio"))->setChecked(false);
        vertical->setValue(10);
        QCOMPARE(dialog.resultSize(), QSize(300, 10));
        dialog.findChild<QSpinBox *>(QStringLiteral("resizeSkew.skewHorizontal"))->setValue(20);
        QCOMPARE(dialog.skewHorizontal(), 20);
        QVERIFY(dialog.changesAnything());
    }

    void imagePropertiesConvertsUnits()
    {
        ImagePropertiesDialog dialog({QSize(960, 480), 3780, {}, -1, QSize(1152, 648)});
        auto *width = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("imageProperties.width"));
        QCOMPARE(width->value(), 960.0);
        dialog.findChild<QRadioButton *>(QStringLiteral("imageProperties.inches"))->setChecked(true);
        QCOMPARE(width->value(), 10.0); // 960 px at 96 DPI
        dialog.findChild<QRadioButton *>(QStringLiteral("imageProperties.centimeters"))->setChecked(true);
        QCOMPARE(width->value(), 25.4);
        width->setValue(2.54);
        QCOMPARE(dialog.resultSize(), QSize(96, 480));
        dialog.findChild<QPushButton *>(QStringLiteral("imageProperties.default"))->click();
        QCOMPARE(dialog.resultSize(), QSize(1152, 648));
        QVERIFY(!dialog.blackAndWhite());
    }

    void editColorsKeepsAllViewsInSync()
    {
        EditColorsDialog dialog(QColor(255, 0, 0), {});
        auto *hex = dialog.findChild<QLineEdit *>(QStringLiteral("editColors.hex"));
        auto *red = dialog.findChild<QSpinBox *>(QStringLiteral("editColors.channel0"));
        auto *blue = dialog.findChild<QSpinBox *>(QStringLiteral("editColors.channel2"));
        QCOMPARE(hex->text(), QStringLiteral("#FF0000"));
        QCOMPARE(red->value(), 255);

        blue->setValue(255);
        QCOMPARE(dialog.color(), QColor(255, 0, 255));
        QCOMPARE(hex->text(), QStringLiteral("#FF00FF"));

        hex->clear();
        QTest::keyClicks(hex, QStringLiteral("#00ff00"));
        QCOMPARE(dialog.color(), QColor(0, 255, 0));
        QCOMPARE(red->value(), 0);

        dialog.findChild<QComboBox *>(QStringLiteral("editColors.mode"))->setCurrentIndex(1); // HSV
        QCOMPARE(red->value(), 120);                                                          // hue of green
        QCOMPARE(blue->value(), 100);

        QTest::mouseClick(dialog.findChild<ColorSwatch *>(QStringLiteral("editColors.basic.6")),
                          Qt::LeftButton);
        QCOMPARE(dialog.color(), EditColorsDialog::basicColors().at(6));
        dialog.findChild<QToolButton *>(QStringLiteral("editColors.addCustom"))->click();
        QCOMPARE(dialog.customColors().first(), EditColorsDialog::basicColors().at(6));
    }

    void editColorsSetsTheActiveColorAndRemembersIt()
    {
        QTimer::singleShot(0, this, [] {
            auto *dialog = qobject_cast<EditColorsDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->setColor(QColor(0x12, 0x34, 0x56));
            dialog->accept();
        });
        action(ActionId::EditColors)->trigger();
        ToolSettings *settings = m_window->toolSettings();
        QCOMPARE(settings->color(ColorSlot::Primary), QColor(0x12, 0x34, 0x56));
        QCOMPARE(settings->customColors().first(), QColor(0x12, 0x34, 0x56));

        // A new window (next session) gets the custom colour back.
        m_window->editor()->history().setClean();
        m_window = std::make_unique<MainWindow>();
        QCOMPARE(m_window->toolSettings()->customColors().first(), QColor(0x12, 0x34, 0x56));
    }

private:
    Editor *editor() const { return m_window->editor(); }
    QAction *action(ActionId id) const { return m_window->actionRegistry()->get(id); }
    QColor pixel(int x, int y) const { return editor()->document().layer(0).image.pixelColor(x, y); }
    QString statusText(const QString &name) const { return m_window->findChild<QLabel *>(name)->text(); }

    std::unique_ptr<MainWindow> m_window;
};

QTEST_MAIN(TestImageCommands)
#include "tst_imagecommands.moc"
