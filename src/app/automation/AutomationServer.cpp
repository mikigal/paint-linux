#include "automation/AutomationServer.h"

#include <QAbstractButton>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QStyle>
#include <QTest>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QWindow>
#include <qpa/qplatformdrag.h>
#include <qpa/qwindowsysteminterface.h>

namespace {

struct CommandError : std::exception
{
    explicit CommandError(QString text)
        : message(std::move(text))
        , utf8(message.toUtf8())
    {}
    const char *what() const noexcept override { return utf8.constData(); }

    QString message;
    QByteArray utf8;
};

[[noreturn]] void fail(const QString &message)
{
    throw CommandError(message);
}

QString requireString(const QJsonObject &args, const QString &key)
{
    const QJsonValue value = args.value(key);
    if (!value.isString() || value.toString().isEmpty()) {
        fail(QStringLiteral("missing string argument '%1'").arg(key));
    }
    return value.toString();
}

QPoint toPoint(const QJsonValue &value, const QString &what)
{
    const QJsonArray array = value.toArray();
    if (array.size() != 2) {
        fail(QStringLiteral("'%1' must be [x, y]").arg(what));
    }
    return QPoint(array.at(0).toInt(), array.at(1).toInt());
}

QJsonArray toJson(const QRect &rect)
{
    return QJsonArray{rect.x(), rect.y(), rect.width(), rect.height()};
}

QJsonValue variantToJson(const QVariant &value)
{
    switch (value.metaType().id()) {
    case QMetaType::QSize:
        return QJsonArray{value.toSize().width(), value.toSize().height()};
    case QMetaType::QPoint:
        return QJsonArray{value.toPoint().x(), value.toPoint().y()};
    case QMetaType::QRect:
        return toJson(value.toRect());
    case QMetaType::QColor:
        return value.value<QColor>().name(QColor::HexArgb);
    case QMetaType::QKeySequence:
        return value.value<QKeySequence>().toString(QKeySequence::PortableText);
    default:
        return QJsonValue::fromVariant(value);
    }
}

QMainWindow *mainWindow()
{
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        if (auto *window = qobject_cast<QMainWindow *>(widget); window && window->isVisible()) {
            return window;
        }
    }
    fail(QStringLiteral("no visible main window"));
}

// Finds a widget by object name, preferring visible ones.
QWidget *findWidget(const QString &name)
{
    QWidget *hidden = nullptr;
    for (QWidget *topLevel : QApplication::topLevelWidgets()) {
        QList<QWidget *> candidates = topLevel->findChildren<QWidget *>(name);
        if (topLevel->objectName() == name) {
            candidates.prepend(topLevel);
        }
        for (QWidget *candidate : std::as_const(candidates)) {
            if (candidate->isVisible()) {
                return candidate;
            }
            if (!hidden) {
                hidden = candidate;
            }
        }
    }
    return hidden;
}

QAction *findAction(const QString &name)
{
    for (QWidget *topLevel : QApplication::topLevelWidgets()) {
        if (auto *action = topLevel->findChild<QAction *>(name)) {
            return action;
        }
    }
    return nullptr;
}

QObject *findObject(const QString &name)
{
    if (QWidget *widget = findWidget(name)) {
        return widget;
    }
    if (QAction *action = findAction(name)) {
        return action;
    }
    fail(QStringLiteral("no widget or action named '%1'").arg(name));
}

struct WindowPoint
{
    QWidget *window = nullptr; // a top-level widget
    QPoint pos;                // in that window's coordinates
};

// Locates the on-screen position of an action inside a visible menu, menu bar or tool bar.
WindowPoint locateAction(QAction *action)
{
    for (QObject *object : action->associatedObjects()) {
        auto *widget = qobject_cast<QWidget *>(object);
        if (!widget || !widget->isVisible()) {
            continue;
        }
        QRect rect;
        if (auto *menu = qobject_cast<QMenu *>(widget)) {
            rect = menu->actionGeometry(action);
        } else if (auto *menuBar = qobject_cast<QMenuBar *>(widget)) {
            rect = menuBar->actionGeometry(action);
        } else if (auto *toolBar = qobject_cast<QToolBar *>(widget)) {
            if (QWidget *button = toolBar->widgetForAction(action)) {
                rect = button->geometry();
            }
        }
        if (rect.isValid()) {
            QWidget *window = widget->window();
            return {window, widget->mapTo(window, rect.center())};
        }
    }
    fail(QStringLiteral("action '%1' is not visible on screen; use 'invoke' instead")
             .arg(action->objectName()));
}

