#include "io/OpenRaster.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <algorithm>
#include <cmath>
#include <memory>
#include <zip.h>

namespace paint {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("paint::OpenRaster", text);
}

constexpr char MimeType[] = "image/openraster";
// Our own attributes on the background layer.
constexpr char Namespace[] = "urn:pl.mikigal.PaintForLinux";
constexpr char NamespacePrefix[] = "pfl";
constexpr int ThumbnailSize = 256;
// A crafted file could claim anything: refuse entries and images too large to be real.
constexpr zip_uint64_t MaxEntrySize = 512ULL * 1024 * 1024;
constexpr int MaxSide = 20000;
constexpr qint64 MaxPixels = 100'000'000;
constexpr double MetersPerInch = 0.0254;

struct Discard
{
    void operator()(zip_t *archive) const { zip_discard(archive); }
};
using Archive = std::unique_ptr<zip_t, Discard>;

std::optional<QByteArray> readEntry(zip_t *archive, const QString &name)
{
    const QByteArray utf8 = name.toUtf8();
    zip_stat_t stat;
    zip_stat_init(&stat);
    if (zip_stat(archive, utf8.constData(), 0, &stat) != 0 || !(stat.valid & ZIP_STAT_SIZE) ||
        stat.size > MaxEntrySize) {
        return std::nullopt;
    }
    zip_file_t *file = zip_fopen(archive, utf8.constData(), 0);
    if (!file) {
        return std::nullopt;
    }
    QByteArray data(qsizetype(stat.size), Qt::Uninitialized);
    const zip_int64_t read = zip_fread(file, data.data(), stat.size);
    zip_fclose(file);
    if (read != zip_int64_t(stat.size)) {
        return std::nullopt;
    }
    return data;
}

struct StackLayer
{
    QString name;
    QString source;
    QPoint offset;
    qreal opacity = 1;
    bool visible = true;
    bool selected = false;
    bool background = false;
    QColor backgroundColor;
};

bool isHidden(const QXmlStreamAttributes &attributes)
{
    return attributes.value(u"visibility") == u"hidden";
}

qreal opacityOf(const QXmlStreamAttributes &attributes)
{
    bool ok = false;
    const qreal opacity = attributes.value(u"opacity").toDouble(&ok);
    return ok ? std::clamp(opacity, 0.0, 1.0) : 1.0;
}

QPoint offsetOf(const QXmlStreamAttributes &attributes)
{
    return QPoint(attributes.value(u"x").toInt(), attributes.value(u"y").toInt());
}

// Collects the layers of a <stack>, top first; nested stacks pass their offset, opacity and
// visibility down to their layers.
void readStack(QXmlStreamReader &xml, QPoint offset, qreal opacity, bool visible, QList<StackLayer> &layers)
{
    while (xml.readNextStartElement()) {
        const QXmlStreamAttributes attributes = xml.attributes();
        if (xml.name() == u"stack") {
            readStack(xml, offset + offsetOf(attributes), opacity * opacityOf(attributes),
                      visible && !isHidden(attributes), layers);
        } else if (xml.name() == u"layer") {
            StackLayer layer;
            layer.name = attributes.value(u"name").toString();
            layer.source = attributes.value(u"src").toString();
            layer.offset = offset + offsetOf(attributes);
            layer.opacity = opacity * opacityOf(attributes);
            layer.visible = visible && !isHidden(attributes);
            layer.selected = attributes.value(u"selected") == u"true";
            layer.background = attributes.value(QLatin1StringView(Namespace), u"background") == u"true";
            layer.backgroundColor =
                QColor::fromString(attributes.value(QLatin1StringView(Namespace), u"background-color"));
            layers.append(layer);
            xml.skipCurrentElement();
        } else {
            xml.skipCurrentElement();
        }
    }
}

QByteArray encodePng(const QImage &image)
{
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return data;
}

} // namespace

bool isOpenRasterPath(const QString &path)
{
    return QFileInfo(path).suffix().compare(QLatin1StringView(OpenRasterSuffix), Qt::CaseInsensitive) == 0;
}

