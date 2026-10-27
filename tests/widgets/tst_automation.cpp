#include "MainWindow.h"
#include "automation/AutomationServer.h"

#include <QApplication>
#include <QClipboard>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QMenuBar>
#include <QMimeData>
#include <QTemporaryDir>
#include <QTest>

#include <atomic>
#include <thread>

// End-to-end checks of the automation channel the UI scenarios rely on: a real socket,
// real QPA input injection, popups and modal dialogs.
class TestAutomation : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        m_window = std::make_unique<MainWindow>();
        m_window->show();
        QVERIFY(QTest::qWaitForWindowActive(m_window.get()));

        m_server = std::make_unique<AutomationServer>();
        m_server->setStateProvider([this] { return m_window->automationState(); });
        QVERIFY(m_server->listen(socketPath()));
    }

    void cleanupTestCase()
    {
        m_server.reset();
        m_window.reset();
    }

    void unknownCommandFails()
    {
        const QJsonObject reply = call({{QStringLiteral("cmd"), QStringLiteral("nope")}});
        QCOMPARE(reply.value(QStringLiteral("ok")).toBool(), false);
        QVERIFY(reply.value(QStringLiteral("error")).toString().contains(QLatin1String("unknown command")));
    }

    void stateIncludesEditorState()
    {
        const QJsonObject reply = call({{QStringLiteral("cmd"), QStringLiteral("state")}});
        QVERIFY(reply.value(QStringLiteral("ok")).toBool());
        QCOMPARE(reply.value(QStringLiteral("activeWindow")).toString(), QStringLiteral("mainWindow"));
        QCOMPARE(
            reply.value(QStringLiteral("app")).toObject().value(QStringLiteral("layers")).toArray().size(),
            1);
    }

    void clickOpensMenuAndEscapeClosesIt()
    {
        QVERIFY(ok(call({{QStringLiteral("cmd"), QStringLiteral("click")},
                         {QStringLiteral("target"), QStringLiteral("menu.file")}})));
        QTRY_VERIFY(QApplication::activePopupWidget());
        QCOMPARE(QApplication::activePopupWidget()->objectName(), QStringLiteral("menu.file"));

        const QString path = m_dir.filePath(QStringLiteral("menu.png"));
        const QJsonObject shot =
            call({{QStringLiteral("cmd"), QStringLiteral("screenshot")}, {QStringLiteral("path"), path}});
        QVERIFY(ok(shot));
        QCOMPARE(QImage(path).size(), m_window->size());

        QVERIFY(ok(call({{QStringLiteral("cmd"), QStringLiteral("key")},
                         {QStringLiteral("keys"), QStringLiteral("Escape")}})));
        QTRY_VERIFY(!QApplication::activePopupWidget());
    }

    // A modal dialog runs a nested event loop; the reply must still arrive and later
    // commands must be served from inside that loop.
    void modalDialogKeepsChannelResponsive()
    {
        const QList<QJsonObject> replies = callBatch({
            {{QStringLiteral("cmd"), QStringLiteral("invoke")},
             {QStringLiteral("action"), QStringLiteral("action.help.about")}},
            {{QStringLiteral("cmd"), QStringLiteral("state")}},
            {{QStringLiteral("cmd"), QStringLiteral("key")},
             {QStringLiteral("keys"), QStringLiteral("Escape")}},
        });
        QCOMPARE(replies.size(), 3);
        QVERIFY(ok(replies.at(0)));
        QCOMPARE(replies.at(1).value(QStringLiteral("activeModal")).toString(),
                 QStringLiteral("QMessageBox"));
        QVERIFY(ok(replies.at(2)));
        QTRY_VERIFY(!QApplication::activeModalWidget());
    }

    void clipboardRoundTrip()
    {
        QImage image(8, 6, QImage::Format_ARGB32);
        image.fill(Qt::magenta);
        const QString path = m_dir.filePath(QStringLiteral("clip.png"));
        QVERIFY(image.save(path));

        QVERIFY(ok(call({{QStringLiteral("cmd"), QStringLiteral("clipboard")},
                         {QStringLiteral("action"), QStringLiteral("set")},
                         {QStringLiteral("imagePath"), path}})));
        QCOMPARE(QGuiApplication::clipboard()->image().size(), QSize(8, 6));
        const QJsonObject reply = call({{QStringLiteral("cmd"), QStringLiteral("clipboard")}});
        QCOMPARE(reply.value(QStringLiteral("hasImage")).toBool(), true);
    }

    void dropOnRefusingWidgetIsReported()
    {
        const QJsonObject reply = call({{QStringLiteral("cmd"), QStringLiteral("drop")},
                                        {QStringLiteral("target"), QStringLiteral("workspace")},
                                        {QStringLiteral("text"), QStringLiteral("hello")}});
        QVERIFY(ok(reply));
        QCOMPARE(reply.value(QStringLiteral("accepted")).toBool(), false);
    }

private:
    static bool ok(const QJsonObject &reply)
    {
        if (!reply.value(QStringLiteral("ok")).toBool()) {
            qWarning("automation error: %s", qPrintable(reply.value(QStringLiteral("error")).toString()));
            return false;
        }
        return true;
    }

    QString socketPath() const { return m_dir.filePath(QStringLiteral("automation.sock")); }

    QJsonObject call(const QJsonObject &request) { return callBatch({request}).value(0); }

    // Like an external client: the requests are sent from another thread over a blocking socket
    // while this thread keeps running the event loop the server lives in. A modal dialog opened
    // by one request spins a nested loop here, so later requests must be served from inside it.
    QList<QJsonObject> callBatch(const QList<QJsonObject> &requests)
    {
        QList<QJsonObject> replies;
        std::atomic<bool> done = false;
        std::thread client([&] {
            QLocalSocket socket;
            socket.connectToServer(socketPath());
            if (socket.waitForConnected(2000)) {
                for (const QJsonObject &request : requests) {
                    socket.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
                    socket.waitForBytesWritten(2000);
                    while (!socket.canReadLine() && socket.waitForReadyRead(5000)) {
                    }
                    if (!socket.canReadLine()) {
                        break;
                    }
                    replies.append(QJsonDocument::fromJson(socket.readLine()).object());
                }
            }
            done = true;
        });
        if (!QTest::qWaitFor([&] { return done.load(); }, 20000)) {
            qWarning("automation client did not finish");
        }
        client.join();
        return replies;
    }

    QTemporaryDir m_dir;
    std::unique_ptr<MainWindow> m_window;
    std::unique_ptr<AutomationServer> m_server;
};

QTEST_MAIN(TestAutomation)
#include "tst_automation.moc"