// Resolves where input should go: {"target": name, "at": [x, y]} (widget-local, defaults to its
// centre) or {"x": .., "y": .., "window": name} (window coordinates, defaults to the main window).
WindowPoint resolvePoint(const QJsonObject &args, const QString &targetKey = QStringLiteral("target"),
                         const QString &pointKey = QString())
{
    if (!pointKey.isEmpty() && args.contains(pointKey)) {
        WindowPoint point;
        point.window = mainWindow();
        point.pos = toPoint(args.value(pointKey), pointKey);
        if (args.contains(QStringLiteral("relativeTo"))) {
            QWidget *origin = findWidget(args.value(QStringLiteral("relativeTo")).toString());
            if (!origin) {
                fail(QStringLiteral("unknown relativeTo widget"));
            }
            point.window = origin->window();
            point.pos = origin->mapTo(point.window, point.pos);
        }
        return point;
    }

    if (args.contains(targetKey)) {
        const QString name = args.value(targetKey).toString();
        QWidget *widget = findWidget(name);
        if (auto *menu = qobject_cast<QMenu *>(widget); menu && !menu->isVisible()) {
            // A closed menu is reached through its entry in the parent menu bar or menu.
            return locateAction(menu->menuAction());
        }
        if (widget) {
            if (!widget->isVisible()) {
                fail(QStringLiteral("widget '%1' is not visible").arg(name));
            }
            QWidget *window = widget->window();
            const QPoint local = args.contains(QStringLiteral("at"))
                                     ? toPoint(args.value(QStringLiteral("at")), QStringLiteral("at"))
                                     : widget->rect().center();
            return {window, widget->mapTo(window, local)};
        }
        if (QAction *action = findAction(name)) {
            return locateAction(action);
        }
        fail(QStringLiteral("no widget or action named '%1'").arg(name));
    }

    if (args.contains(QStringLiteral("x")) && args.contains(QStringLiteral("y"))) {
        QWidget *window = mainWindow();
        if (args.contains(QStringLiteral("window"))) {
            QWidget *named = findWidget(args.value(QStringLiteral("window")).toString());
            if (!named) {
                fail(QStringLiteral("unknown window"));
            }
            window = named->window();
        }
        return {window,
                QPoint(args.value(QStringLiteral("x")).toInt(), args.value(QStringLiteral("y")).toInt())};
    }
    fail(QStringLiteral("expected 'target' or 'x'/'y'"));
}

Qt::MouseButton parseButton(const QJsonObject &args)
{
    const QString name = args.value(QStringLiteral("button")).toString(QStringLiteral("left"));
    if (name == QLatin1String("left")) {
        return Qt::LeftButton;
    }
    if (name == QLatin1String("right")) {
        return Qt::RightButton;
    }
    if (name == QLatin1String("middle")) {
        return Qt::MiddleButton;
    }
    fail(QStringLiteral("unknown button '%1'").arg(name));
}

Qt::KeyboardModifiers parseModifiers(const QJsonObject &args)
{
    const QJsonValue value = args.value(QStringLiteral("modifiers"));
    QStringList names;
    if (value.isArray()) {
        for (const auto &item : value.toArray()) {
            names.append(item.toString());
        }
    } else if (value.isString()) {
        names = value.toString().split(QLatin1Char('+'), Qt::SkipEmptyParts);
    }
    Qt::KeyboardModifiers modifiers;
    for (const QString &raw : std::as_const(names)) {
        const QString name = raw.trimmed().toLower();
        if (name == QLatin1String("ctrl") || name == QLatin1String("control")) {
            modifiers |= Qt::ControlModifier;
        } else if (name == QLatin1String("shift")) {
            modifiers |= Qt::ShiftModifier;
        } else if (name == QLatin1String("alt")) {
            modifiers |= Qt::AltModifier;
        } else if (name == QLatin1String("meta")) {
            modifiers |= Qt::MetaModifier;
        } else {
            fail(QStringLiteral("unknown modifier '%1'").arg(raw));
        }
    }
    return modifiers;
}

QWindow *keyboardWindow()
{
    if (QWidget *popup = QApplication::activePopupWidget()) {
        return popup->windowHandle();
    }
    if (QWindow *focus = QGuiApplication::focusWindow()) {
        return focus;
    }
    return mainWindow()->windowHandle();
}

QJsonArray describeActions(const QList<QAction *> &actions)
{
    QJsonArray result;
    for (QAction *action : actions) {
        QJsonObject item{{QStringLiteral("name"), action->objectName()},
                         {QStringLiteral("text"), action->text()},
                         {QStringLiteral("enabled"), action->isEnabled()},
                         {QStringLiteral("visible"), action->isVisible()}};
        if (action->isSeparator()) {
            item.insert(QStringLiteral("separator"), true);
        }
        if (action->isCheckable()) {
            item.insert(QStringLiteral("checked"), action->isChecked());
        }
        if (!action->shortcut().isEmpty()) {
            item.insert(QStringLiteral("shortcut"), action->shortcut().toString(QKeySequence::PortableText));
        }
        if (QMenu *submenu = action->menu()) {
            item.insert(QStringLiteral("menu"), submenu->objectName());
        }
        result.append(item);
    }
    return result;
}

