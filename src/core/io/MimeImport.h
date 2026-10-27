#pragma once

#include <QImage>
#include <QString>
#include <QUrl>

#include <optional>

class QMimeData;

namespace paint {

// Turning clipboard and drag-and-drop payloads from other applications into images.
// Sources, in order of preference:
//  * decoded image data (application/x-qt-image) and raw image/* formats — screenshots,
//    "Copy image" in browsers, GIMP, Krita;
//  * text/uri-list with local files — files copied or dragged from Dolphin;
//  * HTML with an <img> whose source is a data: URI.
// Remote images (http/https links from browsers) need a download; see remoteImageUrl().

struct ImportedImage
{
    QImage image; // Format_ARGB32_Premultiplied
    QString path; // the local file it came from, if any
};

std::optional<ImportedImage> imageFromMimeData(const QMimeData *mime);

// The first http(s) URL in the payload that may point at an image (from links or <img> tags).
QUrl remoteImageUrl(const QMimeData *mime);

// Whether the payload can become an image right away or after a download (for enabling Paste
// and accepting drops).
bool mimeDataHasImage(const QMimeData *mime, bool allowRemote);

} // namespace paint
