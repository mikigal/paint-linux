#include "io/Encoders.h"

#include <QList>
#include <QtEndian>

#include <array>
#include <bit>
#include <vector>

namespace paint {

namespace {

// The 16 colours of Windows' standard palette, in their usual order.
const QList<QRgb> &windows16()
{
    static const QList<QRgb> colors{
        qRgb(0, 0, 0),       qRgb(128, 0, 0),   qRgb(0, 128, 0),   qRgb(128, 128, 0),
        qRgb(0, 0, 128),     qRgb(128, 0, 128), qRgb(0, 128, 128), qRgb(192, 192, 192),
        qRgb(128, 128, 128), qRgb(255, 0, 0),   qRgb(0, 255, 0),   qRgb(255, 255, 0),
        qRgb(0, 0, 255),     qRgb(255, 0, 255), qRgb(0, 255, 255), qRgb(255, 255, 255),
    };
    return colors;
}

template<typename T>
void append(QByteArray &data, T value)
{
    const T little = qToLittleEndian(value);
    data.append(reinterpret_cast<const char *>(&little), sizeof(T));
}

// Packs LZW codes of growing width, least significant bit first, into GIF data sub-blocks.
class CodeWriter
{
public:
    explicit CodeWriter(QByteArray &out)
        : m_out(out)
    {}

    void write(quint32 code, quint32 width)
    {
        m_bits |= code << m_count;
        m_count += width;
        while (m_count >= 8) {
            put(char(m_bits & 0xffu));
            m_bits >>= 8u;
            m_count -= 8;
        }
    }

    void finish()
    {
        if (m_count > 0) {
            put(char(m_bits & 0xffu));
        }
        flushBlock();
        m_out.append('\0'); // block terminator
    }

private:
    void put(char byte)
    {
        m_block.append(byte);
        if (m_block.size() == 255) {
            flushBlock();
        }
    }

    void flushBlock()
    {
        if (!m_block.isEmpty()) {
            m_out.append(char(m_block.size()));
            m_out.append(m_block);
            m_block.clear();
        }
    }