QJsonObject describeWidget(QWidget *widget, QWidget *window, bool includeHidden)
{
    QJsonObject node{
        {QStringLiteral("class"), QString::fromLatin1(widget->metaObject()->className())},
        {QStringLiteral("name"), widget->objectName()},
        {QStringLiteral("geometry"), toJson(QRect(widget->mapTo(window, QPoint(0, 0)), widget->size()))},
        {QStringLiteral("visible"), widget->isVisible()},
        {QStringLiteral("enabled"), widget->isEnabled()},
    };
    if (!widget->toolTip().isEmpty()) {
        node.insert(QStringLiteral("toolTip"), widget->toolTip());
    }
    if (auto *button = qobject_cast<QAbstractButton *>(widget)) {
        node.insert(QStringLiteral("text"), button->text());
        if (button->isCheckable()) {
            node.insert(QStringLiteral("checked"), button->isChecked());
        }
    } else if (auto *label = qobject_cast<QLabel *>(widget)) {
        node.insert(QStringLiteral("text"), label->text());
    } else if (auto *edit = qobject_cast<QLineEdit *>(widget)) {
        node.insert(QStringLiteral("text"), edit->text());
    } else if (auto *spin = qobject_cast<QAbstractSpinBox *>(widget)) {
        node.insert(QStringLiteral("text"), spin->text());
    } else if (auto *slider = qobject_cast<QAbstractSlider *>(widget)) {
        node.insert(QStringLiteral("value"), slider->value());
        node.insert(QStringLiteral("range"), QJsonArray{slider->minimum(), slider->maximum()});
    } else if (auto *combo = qobject_cast<QComboBox *>(widget)) {
        node.insert(QStringLiteral("text"), combo->currentText());
    }
    if (qobject_cast<QMenu *>(widget) || qobject_cast<QMenuBar *>(widget) ||
        qobject_cast<QToolBar *>(widget)) {
        node.insert(QStringLiteral("actions"), describeActions(widget->actions()));
    }

    QJsonArray children;
    for (QWidget *child : widget->findChildren<QWidget *>(Qt::FindDirectChildrenOnly)) {
        if (child->isWindow() || (!includeHidden && !child->isVisible())) {
            continue;
        }
        children.append(describeWidget(child, window, includeHidden));
    }
    if (!children.isEmpty()) {
        node.insert(QStringLiteral("children"), children);
    }
    return node;
}

void settle()
{
    QCoreApplication::sendPostedEvents();
    QCoreApplication::processEvents();
}

// Pastes visible popups (menus, combo box lists, tool tips) that belong to `window` onto its grab,
// so a screenshot shows what a user would see.
QImage compositeWithPopups(QWidget *window)
{
    const QPixmap base = window->grab();
    const QPoint origin = window->geometry().topLeft();
    QRect bounds(QPoint(0, 0), base.deviceIndependentSize().toSize());

    QList<QWidget *> popups;
    for (QWidget *topLevel : QApplication::topLevelWidgets()) {
        const Qt::WindowType type = topLevel->windowType();
        if (topLevel != window && topLevel->isVisible() && (type == Qt::Popup || type == Qt::ToolTip)) {
            popups.append(topLevel);
        }
    }
    // The active popup is the innermost one, so it goes on top.
    if (QWidget *active = QApplication::activePopupWidget(); popups.removeOne(active)) {
        popups.append(active);
    }
    for (QWidget *popup : std::as_const(popups)) {
        bounds |= QRect(popup->geometry().topLeft() - origin, popup->size());
    }

    QImage image(bounds.size() * base.devicePixelRatio(), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(base.devicePixelRatio());
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.translate(-bounds.topLeft());
    painter.drawPixmap(QPoint(0, 0), base);
    for (QWidget *popup : std::as_const(popups)) {
        painter.drawPixmap(popup->geometry().topLeft() - origin, popup->grab());
    }
    return image;
}

// Builds drag-and-drop or clipboard payloads: {"urls": [...]}, {"imagePath": file},
// {"text": ..}, {"html": ..} and raw {"mime": {"type": "<base64>"}}.
QMimeData *buildMimeData(const QJsonObject &args)
{
    auto mime = std::make_unique<QMimeData>();
    if (args.contains(QStringLiteral("urls"))) {
        QList<QUrl> urls;
        for (const auto &value : args.value(QStringLiteral("urls")).toArray()) {
            const QString text = value.toString();
            const QUrl url(text);
            urls.append(url.scheme().isEmpty() ? QUrl::fromLocalFile(QDir(text).absolutePath()) : url);
        }
        mime->setUrls(urls);
    }
    if (args.contains(QStringLiteral("imagePath"))) {
        const QString path = args.value(QStringLiteral("imagePath")).toString();
        QImage image(path);
        if (image.isNull()) {
            fail(QStringLiteral("cannot load image '%1'").arg(path));
        }
        mime->setImageData(image);
    }
    if (args.contains(QStringLiteral("text"))) {
        mime->setText(args.value(QStringLiteral("text")).toString());
    }
    if (args.contains(QStringLiteral("html"))) {
        mime->setHtml(args.value(QStringLiteral("html")).toString());
    }
    const QJsonObject raw = args.value(QStringLiteral("mime")).toObject();
    for (auto it = raw.begin(); it != raw.end(); ++it) {
        mime->setData(it.key(), QByteArray::fromBase64(it.value().toString().toLatin1()));
    }
    if (mime->formats().isEmpty()) {
        fail(QStringLiteral("no payload: pass urls, imagePath, text, html or mime"));
    }
    return mime.release();
}

} // namespace

