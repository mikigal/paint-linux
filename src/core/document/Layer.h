#pragma once

#include <QImage>
#include <QString>

namespace paint {

// A raster layer. Its image always matches the document size and uses
// premultiplied ARGB so compositing with QPainter needs no conversion.
struct Layer
{
    QString name;
    QImage image;
    bool visible = true;
};

} // namespace paint
