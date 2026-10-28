#include "platform/Printing.h"

#include <QPainter>
#include <QPrinter>

#include <algorithm>
#include <cmath>

namespace {

constexpr double MetersPerInch = 0.0254;
constexpr double DefaultDpi = 96;

} // namespace

QRectF printArea(QSize picture, int dotsPerMeter, const QRectF &page, int printerDpi)
{
    // Whole DPI, as Image properties shows it (3780 dots per metre is 96 DPI).
    const double pictureDpi =
        dotsPerMeter > 0 ? std::max(1.0, std::round(dotsPerMeter * MetersPerInch)) : DefaultDpi;
    QSizeF size = QSizeF(picture) * (printerDpi / pictureDpi);
    if (size.width() > page.width() || size.height() > page.height()) {
        size.scale(page.size(), Qt::KeepAspectRatio);
    }
    return QRectF(page.topLeft(), size);
}

bool printPicture(QPrinter *printer, const QImage &picture, int dotsPerMeter)
{
    QPainter painter;
    if (!painter.begin(printer)) {
        return false;
    }
    // The painter's origin is the top-left of the printable area.
    const QRectF page(QPointF(), printer->pageLayout().paintRectPixels(printer->resolution()).size());
    const QRectF area = printArea(picture.size(), dotsPerMeter, page, printer->resolution());
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(area, picture);
    return painter.end();
}