AutomationServer::AutomationServer(QObject *parent)
    : QObject(parent)
{
    const auto add = [this](const QString &name, Command handler, const QString &summary) {
        m_commands.insert(name, CommandInfo{handler, summary});
    };
    add(QStringLiteral("ping"), &AutomationServer::ping, QStringLiteral("check the connection"));
    add(QStringLiteral("help"), &AutomationServer::help, QStringLiteral("list commands"));
    add(QStringLiteral("tree"), &AutomationServer::tree,
        QStringLiteral("widget tree [target] [hidden:bool] — names, classes, geometry, actions"));
    add(QStringLiteral("state"), &AutomationServer::state,
        QStringLiteral("window, focus, popups and editor state"));
    add(QStringLiteral("screenshot"), &AutomationServer::screenshot,
        QStringLiteral("path [target] [popups:bool=true] — save a PNG of a window or widget"));
    add(QStringLiteral("mouse"), &AutomationServer::mouse,
        QStringLiteral("action:press|release|move, target|x,y, [button] [modifiers]"));
    add(QStringLiteral("click"), &AutomationServer::click,
        QStringLiteral("target|x,y [at] [button] [modifiers] [double:bool]"));
    add(QStringLiteral("drag"), &AutomationServer::drag,
        QStringLiteral("from,to | path:[[x,y],..] [relativeTo] [steps] [button] [modifiers]"));
    add(QStringLiteral("wheel"), &AutomationServer::wheel,
        QStringLiteral("target|x,y [dy=120] [dx] [modifiers]"));
    add(QStringLiteral("key"), &AutomationServer::key,
        QStringLiteral("keys:'Ctrl+Shift+N' (comma separates a sequence)"));
    add(QStringLiteral("type"), &AutomationServer::type,
        QStringLiteral("text — type into the focused widget"));
    add(QStringLiteral("invoke"), &AutomationServer::invoke,
        QStringLiteral("action [checked:bool] [force:bool]"));
    add(QStringLiteral("property"), &AutomationServer::property,
        QStringLiteral("target, name, [value] — read or write a Qt property"));
    add(QStringLiteral("resize"), &AutomationServer::resize, QStringLiteral("width, height [target]"));
    add(QStringLiteral("wait"), &AutomationServer::wait, QStringLiteral("ms — keep processing events"));
    add(QStringLiteral("clipboard"), &AutomationServer::clipboard,
        QStringLiteral("action:get|set|clear [urls|imagePath|text|html|mime] [saveImage]"));
    add(QStringLiteral("drop"), &AutomationServer::drop,
        QStringLiteral("target|x,y + urls|imagePath|text|html|mime [modifiers] — external drag and drop"));
    add(QStringLiteral("quit"), &AutomationServer::quit, QStringLiteral("quit the application"));
}

AutomationServer::~AutomationServer() = default;

bool AutomationServer::listen(const QString &socketPath)
{
    QLocalServer::removeServer(socketPath);
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(socketPath)) {
        qCritical("Automation server cannot listen on %s: %s", qPrintable(socketPath),
                  qPrintable(m_server->errorString()));
        return false;
    }
    connect(m_server, &QLocalServer::newConnection, this, &AutomationServer::handleConnection);
    qInfo("Automation server listening on %s", qPrintable(m_server->fullServerName()));
    return true;
}

void AutomationServer::handleConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] { handleData(socket); });
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
    }
}

