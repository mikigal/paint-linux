#pragma once

#include <QImage>
#include <QRectF>
#include <QSize>

class QPrinter;

// Where Paint puts a picture on paper: at its own size (from its resolution) at the top-left of
// the printable area, shrunk to fit when it is larger. `page` is the printable area in printer
// pixels at `printerDpi`.
QRectF printArea(QSize picture, int dotsPerMeter, const QRectF &page, int printerDpi);

// Prints `picture` on one page; false when the printer could not be started.
bool printPicture(QPrinter *printer, const QImage &picture, int dotsPerMeter);
