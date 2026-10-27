#pragma once

#include <QEvent>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPoint>

#include <functional>

class QLocalServer;
class QLocalSocket;
class QWidget;

// A line-based JSON control channel that lets UI scenarios and developer tooling
// (tools/paintctl.py) drive the running application: synthesise input, inspect
// widgets, take screenshots and exchange clipboard / drag-and-drop data.
// Input is injected through the QPA layer, so it follows the same delivery path
// as real events (popups, shortcuts, drag-and-drop handling).
//
// Protocol: one JSON object per line, {"cmd": "<name>", ...arguments}.
// Every request gets one reply line: {"ok": true, ...} or {"ok": false, "error": "..."}.
//
// Only compiled when PAINT_AUTOMATION is enabled; never part of release builds.
class AutomationServer : public QObject
{
    Q_OBJECT

public:
    using StateProvider = std::function<QJsonObject()>;

    explicit AutomationServer(QObject *parent = nullptr);
    ~AutomationServer() override;

    bool listen(const QString &socketPath);
    void setStateProvider(StateProvider provider) { m_stateProvider = std::move(provider); }

private:
    // A command validates its arguments and replies. Commands that inject input
    // put the injection into `deferred`: the reply is queued before it runs, so a
    // client still gets its answer when the input opens a modal dialog (whose
    // nested event loop then keeps serving further commands).
    struct Result
    {
        Result(QJsonObject r = {}, std::function<void()> d = {}) // NOLINT(google-explicit-constructor)
            : reply(std::move(r))
            , deferred(std::move(d))
        {}
        QJsonObject reply;
        std::function<void()> deferred;
    };
    using Command = Result (AutomationServer::*)(const QJsonObject &);

    void handleConnection();
    void handleData(QLocalSocket *socket);
    void processRequests(QLocalSocket *socket);
    Result execute(const QJsonObject &request);

    void sendMouse(QWidget *window, QPoint pos, QEvent::Type type, Qt::MouseButton button,
                   Qt::KeyboardModifiers modifiers);

    Result ping(const QJsonObject &args);
    Result help(const QJsonObject &args);
    Result tree(const QJsonObject &args);
    Result state(const QJsonObject &args);
    Result screenshot(const QJsonObject &args);
    Result mouse(const QJsonObject &args);
    Result click(const QJsonObject &args);
    Result drag(const QJsonObject &args);
    Result wheel(const QJsonObject &args);
    Result key(const QJsonObject &args);
    Result type(const QJsonObject &args);
    Result invoke(const QJsonObject &args);
    Result property(const QJsonObject &args);
    Result resize(const QJsonObject &args);
    Result wait(const QJsonObject &args);
    Result clipboard(const QJsonObject &args);
    Result drop(const QJsonObject &args);
    Result quit(const QJsonObject &args);

    struct CommandInfo
    {
        Command handler;
        QString summary;
    };

    QLocalServer *m_server = nullptr;
    StateProvider m_stateProvider;
    QHash<QString, CommandInfo> m_commands;
    Qt::MouseButtons m_pressedButtons;
    int m_timestamp = 0;
};