void AutomationServer::handleData(QLocalSocket *socket)
{
    // Requests are executed from a queued call rather than inside this readyRead() slot:
    // QLocalSocket never re-emits readyRead() recursively, so a command that opens a modal
    // dialog from within the slot would leave the client's next request unread forever.
    QMetaObject::invokeMethod(
        this,
        [this, guard = QPointer<QLocalSocket>(socket)] {
            if (guard) {
                processRequests(guard);
            }
        },
        Qt::QueuedConnection);
}

void AutomationServer::processRequests(QLocalSocket *socket)
{
    const QPointer<QLocalSocket> guard(socket);
    while (guard && socket->canReadLine()) {
        const QByteArray line = socket->readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        QJsonParseError error{};
        const QJsonDocument request = QJsonDocument::fromJson(line, &error);
        Result result = request.isObject()
                            ? execute(request.object())
                            : Result(QJsonObject{{QStringLiteral("ok"), false},
                                                 {QStringLiteral("error"), error.errorString()}});

        const QByteArray reply = QJsonDocument(result.reply).toJson(QJsonDocument::Compact) + '\n';
        if (!result.deferred) {
            socket->write(reply);
            socket->flush();
            continue;
        }
        QTimer::singleShot(0, socket, [guard, reply] {
            if (guard) {
                guard->write(reply);
                guard->flush();
            }
        });
        try {
            result.deferred();
        } catch (const CommandError &e) {
            qWarning("Automation command failed after replying: %s", qPrintable(e.message));
        }
    }
}

AutomationServer::Result AutomationServer::execute(const QJsonObject &request)
{
    const QString name = request.value(QStringLiteral("cmd")).toString();
    const auto it = m_commands.constFind(name);
    if (it == m_commands.cend()) {
        return QJsonObject{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"), QStringLiteral("unknown command '%1'").arg(name)}};
    }
    try {
        Result result = (this->*(it->handler))(request);
        result.reply.insert(QStringLiteral("ok"), true);
        return result;
    } catch (const CommandError &e) {
        return QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), e.message}};
    }
}

void AutomationServer::sendMouse(QWidget *window, QPoint pos, QEvent::Type type, Qt::MouseButton button,
                                 Qt::KeyboardModifiers modifiers)
{
    QWindow *handle = window->windowHandle();
    if (!handle) {
        fail(QStringLiteral("window has no native handle"));
    }
    if (type == QEvent::MouseButtonPress) {
        m_pressedButtons |= button;
    } else if (type == QEvent::MouseButtonRelease) {
        m_pressedButtons &= ~button;
    }
    m_timestamp += 16;
    // qt_handleMouseEvent (exported by QtGui for QtTest) unlike QTest::mouseMove keeps modifiers on moves.
    qt_handleMouseEvent(handle, pos, handle->mapToGlobal(QPointF(pos)), m_pressedButtons, button, type,
                        modifiers, m_timestamp);
    if (type == QEvent::MouseButtonRelease) {
        // Keep consecutive clicks from turning into double clicks.
        m_timestamp += 1000;
    }
    settle();
}

AutomationServer::Result AutomationServer::ping(const QJsonObject &)
{
    return QJsonObject{{QStringLiteral("pid"), QCoreApplication::applicationPid()},
                       {QStringLiteral("platform"), QGuiApplication::platformName()}};
}

AutomationServer::Result AutomationServer::help(const QJsonObject &)
{
    QJsonObject commands;
    for (auto it = m_commands.cbegin(); it != m_commands.cend(); ++it) {
        commands.insert(it.key(), it->summary);
    }
    return QJsonObject{{QStringLiteral("commands"), commands}};
}

AutomationServer::Result AutomationServer::tree(const QJsonObject &args)
{
    settle();
    const bool includeHidden = args.value(QStringLiteral("hidden")).toBool();
    QJsonArray windows;
    if (args.contains(QStringLiteral("target"))) {
        QWidget *widget = findWidget(args.value(QStringLiteral("target")).toString());
        if (!widget) {
            fail(QStringLiteral("no such widget"));
        }
        windows.append(describeWidget(widget, widget->window(), includeHidden));
    } else {
        for (QWidget *topLevel : QApplication::topLevelWidgets()) {
            if (includeHidden || topLevel->isVisible()) {
                windows.append(describeWidget(topLevel, topLevel, includeHidden));
            }
        }
    }
    return QJsonObject{{QStringLiteral("windows"), windows}};
}

