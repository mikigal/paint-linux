#include "platform/RemoteImageLoader.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

RemoteImageLoader::RemoteImageLoader(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{}

void RemoteImageLoader::load(const QUrl &url, std::function<void(const QImage &, const QString &)> done)
{
    QNetworkRequest request(url);
    request.setTransferTimeout(TimeoutMs);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = m_network->get(request);
    // Refuse anything larger than an image could reasonably be.
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64 total) {
        if (received > MaxBytes || total > MaxBytes) {
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            done({}, reply->errorString());
            return;
        }
        const QImage image = QImage::fromData(reply->readAll());
        done(image, image.isNull() ? tr("The link does not point to an image.") : QString());
    });
}
