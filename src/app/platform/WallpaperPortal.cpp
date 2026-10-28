#include "platform/WallpaperPortal.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QFile>
#include <QRandomGenerator>

namespace {

constexpr QLatin1StringView Service("org.freedesktop.portal.Desktop");
constexpr QLatin1StringView ObjectPath("/org/freedesktop/portal/desktop");
constexpr QLatin1StringView WallpaperInterface("org.freedesktop.portal.Wallpaper");
constexpr QLatin1StringView RequestInterface("org.freedesktop.portal.Request");

} // namespace

WallpaperPortal::WallpaperPortal(QObject *parent)
    : QObject(parent)
{}

void WallpaperPortal::setWallpaper(const QString &path)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QFile file(path);
    if (!bus.isConnected() || !file.open(QIODevice::ReadOnly)) {
        Q_EMIT finished(false, tr("The desktop can't be reached."));
        return;
    }
    stopListening();
    // The portal answers on a request object whose path follows from our bus name and a token;
    // listen there before asking, so the answer cannot be missed.
    const QString token = QStringLiteral("paint%1").arg(QRandomGenerator::global()->generate());
    const QString sender = bus.baseService().mid(1).replace(QLatin1Char('.'), QLatin1Char('_'));
    m_requestPath = QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
    bus.connect(Service, m_requestPath, RequestInterface, QStringLiteral("Response"), this,
                SLOT(handleResponse(uint, QVariantMap)));

    QDBusMessage call = QDBusMessage::createMethodCall(Service, ObjectPath, WallpaperInterface,
                                                       QStringLiteral("SetWallpaperFile"));
    const QVariantMap options{
        {QStringLiteral("handle_token"), token},
        {QStringLiteral("show-preview"), false},
        {QStringLiteral("set-on"), QStringLiteral("background")},
    };
    call << QString() << QVariant::fromValue(QDBusUnixFileDescriptor(file.handle())) << options;
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *pending) {
        pending->deleteLater();
        const QDBusPendingReply<QDBusObjectPath> reply = *pending;
        if (reply.isError()) {
            stopListening();
            const bool missing = reply.error().type() == QDBusError::ServiceUnknown ||
                                 reply.error().type() == QDBusError::UnknownInterface ||
                                 reply.error().type() == QDBusError::UnknownMethod;
            Q_EMIT finished(false, missing ? tr("This desktop doesn't offer setting the background "
                                                "(the XDG desktop portal is missing).")
                                           : reply.error().message());
        }
    });
}

void WallpaperPortal::handleResponse(uint response, const QVariantMap &)
{
    stopListening();
    // 0: done, 1: the user cancelled, 2: something else went wrong.
    Q_EMIT finished(response == 0, response == 2 ? tr("The desktop refused the picture.") : QString());
}

void WallpaperPortal::stopListening()
{
    if (!m_requestPath.isEmpty()) {
        QDBusConnection::sessionBus().disconnect(Service, m_requestPath, RequestInterface,
                                                 QStringLiteral("Response"), this,
                                                 SLOT(handleResponse(uint, QVariantMap)));
        m_requestPath.clear();
    }
}
