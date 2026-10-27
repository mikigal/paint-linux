#pragma once

#include "document/Document.h"

#include <QString>

#include <optional>

namespace paint {

// Paint for Linux keeps layered images ("Save as project") as OpenRaster (.ora), which GIMP,
// Krita and MyPaint open too. The background is stored as the bottom layer, filled with its
// colour and marked with a pfl:background attribute so it comes back as the background here and
// still looks right elsewhere.

inline constexpr char OpenRasterSuffix[] = "ora";

bool isOpenRasterPath(const QString &path);

struct LoadedProject
{
    std::optional<Document> document;
    QString error;
};

// Reads any OpenRaster file: nested stacks are flattened into layers, and layer offsets and
// opacity are baked into the pixels (Paint layers have neither).
LoadedProject loadOpenRaster(const QString &path);

// Returns an error message, or an empty string on success.
QString saveOpenRaster(const Document &document, const QString &path);

} // namespace paint
