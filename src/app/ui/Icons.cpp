#include "ui/Icons.h"

#include "shapes/ShapeGeometry.h"

#include <QApplication>
#include <QFile>
#include <QHash>
#include <QIconEngine>
#include <QPainter>
#include <QPixmapCache>
#include <QSvgRenderer>

#include <functional>

namespace paint::ui {

namespace {

// Colour tokens written by tools/icons/build_icons.py.
constexpr QLatin1StringView ForegroundToken("#1a1a1a");
constexpr QLatin1StringView AccentToken("#005fb8");

using IconPainter = std::function<void(QPainter *, const QRectF &, const IconColors &)>;

class PaletteIconEngine : public QIconEngine
{
public:
    PaletteIconEngine(QString key, IconPainter painter)
        : m_key(std::move(key))
        , m_painter(std::move(painter))
    {}

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State) override
    {
        const qreal ratio = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
        painter->drawImage(rect, render(rect.size() * ratio, mode));
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }

    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State, qreal scale) override
    {
        const QSize pixelSize = size * scale;
        const IconColors colors = iconColors(QApplication::palette());
        const QString cacheKey = QStringLiteral("paint-icon:%1:%2x%3:%4:%5:%6")
                                     .arg(m_key)
                                     .arg(pixelSize.width())
                                     .arg(pixelSize.height())
                                     .arg(int(mode))
                                     .arg(colors.foreground.rgba())
                                     .arg(colors.accent.rgba());
        QPixmap pixmap;
        if (!QPixmapCache::find(cacheKey, &pixmap)) {
            pixmap = QPixmap::fromImage(render(pixelSize, mode));
            QPixmapCache::insert(cacheKey, pixmap);
        }
        pixmap.setDevicePixelRatio(scale);
        return pixmap;
    }

    QSize actualSize(const QSize &size, QIcon::Mode, QIcon::State) override { return size; }
    QIconEngine *clone() const override { return new PaletteIconEngine(m_key, m_painter); }
    QString key() const override { return QStringLiteral("PaletteIconEngine"); }
    bool isNull() override { return false; }

private:
    QImage render(QSize pixelSize, QIcon::Mode mode) const
    {
        QImage image(pixelSize, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        const QPalette palette = QApplication::palette();
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        m_painter(&painter, QRectF(QPointF(0, 0), pixelSize), iconColors(palette));
        if (mode == QIcon::Disabled) {
            // Paint greys out the whole icon, accents included.
            painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            painter.fillRect(image.rect(), palette.color(QPalette::Disabled, QPalette::WindowText));
        }
        return image;
    }

    QString m_key;
    IconPainter m_painter;
};

QByteArray svgSource(const QString &name)
{
    static QHash<QString, QByteArray> cache;
    auto it = cache.find(name);
    if (it == cache.end()) {
        QFile file(QStringLiteral(":/paint/icons/%1.svg").arg(name));
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning("Missing icon %s", qPrintable(name));
        }
        it = cache.insert(name, file.readAll());
    }
    return *it;
}

} // namespace

bool isDarkPalette(const QPalette &palette)
{
    return palette.color(QPalette::Window).lightness() < 128;
}

IconColors iconColors(const QPalette &palette)
{
    // The accent is Paint's own blue, lightened for dark backgrounds.
    return {palette.color(QPalette::WindowText),
            isDarkPalette(palette) ? QColor(0x4c, 0xc2, 0xff) : QColor(0x00, 0x5f, 0xb8)};
}

QIcon icon(const QString &name)
{
    return QIcon(
        new PaletteIconEngine(name, [name](QPainter *painter, const QRectF &rect, const IconColors &colors) {
            QByteArray svg = svgSource(name);
            svg.replace(ForegroundToken.data(), colors.foreground.name().toLatin1());
            svg.replace(AccentToken.data(), colors.accent.name().toLatin1());
            QSvgRenderer(svg).render(painter, rect);
        }));
}

QIcon shapeIcon(ShapeType type)
{
    return QIcon(new PaletteIconEngine(
        QStringLiteral("shape-%1").arg(int(type)),
        [type](QPainter *painter, const QRectF &rect, const IconColors &colors) {
            // Icons are designed on a 20 px grid with a 2.5 px margin.
            const qreal unit = rect.width() / 20.0;
            QPen pen(colors.foreground, qMax(1.0, unit));
            pen.setJoinStyle(Qt::MiterJoin);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            const qreal margin = 2.5 * unit;
            painter->drawPath(shapePath(type, rect.adjusted(margin, margin, -margin, -margin)));
        }));
}

} // namespace paint::ui