AutomationServer::Result AutomationServer::state(const QJsonObject &)
{
    settle();
    QMainWindow *window = mainWindow();
    const auto nameOf = [](const QWidget *widget) {
        return widget ? QJsonValue(widget->objectName().isEmpty()
                                       ? QString::fromLatin1(widget->metaObject()->className())
                                       : widget->objectName())
                      : QJsonValue();
    };
    QJsonArray topLevels;
    for (QWidget *topLevel : QApplication::topLevelWidgets()) {
        if (topLevel->isVisible()) {
            topLevels.append(QJsonObject{{QStringLiteral("name"), nameOf(topLevel)},
                                         {QStringLiteral("title"), topLevel->windowTitle()},
                                         {QStringLiteral("geometry"), toJson(topLevel->geometry())}});
        }
    }
    QJsonObject result{
        {QStringLiteral("title"),
         window->windowHandle() ? window->windowHandle()->title() : window->windowTitle()},
        {QStringLiteral("size"), QJsonArray{window->width(), window->height()}},
        {QStringLiteral("activeWindow"), nameOf(QApplication::activeWindow())},
        {QStringLiteral("activePopup"), nameOf(QApplication::activePopupWidget())},
        {QStringLiteral("activeModal"), nameOf(QApplication::activeModalWidget())},
        {QStringLiteral("focusWidget"), nameOf(QApplication::focusWidget())},
        {QStringLiteral("topLevels"), topLevels},
        {QStringLiteral("style"), QApplication::style()->name()},
        {QStringLiteral("clipboardFormats"),
         QJsonArray::fromStringList(QGuiApplication::clipboard()->mimeData()
                                        ? QGuiApplication::clipboard()->mimeData()->formats()
                                        : QStringList())},
    };
    if (m_stateProvider) {
        result.insert(QStringLiteral("app"), m_stateProvider());
    }
    return result;
}

AutomationServer::Result AutomationServer::screenshot(const QJsonObject &args)
{
    const QString path = requireString(args, QStringLiteral("path"));
    settle();
    QWidget *target = mainWindow();
    if (args.contains(QStringLiteral("target"))) {
        const QString name = args.value(QStringLiteral("target")).toString();
        if (name == QLatin1String("active")) {
            target = QApplication::activeModalWidget() ? QApplication::activeModalWidget()
                                                       : QApplication::activeWindow();
        } else {
            target = findWidget(name);
        }
        if (!target || !target->isVisible()) {
            fail(QStringLiteral("screenshot target '%1' is not visible").arg(name));
        }
    }

    const bool withPopups = args.value(QStringLiteral("popups")).toBool(true);
    const QImage image =
        target->isWindow() && withPopups ? compositeWithPopups(target) : target->grab().toImage();
    QDir().mkpath(QFileInfo(path).absolutePath());
    if (!image.save(path, "PNG")) {
        fail(QStringLiteral("cannot write '%1'").arg(path));
    }
    return QJsonObject{{QStringLiteral("path"), QFileInfo(path).absoluteFilePath()},
                       {QStringLiteral("size"), QJsonArray{image.width(), image.height()}}};
}

AutomationServer::Result AutomationServer::mouse(const QJsonObject &args)
{
    const QString action = requireString(args, QStringLiteral("action"));
    QEvent::Type type = QEvent::MouseMove;
    if (action == QLatin1String("press")) {
        type = QEvent::MouseButtonPress;
    } else if (action == QLatin1String("release")) {
        type = QEvent::MouseButtonRelease;
    } else if (action != QLatin1String("move")) {
        fail(QStringLiteral("unknown mouse action '%1'").arg(action));
    }
    const WindowPoint point = resolvePoint(args);
    const Qt::MouseButton button = type == QEvent::MouseMove ? Qt::NoButton : parseButton(args);
    const Qt::KeyboardModifiers modifiers = parseModifiers(args);
    QPointer<QWidget> window(point.window);
    return Result({}, [this, window, point, type, button, modifiers] {
        if (window) {
            sendMouse(window, point.pos, type, button, modifiers);
        }
    });
}

AutomationServer::Result AutomationServer::click(const QJsonObject &args)
{
    const WindowPoint point = resolvePoint(args);
    const Qt::MouseButton button = parseButton(args);
    const Qt::KeyboardModifiers modifiers = parseModifiers(args);
    const bool doubleClick = args.value(QStringLiteral("double")).toBool();
    QPointer<QWidget> window(point.window);
    return Result(QJsonObject{{QStringLiteral("at"), QJsonArray{point.pos.x(), point.pos.y()}}},
                  [this, window, point, button, modifiers, doubleClick] {
                      if (!window) {
                          return;
                      }
                      sendMouse(window, point.pos, QEvent::MouseMove, Qt::NoButton, modifiers);
                      sendMouse(window, point.pos, QEvent::MouseButtonPress, button, modifiers);
                      if (doubleClick) {
                          m_timestamp -= 1000; // stay inside the double-click interval
                          sendMouse(window, point.pos, QEvent::MouseButtonRelease, button, modifiers);
                          m_timestamp -= 1000;
                          sendMouse(window, point.pos, QEvent::MouseButtonPress, button, modifiers);
                      }
                      if (window) {
                          sendMouse(window, point.pos, QEvent::MouseButtonRelease, button, modifiers);
                      }
                  });
}