LoadedProject loadOpenRaster(const QString &path)
{
    const QString unreadable = tr("This file can't be opened: it is not a valid OpenRaster image.");
    int error = 0;
    const Archive archive(zip_open(QFile::encodeName(path).constData(), ZIP_RDONLY, &error));
    if (!archive) {
        return {std::nullopt, unreadable};
    }
    if (const std::optional<QByteArray> mime = readEntry(archive.get(), QStringLiteral("mimetype"));
        mime && mime->trimmed() != MimeType) {
        return {std::nullopt, unreadable};
    }
    const std::optional<QByteArray> stack = readEntry(archive.get(), QStringLiteral("stack.xml"));
    if (!stack) {
        return {std::nullopt, unreadable};
    }

    QXmlStreamReader xml(*stack);
    if (!xml.readNextStartElement() || xml.name() != u"image") {
        return {std::nullopt, unreadable};
    }
    const QSize size(xml.attributes().value(u"w").toInt(), xml.attributes().value(u"h").toInt());
    const double dpi = xml.attributes().value(u"xres").toDouble();
    if (size.width() < 1 || size.height() < 1 || size.width() > MaxSide || size.height() > MaxSide ||
        qint64(size.width()) * size.height() > MaxPixels) {
        return {std::nullopt, tr("This image is too large to open.")};
    }
    QList<StackLayer> stackLayers;
    while (xml.readNextStartElement()) {
        if (xml.name() == u"stack") {
            readStack(xml, QPoint(), 1, true, stackLayers);
        } else {
            xml.skipCurrentElement();
        }
    }
    if (xml.hasError()) {
        return {std::nullopt, unreadable};
    }

    // Our background comes back as the background; other files keep their bottom layer.
    QColor background = Qt::white;
    bool backgroundVisible = false;
    if (!stackLayers.isEmpty() && stackLayers.last().background) {
        const StackLayer bottom = stackLayers.takeLast();
        background = bottom.backgroundColor.isValid() ? bottom.backgroundColor : QColor(Qt::white);
        backgroundVisible = bottom.visible;
    }

    QList<Layer> layers;
    qsizetype active = -1;
    // stack.xml lists layers top first; documents keep them bottom first.
    std::ranges::reverse(stackLayers);
    for (const StackLayer &stackLayer : std::as_const(stackLayers)) {
        const std::optional<QByteArray> png = readEntry(archive.get(), stackLayer.source);
        const QImage source = png ? QImage::fromData(*png) : QImage();
        if (source.isNull()) {
            return {std::nullopt, unreadable};
        }
        QImage image;
        if (source.size() == size && stackLayer.offset.isNull() && stackLayer.opacity >= 1) {
            image = source.convertToFormat(Document::LayerFormat);
        } else {
            image = QImage(size, Document::LayerFormat);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            painter.setOpacity(stackLayer.opacity);
            painter.drawImage(stackLayer.offset, source);
        }
        if (stackLayer.selected) {
            active = layers.size();
        }
        layers.append(Layer{stackLayer.name.isEmpty() ? QStringLiteral("Layer %1").arg(layers.size() + 1)
                                                      : stackLayer.name,
                            std::move(image), stackLayer.visible});
    }
    if (layers.isEmpty()) {
        QImage empty(size, Document::LayerFormat);
        empty.fill(Qt::transparent);
        layers.append(Layer{QStringLiteral("Layer 1"), std::move(empty), true});
    }

    Document document(size, background);
    document.setBackgroundVisible(backgroundVisible);
    if (dpi > 0) {
        document.setDotsPerMeter(int(std::lround(dpi / MetersPerInch)));
    }
    document.setLayers(std::move(layers), active >= 0 ? active : document.layerCount() - 1);
    return {std::move(document), {}};
}

