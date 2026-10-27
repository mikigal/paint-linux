#pragma once

#include "tools/ToolTypes.h"

#include <QColor>
#include <QImage>
#include <QPoint>

namespace paint {

// Deterministic per-pixel noise in [0, 1].
float whiteNoise(int x, int y);
// Smooth noise in [0, 1] with features about one unit apart.
float valueNoise(float x, float y);

// Colours a mark in one of Paint's textured styles (crayon, marker, oil, natural pencil,
// watercolour; solid and none are plain). `coverage` (Format_Alpha8) tells how much of each pixel
// the mark covers; `grainTopLeft` is its first pixel in grain coordinates — a shape's own, so the
// texture moves with the shape, or the document's, so brush strokes share one paper grain.
// Returns a non-premultiplied ARGB32 image of the same size.
QImage textured(const QImage &coverage, StrokeStyle style, const QColor &color, QPoint grainTopLeft);

// How far (in pixels) watercolour looks past a pixel to find the mark's edge.
inline constexpr int WatercolorEdge = 2;

} // namespace paint