    QByteArray &m_out;
    QByteArray m_block;
    quint32 m_bits = 0;
    quint32 m_count = 0;
};

// LZW-compresses palette indices the way GIF decoders expect.
void compress(const QImage &indexed, int minimumCodeSize, QByteArray &out)
{
    constexpr quint32 MaxCode = 4095;
    const quint32 clearCode = 1u << quint32(minimumCodeSize);
    const quint32 endCode = clearCode + 1;
    // next[code * 256 + index]: the code for `code`'s string followed by `index`, or 0.
    std::vector<quint16> next(size_t(MaxCode + 1) * 256, 0);
    quint32 codeSize = quint32(minimumCodeSize) + 1;
    quint32 lastCode = endCode;

    CodeWriter writer(out);
    writer.write(clearCode, codeSize);
    int current = -1;
    for (int y = 0; y < indexed.height(); ++y) {
        const uchar *line = indexed.constScanLine(y);
        for (int x = 0; x < indexed.width(); ++x) {
            const int index = line[x];
            if (current < 0) {
                current = index;
                continue;
            }
            quint16 &known = next[size_t(current) * 256 + size_t(index)];
            if (known) {
                current = known;
                continue;
            }
            writer.write(quint32(current), codeSize);
            known = quint16(++lastCode);
            if (lastCode >= (1u << codeSize)) {
                ++codeSize;
            }
            if (lastCode == MaxCode) {
                // The table is full: start over.
                writer.write(clearCode, codeSize);
                std::fill(next.begin(), next.end(), quint16(0));
                codeSize = quint32(minimumCodeSize) + 1;
                lastCode = endCode;
            }
            current = index;
        }
    }
    if (current >= 0) {
        writer.write(quint32(current), codeSize);
    }
    writer.write(endCode, codeSize);
    writer.finish();
}

} // namespace

QImage reduceColors(const QImage &image, int bitsPerPixel)
{
    const QImage opaque = image.convertToFormat(QImage::Format_RGB32);
    switch (bitsPerPixel) {
    case 1: {
        // Black below half brightness, white above, as Paint's monochrome bitmaps.
        QImage mono(opaque.size(), QImage::Format_Mono);
        mono.setColorTable({qRgb(0, 0, 0), qRgb(255, 255, 255)});
        for (int y = 0; y < opaque.height(); ++y) {
            const auto *line = reinterpret_cast<const QRgb *>(opaque.constScanLine(y));
            for (int x = 0; x < opaque.width(); ++x) {
                mono.setPixel(x, y, qGray(line[x]) >= 128 ? 1 : 0);
            }
        }
        mono.setDotsPerMeterX(image.dotsPerMeterX());
        mono.setDotsPerMeterY(image.dotsPerMeterY());
        return mono;
    }
    case 4:
        return opaque.convertToFormat(QImage::Format_Indexed8, windows16(), Qt::ThresholdDither);
    default:
        return opaque.convertToFormat(QImage::Format_Indexed8, Qt::DiffuseDither);
    }
}

QByteArray encodeBmp(const QImage &source, int bitsPerPixel)
{
    Q_ASSERT(bitsPerPixel == 1 || bitsPerPixel == 4 || bitsPerPixel == 8 || bitsPerPixel == 24);
    const QImage image = bitsPerPixel == 24 ? source.convertToFormat(QImage::Format_RGB32)
                                            : reduceColors(source, bitsPerPixel);
    const int width = image.width();
    const int height = image.height();
    const quint32 rowSize = ((quint32(bitsPerPixel) * quint32(width) + 31) / 32) * 4;
    const quint32 colors = bitsPerPixel == 24 ? 0 : 1u << quint32(bitsPerPixel);
    const quint32 offset = 14 + 40 + colors * 4;
    const quint32 imageSize = rowSize * quint32(height);

    QByteArray data;
    data.reserve(qsizetype(offset) + qsizetype(imageSize));
    // BITMAPFILEHEADER
    data.append("BM");
    append<quint32>(data, offset + imageSize);
    append<quint32>(data, 0);
    append<quint32>(data, offset);
    // BITMAPINFOHEADER; a positive height stores the rows bottom-up.
    append<quint32>(data, 40);
    append<qint32>(data, width);
    append<qint32>(data, height);
    append<quint16>(data, 1);
    append<quint16>(data, quint16(bitsPerPixel));
    append<quint32>(data, 0); // BI_RGB
    append<quint32>(data, imageSize);
    append<qint32>(data, source.dotsPerMeterX());
    append<qint32>(data, source.dotsPerMeterY());
    append<quint32>(data, colors);
    append<quint32>(data, 0);
    const QList<QRgb> table = image.colorTable();
    for (quint32 i = 0; i < colors; ++i) {
        const QRgb color = i < quint32(table.size()) ? table.at(i) : 0;
        data.append(char(qBlue(color)));
        data.append(char(qGreen(color)));
        data.append(char(qRed(color)));
        data.append('\0');
    }

    QByteArray row(qsizetype(rowSize), '\0');
    for (int y = height - 1; y >= 0; --y) {
        row.fill('\0');
        const uchar *line = image.constScanLine(y);
        switch (bitsPerPixel) {
        case 1:
            // Format_Mono keeps eight pixels per byte, most significant bit first, like BMP.
            std::copy_n(line, (width + 7) / 8, row.data());
            break;
        case 4:
            for (int x = 0; x < width; ++x) {
                // Two pixels a byte, the first in the high half.
                row[x / 2] =
                    char(quint32(uchar(row[x / 2])) | (x % 2 ? quint32(line[x]) : quint32(line[x]) << 4u));
            }
            break;
        case 8:
            std::copy_n(line, width, row.data());
            break;
        default: {
            const auto *pixels = reinterpret_cast<const QRgb *>(line);
            for (int x = 0; x < width; ++x) {
                row[3 * x] = char(qBlue(pixels[x]));
                row[3 * x + 1] = char(qGreen(pixels[x]));
                row[3 * x + 2] = char(qRed(pixels[x]));
            }
            break;
        }
        }
        data.append(row);
    }
    return data;
}

QByteArray encodeGif(const QImage &source)
{
    const QImage image = reduceColors(source, 8);
    QList<QRgb> table = image.colorTable();
    // The colour table holds a power of two entries, at least two.
    const int tableBits = std::max(1, int(std::bit_width(quint32(std::max<qsizetype>(2, table.size()) - 1))));
    table.resize(qsizetype(1) * (1u << quint32(tableBits)), 0);

    QByteArray data;
    data.append("GIF89a");
    append<quint16>(data, quint16(image.width()));
    append<quint16>(data, quint16(image.height()));
    // A global colour table, 8 bits per colour channel, and the table's size.
    data.append(char(0x80u | (7u << 4u) | quint32(tableBits - 1)));
    data.append('\0'); // background colour index
    data.append('\0'); // square pixels
    for (QRgb color : std::as_const(table)) {
        data.append(char(qRed(color)));
        data.append(char(qGreen(color)));
        data.append(char(qBlue(color)));
    }
    // Image descriptor: the whole screen, no local table, not interlaced.
    data.append(',');
    append<quint16>(data, 0);
    append<quint16>(data, 0);
    append<quint16>(data, quint16(image.width()));
    append<quint16>(data, quint16(image.height()));
    data.append('\0');
    const int minimumCodeSize = std::max(2, tableBits);
    data.append(char(minimumCodeSize));
    compress(image, minimumCodeSize, data);
    data.append(';'); // trailer
    return data;
}

} // namespace paint
