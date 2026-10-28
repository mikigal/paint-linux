#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

// Sets the desktop background through the XDG desktop portal (org.freedesktop.portal.Wallpaper),
// which KDE, GNOME and other desktops provide, so no desktop-specific code is needed.
class WallpaperPortal : public QObject
{
    Q_OBJECT

public:
    explicit WallpaperPortal(QObject *parent = nullptr);

    // Asks the desktop to show the image at `path` as its background; finished() follows.
    void setWallpaper(const QString &path);

Q_SIGNALS:
    // `error` explains a failure; a request the user declined is not a success but has no error.
    void finished(bool success, const QString &error);

private Q_SLOTS:
    void handleResponse(uint response, const QVariantMap &results);

private:
    void stopListening();

    QString m_requestPath;
};