AutomationServer::Result AutomationServer::drag(const QJsonObject &args)
{
    QWidget *window = mainWindow();
    QWidget *origin = nullptr;
    if (args.contains(QStringLiteral("relativeTo"))) {
        origin = findWidget(args.value(QStringLiteral("relativeTo")).toString());
        if (!origin || !origin->isVisible()) {
            fail(QStringLiteral("relativeTo widget is not visible"));
        }
        window = origin->window();
    }

    QList<QPoint> path;
    if (args.contains(QStringLiteral("path"))) {
        for (const auto &value : args.value(QStringLiteral("path")).toArray()) {
            path.append(toPoint(value, QStringLiteral("path")));
        }
        if (path.size() < 2) {
            fail(QStringLiteral("'path' needs at least two points"));
        }
    } else {
        const QPoint from = toPoint(args.value(QStringLiteral("from")), QStringLiteral("from"));
        const QPoint to = toPoint(args.value(QStringLiteral("to")), QStringLiteral("to"));
        const int steps = qMax(1, args.value(QStringLiteral("steps")).toInt(10));
        for (int i = 0; i <= steps; ++i) {
            path.append(from + (to - from) * i / steps);
        }
    }
    if (origin) {
        for (QPoint &point : path) {
            point = origin->mapTo(window, point);
        }
    }

    const Qt::MouseButton button = parseButton(args);
    const Qt::KeyboardModifiers modifiers = parseModifiers(args);
    QPointer<QWidget> guard(window);
    return Result(QJsonObject{{QStringLiteral("points"), int(path.size())}},
                  [this, guard, path, button, modifiers] {
                      sendMouse(guard, path.first(), QEvent::MouseMove, Qt::NoButton, modifiers);
                      sendMouse(guard, path.first(), QEvent::MouseButtonPress, button, modifiers);
                      for (qsizetype i = 1; i < path.size() && guard; ++i) {
                          sendMouse(guard, path.at(i), QEvent::MouseMove, Qt::NoButton, modifiers);
                      }
                      if (guard) {
                          sendMouse(guard, path.last(), QEvent::MouseButtonRelease, button, modifiers);
                      }
                  });
}

AutomationServer::Result AutomationServer::wheel(const QJsonObject &args)
{
    const WindowPoint point = resolvePoint(args);
    const QPoint angle(args.value(QStringLiteral("dx")).toInt(0),
                       args.value(QStringLiteral("dy")).toInt(120));
    const Qt::KeyboardModifiers modifiers = parseModifiers(args);
    QPointer<QWidget> window(point.window);
    return Result({}, [window, point, angle, modifiers] {
        if (!window || !window->windowHandle()) {
            return;
        }
        QWindow *handle = window->windowHandle();
        QWindowSystemInterface::handleWheelEvent(handle, point.pos, handle->mapToGlobal(QPointF(point.pos)),
                                                 QPoint(), angle, modifiers);
        QWindowSystemInterface::flushWindowSystemEvents();
        settle();
    });
}

AutomationServer::Result AutomationServer::key(const QJsonObject &args)
{
    const QKeySequence sequence =
        QKeySequence::fromString(requireString(args, QStringLiteral("keys")), QKeySequence::PortableText);
    if (sequence.isEmpty() || sequence[0] == QKeyCombination()) {
        fail(QStringLiteral("cannot parse key sequence"));
    }
    return Result({}, [sequence] {
        for (int i = 0; i < sequence.count(); ++i) {
            const QKeyCombination combination = sequence[i];
            QTest::keyClick(keyboardWindow(), combination.key(), combination.keyboardModifiers());
            settle();
        }
    });
}

AutomationServer::Result AutomationServer::type(const QJsonObject &args)
{
    const QString text = requireString(args, QStringLiteral("text"));
    QPointer<QWidget> focus(QApplication::focusWidget());
    if (!focus) {
        fail(QStringLiteral("no widget has keyboard focus"));
    }
    return Result({}, [focus, text] {
        if (focus) {
            QTest::keyClicks(focus, text);
            settle();
        }
    });
}

AutomationServer::Result AutomationServer::invoke(const QJsonObject &args)
{
    const QString name = requireString(args, QStringLiteral("action"));
    QPointer<QAction> action(findAction(name));
    if (!action) {
        fail(QStringLiteral("no action named '%1'").arg(name));
    }
    if (!action->isEnabled() && !args.value(QStringLiteral("force")).toBool()) {
        fail(QStringLiteral("action '%1' is disabled").arg(name));
    }
    const QJsonValue checked = args.value(QStringLiteral("checked"));
    return Result({}, [action, checked] {
        if (!action) {
            return;
        }
        if (checked.isBool() && action->isCheckable()) {
            if (action->isChecked() != checked.toBool()) {
                action->trigger();
            }
        } else {
            action->trigger();
        }
        settle();
    });
}

