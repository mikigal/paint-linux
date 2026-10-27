#pragma once

#include <QImage>
#include <QObject>
#include <QUrl>

#include <functional>

class QNetworkAccessManager;

// Downloads an image dragged or pasted from a web browser as a link.
class RemoteImageLoader : public QObject
{
    Q_OBJECT

public:
    static constexpr qint64 MaxBytes = 64 * 1024 * 1024;
    static constexpr int TimeoutMs = 30'000;

    explicit RemoteImageLoader(QObject *parent = nullptr);

    // Calls `done` with the image, or with a null image and an error message.
    void load(const QUrl &url, std::function<void(const QImage &image, const QString &error)> done);

private:
    QNetworkAccessManager *m_network;
};