QString saveOpenRaster(const Document &document, const QString &path)
{
    QByteArray stack;
    QXmlStreamWriter xml(&stack);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("image"));
    xml.writeNamespace(QLatin1StringView(Namespace), QLatin1StringView(NamespacePrefix));
    const QString dpi = QString::number(std::lround(document.dotsPerMeter() * MetersPerInch));
    xml.writeAttribute(QStringLiteral("version"), QStringLiteral("0.0.5"));
    xml.writeAttribute(QStringLiteral("w"), QString::number(document.size().width()));
    xml.writeAttribute(QStringLiteral("h"), QString::number(document.size().height()));
    xml.writeAttribute(QStringLiteral("xres"), dpi);
    xml.writeAttribute(QStringLiteral("yres"), dpi);
    xml.writeStartElement(QStringLiteral("stack"));
    const auto writeLayer = [&xml](const QString &name, const QString &source, bool visible) {
        xml.writeEmptyElement(QStringLiteral("layer"));
        xml.writeAttribute(QStringLiteral("name"), name);
        xml.writeAttribute(QStringLiteral("src"), source);
        xml.writeAttribute(QStringLiteral("x"), QStringLiteral("0"));
        xml.writeAttribute(QStringLiteral("y"), QStringLiteral("0"));
        xml.writeAttribute(QStringLiteral("opacity"), QStringLiteral("1.000"));
        xml.writeAttribute(QStringLiteral("visibility"),
                           visible ? QStringLiteral("visible") : QStringLiteral("hidden"));
        xml.writeAttribute(QStringLiteral("composite-op"), QStringLiteral("svg:src-over"));
    };
    for (qsizetype i = document.layerCount() - 1; i >= 0; --i) {
        const Layer &layer = document.layer(i);
        writeLayer(layer.name, QStringLiteral("data/layer%1.png").arg(i), layer.visible);
        if (i == document.activeLayerIndex()) {
            xml.writeAttribute(QStringLiteral("selected"), QStringLiteral("true"));
        }
    }
    writeLayer(QStringLiteral("Background"), QStringLiteral("data/background.png"),
               document.isBackgroundVisible());
    xml.writeAttribute(QLatin1StringView(Namespace), QStringLiteral("background"), QStringLiteral("true"));
    xml.writeAttribute(QLatin1StringView(Namespace), QStringLiteral("background-color"),
                       document.backgroundColor().name(QColor::HexArgb));
    xml.writeEndElement(); // stack
    xml.writeEndElement(); // image
    xml.writeEndDocument();

    QList<std::pair<QString, QByteArray>> entries;
    entries.append({QStringLiteral("mimetype"), QByteArray(MimeType)});
    entries.append({QStringLiteral("stack.xml"), stack});
    for (qsizetype i = 0; i < document.layerCount(); ++i) {
        entries.append({QStringLiteral("data/layer%1.png").arg(i), encodePng(document.layer(i).image)});
    }
    QImage background(document.size(), QImage::Format_ARGB32);
    background.fill(document.backgroundColor());
    entries.append({QStringLiteral("data/background.png"), encodePng(background)});
    const QImage merged = document.composite();
    entries.append({QStringLiteral("mergedimage.png"), encodePng(merged)});
    entries.append({QStringLiteral("Thumbnails/thumbnail.png"),
                    encodePng(merged.width() > ThumbnailSize || merged.height() > ThumbnailSize
                                  ? merged.scaled(ThumbnailSize, ThumbnailSize, Qt::KeepAspectRatio,
                                                  Qt::SmoothTransformation)
                                  : merged)});

    int error = 0;
    Archive archive(zip_open(QFile::encodeName(path).constData(), ZIP_CREATE | ZIP_TRUNCATE, &error));
    if (!archive) {
        return tr("The file can't be saved.");
    }
    for (const auto &[name, data] : std::as_const(entries)) {
        // libzip reads the buffers when the archive is closed; `entries` outlives that.
        zip_source_t *source =
            zip_source_buffer(archive.get(), data.constData(), zip_uint64_t(data.size()), 0);
        const zip_int64_t index =
            source ? zip_file_add(archive.get(), name.toUtf8().constData(), source, ZIP_FL_ENC_UTF_8) : -1;
        if (index < 0) {
            zip_source_free(source);
            return tr("The file can't be saved.");
        }
        // The mimetype comes first and uncompressed so the file can be recognised; PNGs are
        // compressed already.
        zip_set_file_compression(archive.get(), zip_uint64_t(index),
                                 name == u"stack.xml" ? ZIP_CM_DEFLATE : ZIP_CM_STORE, 0);
    }
    // Writing happens here; on failure the archive is still open and gets discarded.
    zip_t *open = archive.release();
    if (zip_close(open) != 0) {
        zip_discard(open);
        return tr("The file can't be saved.");
    }
    return {};
}

} // namespace paint