AutomationServer::Result AutomationServer::property(const QJsonObject &args)
{
    QObject *object = findObject(requireString(args, QStringLiteral("target")));
    const QByteArray name = requireString(args, QStringLiteral("name")).toLatin1();
    if (args.contains(QStringLiteral("value"))) {
        if (!object->setProperty(name.constData(), args.value(QStringLiteral("value")).toVariant())) {
            fail(QStringLiteral("cannot set property '%1'").arg(QString::fromLatin1(name)));
        }
        settle();
    }
    return QJsonObject{{QStringLiteral("value"), variantToJson(object->property(name.constData()))}};
}

AutomationServer::Result AutomationServer::resize(const QJsonObject &args)
{
    QWidget *window = mainWindow();
    if (args.contains(QStringLiteral("target"))) {
        window = findWidget(args.value(QStringLiteral("target")).toString());
        if (!window) {
            fail(QStringLiteral("no such window"));
        }
        window = window->window();
    }
    if (window->isMaximized() || window->isFullScreen()) {
        window->showNormal();
    }
    window->resize(args.value(QStringLiteral("width")).toInt(window->width()),
                   args.value(QStringLiteral("height")).toInt(window->height()));
    settle();
    return QJsonObject{{QStringLiteral("size"), QJsonArray{window->width(), window->height()}}};
}

AutomationServer::Result AutomationServer::wait(const QJsonObject &args)
{
    QTest::qWait(args.value(QStringLiteral("ms")).toInt(100));
    return QJsonObject{};
}

AutomationServer::Result AutomationServer::clipboard(const QJsonObject &args)
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    const QString action = args.value(QStringLiteral("action")).toString(QStringLiteral("get"));
    if (action == QLatin1String("set")) {
        clipboard->setMimeData(buildMimeData(args));
        settle();
        return QJsonObject{};
    }
    if (action == QLatin1String("clear")) {
        clipboard->clear();
        settle();
        return QJsonObject{};
    }
    if (action != QLatin1String("get")) {
        fail(QStringLiteral("unknown clipboard action '%1'").arg(action));
    }

    const QMimeData *mime = clipboard->mimeData();
    QJsonObject result{
        {QStringLiteral("formats"), QJsonArray::fromStringList(mime ? mime->formats() : QStringList())},
        {QStringLiteral("hasImage"), mime && mime->hasImage()},
        {QStringLiteral("text"), clipboard->text()}};
    if (args.contains(QStringLiteral("saveImage"))) {
        const QImage image = clipboard->image();
        if (image.isNull()) {
            fail(QStringLiteral("clipboard holds no image"));
        }
        image.save(args.value(QStringLiteral("saveImage")).toString(), "PNG");
        result.insert(QStringLiteral("imageSize"), QJsonArray{image.width(), image.height()});
    }
    return result;
}

AutomationServer::Result AutomationServer::drop(const QJsonObject &args)
{
    const WindowPoint point = resolvePoint(args);
    QWindow *handle = point.window->windowHandle();
    if (!handle) {
        fail(QStringLiteral("window has no native handle"));
    }
    std::shared_ptr<QMimeData> mime(buildMimeData(args));
    const Qt::KeyboardModifiers modifiers = parseModifiers(args);
    const Qt::DropActions actions = Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;

    // Enter/move synchronously so the reply can report whether the target accepts the payload;
    // the drop itself is deferred because handling it may open a dialog.
    const QPlatformDragQtResponse response =
        QWindowSystemInterface::handleDrag(handle, mime.get(), point.pos, actions, Qt::LeftButton, modifiers);
    settle();
    QPointer<QWindow> window(handle);
    const bool accepted = response.isAccepted();
    return Result(QJsonObject{{QStringLiteral("accepted"), accepted},
                              {QStringLiteral("formats"), QJsonArray::fromStringList(mime->formats())}},
                  [window, mime, point, actions, modifiers, accepted] {
                      if (!window) {
                          return;
                      }
                      if (accepted) {
                          QWindowSystemInterface::handleDrop(window, mime.get(), point.pos, actions,
                                                             Qt::LeftButton, modifiers);
                      } else {
                          // A refused drag ends with a leave, like a real one released over a non-target.
                          QWindowSystemInterface::handleDrag(window, nullptr, point.pos, actions,
                                                             Qt::NoButton, modifiers);
                      }
                      settle();
                  });
}

AutomationServer::Result AutomationServer::quit(const QJsonObject &)
{
    QTimer::singleShot(50, qApp, &QCoreApplication::quit);
    return QJsonObject{};
}
