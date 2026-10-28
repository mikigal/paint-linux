#include "ActionRegistry.h"
#include "MainWindow.h"
#include "editor/Editor.h"
#include "platform/Printing.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QFile>
#include <QMenu>
#include <QMessageBox>
#include <QPrinter>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

using namespace paint;

// Files and the platform around them: recent files, colour-reducing formats, printing.
class TestFiles : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        QVERIFY(m_dir.isValid());
        QSettings().remove(QStringLiteral("recentFiles"));
        m_window = std::make_unique<MainWindow>();
        m_window->resize(1000, 700);
        m_window->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_window.get()));
    }

    void cleanup()
    {
        m_window->editor()->history().setClean();
        m_window.reset();
    }

    void recentFilesListWhatWasOpenedAndSaved()
    {
        QMenu *recent = m_window->findChild<QMenu *>(QStringLiteral("menu.recent"));
        QCOMPARE(recent->actions().size(), 1);
        QVERIFY(!recent->actions().first()->isEnabled()); // "No recent images"

        const QString first = m_dir.filePath(QStringLiteral("first & best.png"));
        const QString second = m_dir.filePath(QStringLiteral("second.png"));
        QVERIFY(m_window->saveFile(first));
        QVERIFY(m_window->saveFile(second));
        QVERIFY(m_window->openFile(first));
        // Newest first, numbered, with ampersands kept literal.
        QCOMPARE(recent->actions().at(0)->text(), QStringLiteral("&1 first && best.png"));
        QCOMPARE(recent->actions().at(1)->text(), QStringLiteral("&2 second.png"));
        QCOMPARE(recent->actions().at(0)->toolTip(), first);

        recent->actions().at(1)->trigger();
        QCOMPARE(m_window->editor()->filePath(), second);
        // Files that are gone drop off the list.
        QFile::remove(first);
        m_window->findChild<QAction *>(QStringLiteral("action.recent.clear"))->trigger();
        QCOMPARE(recent->actions().size(), 1);
        QVERIFY(!recent->actions().first()->isEnabled());
    }

    void colourReducingFormatsAskFirst()
    {
        const QString path = m_dir.filePath(QStringLiteral("picture.gif"));
        answerColorLoss(QMessageBox::RejectRole);
        QVERIFY(!m_window->saveFile(path));
        QVERIFY(!QFile::exists(path));
        answerColorLoss(QMessageBox::AcceptRole);
        QVERIFY(m_window->saveFile(path));
        QVERIFY(QFile::exists(path));
        // PNG keeps every colour and does not ask.
        QVERIFY(m_window->saveFile(m_dir.filePath(QStringLiteral("picture.png"))));
    }

    void printsAtTheirOwnSizeShrunkToFit()
    {
        // 96 DPI pictures on a 600 DPI page of 4800 × 6000 printable pixels.
        QCOMPARE(printArea(QSize(96, 48), 3780, QRectF(0, 0, 4800, 6000), 600), QRectF(0, 0, 600, 300));
        QCOMPARE(printArea(QSize(960, 480), 3780, QRectF(0, 0, 4800, 6000), 600), QRectF(0, 0, 4800, 2400));
        // A 300 DPI picture prints smaller than a 96 DPI one with the same pixels.
        QCOMPARE(printArea(QSize(300, 300), 11811, QRectF(10, 10, 4800, 6000), 600),
                 QRectF(10, 10, 600, 600));

        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        const QString pdf = m_dir.filePath(QStringLiteral("print.pdf"));
        printer.setOutputFileName(pdf);
        QImage picture(200, 100, QImage::Format_ARGB32);
        picture.fill(Qt::red);
        QVERIFY(printPicture(&printer, picture, 3780));
        QFile file(pdf);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.read(4) == "%PDF");
    }

private:
    void answerColorLoss(QMessageBox::ButtonRole answer)
    {
        QTimer::singleShot(0, this, [answer] {
            // The platform theme may show its own copy of the message box, so it is known by its text.
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY2(box && box->text().contains(QLatin1String("reduce the colours")),
                     "expected the colour warning");
            for (QAbstractButton *button : box->buttons()) {
                if (box->buttonRole(button) == answer) {
                    button->click();
                    return;
                }
            }
            QFAIL("the warning has no such button");
        });
    }

    QTemporaryDir m_dir;
    std::unique_ptr<MainWindow> m_window;
};

QTEST_MAIN(TestFiles)
#include "tst_files.moc"
