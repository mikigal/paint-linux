#include "ops/Wallpaper.h"

#include <QPainter>

namespace paint {

QImage wallpaperImage(const QImage &picture, QSize screen, WallpaperMode mode)
{
    QImage result(screen, QImage::Format_RGB32);
    result.fill(Qt::black);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    switch (mode) {
    case WallpaperMode::Fill: {
        QRectF target(QPointF(),
                      QSizeF(picture.size()).scaled(QSizeF(screen), Qt::KeepAspectRatioByExpanding));
        target.moveCenter(QRectF(result.rect()).center());
        painter.drawImage(target, picture);
        break;
    }
    case WallpaperMode::Tile:
        painter.fillRect(result.rect(), QBrush(picture));
        break;
    case WallpaperMode::Center: {
        QRect target(QPoint(), picture.size());
        target.moveCenter(result.rect().center());
        painter.drawImage(target.topLeft(), picture);
        break;
    }
    }
    return result;
}

} // namespace paint
