#!/usr/bin/env python3
"""Builds the Paint-style icon set in resources/icons/ from Fluent System Icons.

Windows 11 Paint uses Fluent "regular" line icons with coloured accents (blue wedges, a yellow
pencil, a pink eraser...). Each icon here is composed from layers:
  * outline(name)            the 20 px "regular" glyph,
  * fill(name)               the 20 px "filled" glyph (painted under the outline as a colour accent),
  * raw(svg)                 hand-drawn SVG elements,
each optionally clipped to a region so only part of a glyph takes a colour.

Colours are written as tokens that the app swaps at runtime for the current palette
(see FOREGROUND / ACCENT below); the other accent colours are the same in light and dark themes.

Clip paths, strokes and transforms are baked into plain filled paths with picosvg, because QtSvg
does not apply clip-path; the committed icons therefore render the same in any SVG renderer.

Usage:
    python3 -m venv .venv && .venv/bin/pip install -r tools/icons/requirements.txt
    npm pack @fluentui/svg-icons@1.1.344 && tar xzf fluentui-svg-icons-1.1.344.tgz
    .venv/bin/python tools/icons/build_icons.py --fluent package/icons [--preview preview.png]
"""

from __future__ import annotations

import argparse
import colorsys
import math
import re
import shutil
import subprocess
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

try:
    from picosvg.svg import SVG
except ImportError:  # pragma: no cover - developer setup hint
    raise SystemExit("picosvg is required: pip install -r tools/icons/requirements.txt")

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "resources/icons"

# Runtime-replaced tokens (keep in sync with the app's icon loader).
FOREGROUND = "#1a1a1a"  # → palette text colour
ACCENT = "#005fb8"      # → lighter blue in dark themes
# Fixed accents measured from Paint's toolbar.
YELLOW = "#eece82"
PINK = "#f6aeac"
ORANGE = "#ff6327"
GREY = "#9e9e9e"

DARK_REPLACEMENTS = {FOREGROUND: "#ffffff", ACCENT: "#4cc2ff"}


@dataclass
class Layer:
    kind: str                 # "outline", "fill" or "raw"
    source: str = ""          # Fluent icon base name, or raw SVG markup
    color: str = FOREGROUND
    clip: str | None = None   # SVG shape markup in the 20x20 grid


@dataclass
class Icon:
    name: str
    layers: list[Layer] = field(default_factory=list)
    transform: str | None = None  # applied to the whole icon


def outline(name: str, color: str = FOREGROUND, clip: str | None = None) -> Layer:
    return Layer("outline", name, color, clip)


def fill(name: str, color: str, clip: str | None = None) -> Layer:
    return Layer("fill", name, color, clip)


def raw(markup: str) -> Layer:
    return Layer("raw", markup)


def rect(x: float, y: float, w: float, h: float) -> str:
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}"/>'


def polygon(*points: tuple[float, float]) -> str:
    return '<polygon points="' + " ".join(f"{x},{y}" for x, y in points) + '"/>'


def everything_but(x: float, y: float, w: float, h: float) -> str:
    # Even-odd path covering the whole grid except one rectangle.
    return (f'<path clip-rule="evenodd" d="M-1 -1H21V21H-1Z M{x} {y}h{w}v{h}h{-w}Z"/>')


def colour_wheel() -> Layer:
    """A twelve-segment hue wheel with a "+" badge, like Paint's Edit colors button."""
    segments = []
    for i in range(12):
        a1 = math.radians(i * 30 - 90)
        a2 = math.radians((i + 1) * 30 - 90)
        segments.append(
            f'<path d="M9 10L{9 + 7 * math.cos(a1):.3f} {10 + 7 * math.sin(a1):.3f}'
            f'A7 7 0 0 1 {9 + 7 * math.cos(a2):.3f} {10 + 7 * math.sin(a2):.3f}Z" '
            f'fill="{hsl_to_hex(i * 30, 0.9, 0.55)}"/>')
    centre = '<circle cx="9" cy="10" r="2.5" fill="#ffffff"/>'
    # Fixed colours: the badge stays a white chip with a dark "+" in both themes.
    badge = ('<circle cx="15.5" cy="4.5" r="3.5" fill="#ffffff" stroke="#8a8a8a"/>'
             '<path d="M15.5 2.8v3.4M13.8 4.5h3.4" stroke="#1b1b1b" stroke-linecap="round"/>')
    return raw("".join(segments) + centre + badge)


def hsl_to_hex(hue: float, saturation: float, lightness: float) -> str:
    r, g, b = colorsys.hls_to_rgb(hue / 360, lightness, saturation)
    return "#{:02x}{:02x}{:02x}".format(round(r * 255), round(g * 255), round(b * 255))


DASHED = f'fill="none" stroke="{FOREGROUND}" stroke-width="1" stroke-dasharray="2 1.5"'

ICONS: list[Icon] = [
    # Quick access
    Icon("save", [outline("save")]),
    Icon("undo", [outline("arrow_undo")]),
    Icon("redo", [outline("arrow_redo")]),
    # Selection
    Icon("select-rectangle", [raw(f'<rect x="3.5" y="4.5" width="13" height="11" rx="0.5" {DASHED}/>')]),
    Icon("select-freeform", [raw(
        f'<path d="M6 4.5C9 3 15.5 4 16 8.5S11 16 7.5 15.5 2.5 12 3.5 8.5 4.5 5.2 6 4.5Z" {DASHED}/>')]),
    Icon("select-all", [raw(f'<rect x="3.5" y="3.5" width="13" height="13" rx="0.5" {DASHED}/>'),
                        raw("".join(f'<rect x="{x}" y="{y}" width="2" height="2" fill="{FOREGROUND}"/>'
                                    for x in (6, 9, 12) for y in (6, 9, 12)))]),
    Icon("select-invert", [raw(f'<rect x="3.5" y="3.5" width="13" height="13" rx="0.5" {DASHED}/>'),
                           raw(f'<rect x="6" y="6" width="8" height="8" fill="{ACCENT}"/>')]),
    Icon("delete", [outline("delete")]),
    # Image
    Icon("crop", [outline("crop")]),
    Icon("resize", [outline("resize_image", ACCENT)]),
    Icon("rotate", [fill("rotate_right", ACCENT, clip=rect(0, 10, 20, 10)),
                    outline("rotate_right", clip=rect(0, 0, 20, 10))]),
    Icon("rotate-right", [fill("rotate_right", ACCENT, clip=rect(0, 10, 20, 10)),
                          outline("rotate_right", clip=rect(0, 0, 20, 10))]),
    Icon("rotate-left", [fill("rotate_left", ACCENT, clip=rect(0, 10, 20, 10)),
                         outline("rotate_left", clip=rect(0, 0, 20, 10))]),
    Icon("rotate-180", [outline("arrow_rotate_clockwise")]),
    Icon("rotate-custom", [outline("arrow_rotate_clockwise"), raw(f'<circle cx="10" cy="10" r="1.5" fill="{ACCENT}"/>')]),
    Icon("flip", [fill("flip_vertical", ACCENT, clip=rect(0, 10, 20, 10)),
                  outline("flip_vertical", clip=rect(0, 0, 20, 10))]),
    Icon("flip-vertical", [fill("flip_vertical", ACCENT, clip=rect(0, 10, 20, 10)),
                           outline("flip_vertical", clip=rect(0, 0, 20, 10))]),
    Icon("flip-horizontal", [outline("flip_horizontal", ACCENT, clip=rect(0, 0, 10, 20)),
                             outline("flip_horizontal", clip=rect(10, 0, 10, 20))]),
    Icon("invert-colors", [raw(f'<circle cx="10" cy="10" r="6.5" fill="none" stroke="{FOREGROUND}"/>'),
                           raw(f'<path d="M10 3.5a6.5 6.5 0 0 1 0 13Z" fill="{FOREGROUND}"/>')]),
    # Tools
    Icon("pencil", [fill("edit", YELLOW), outline("edit")]),
    Icon("fill", [fill("paint_bucket", ACCENT, clip=polygon((0, 11), (13, 11), (13, 20), (0, 20))),
                  fill("paint_bucket", ORANGE, clip=rect(13.5, 10, 6.5, 10)),
                  outline("paint_bucket", clip=everything_but(13.5, 10, 6.5, 10))]),
    Icon("text", [raw(f'<path d="M4 16.5 10 3.5l6 13M6.3 11.5h7.4" fill="none" stroke="{FOREGROUND}" '
                      'stroke-linejoin="round"/>')]),
    # The filled eraser glyph fills the body, while Paint colours the tip: draw the tip by hand.
    Icon("eraser", [raw(f'<polygon points="2.8,12.2 5.4,9.6 10.2,14.4 7.6,17" fill="{PINK}"/>'),
                    outline("eraser")]),
    Icon("color-picker", [fill("eyedropper", ACCENT, clip=polygon((4, 0), (20, 0), (20, 16))),
                          outline("eyedropper", clip=f'<path clip-rule="evenodd" d="M-1 -1H21V21H-1Z M4 0H20V16Z"/>')]),
    Icon("magnifier", [outline("zoom_in", clip=everything_but(5.5, 5.5, 5, 5)),
                       outline("zoom_in", ACCENT, clip=rect(5.5, 5.5, 5, 5))]),
    # Brushes, shapes, colours, layers
    # Paint shows the brush tip pointing down, with grey bristles.
    Icon("brushes", [fill("paint_brush", GREY, clip=rect(0, 0, 20, 9)), outline("paint_brush")],
         transform="matrix(1 0 0 -1 0 20)"),
    Icon("shapes", [outline("shapes", ACCENT, clip=everything_but(7.5, 7.5, 12.5, 12.5)),
                    outline("shapes", clip=rect(7.5, 7.5, 12.5, 12.5))]),
    Icon("shape-outline", [outline("color_line")]),
    Icon("shape-fill", [outline("color_fill")]),
    Icon("edit-colors", [colour_wheel()]),
    Icon("layers", [outline("layer")]),
    # View and status bar
    Icon("zoom-in", [outline("zoom_in")]),
    Icon("zoom-out", [outline("zoom_out")]),
    Icon("fit-to-window", [outline("arrow_fit")]),
    Icon("rulers", [outline("ruler")]),
    Icon("gridlines", [outline("grid")]),
    Icon("full-screen", [outline("full_screen_maximize")]),
    Icon("thumbnail", [outline("picture_in_picture")]),
    # Layers panel
    Icon("layer-add", [outline("add")]),
    Icon("layer-duplicate", [outline("copy")]),
    Icon("layer-merge-down", [outline("arrow_download")]),
    Icon("layer-visible", [outline("eye")]),
    Icon("layer-hidden", [outline("eye_off")]),
    # File menu
    Icon("import", [outline("image_add")]),
    Icon("print", [outline("print")]),
    Icon("desktop-background", [outline("desktop")]),
    Icon("image-properties", [outline("image_edit")]),
]


def glyph_path(fluent_dir: Path, name: str, variant: str) -> str:
    svg = (fluent_dir / f"{name}_20_{variant}.svg").read_text()
    paths = re.findall(r'<path[^>]*\sd="([^"]+)"', svg)
    if not paths:
        raise SystemExit(f"no path in {name}_20_{variant}.svg")
    return " ".join(paths)


def render(icon: Icon, fluent_dir: Path) -> str:
    defs, body = [], []
    for index, layer in enumerate(icon.layers):
        clip_attr = ""
        if layer.clip:
            clip_id = f"c{index}"
            defs.append(f'<clipPath id="{clip_id}">{layer.clip}</clipPath>')
            clip_attr = f' clip-path="url(#{clip_id})"'
        if layer.kind == "raw":
            body.append(f"<g{clip_attr}>{layer.source}</g>" if clip_attr else layer.source)
            continue
        variant = "regular" if layer.kind == "outline" else "filled"
        d = glyph_path(fluent_dir, layer.source, variant)
        body.append(f'<path d="{d}" fill="{layer.color}"{clip_attr}/>')
    defs_markup = f"<defs>{''.join(defs)}</defs>" if defs else ""
    content = "".join(body)
    if icon.transform:
        content = f'<g transform="{icon.transform}">{content}</g>'
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 20 20">'
            f"{defs_markup}{content}</svg>\n")


def write_preview(path: Path) -> None:
    """Renders all icons at 2x in the light and dark palettes into one sheet for review."""
    with tempfile.TemporaryDirectory() as tmp:
        tiles = []
        for theme, background in (("light", "#f9f9f9"), ("dark", "#262626")):
            for icon in ICONS:
                svg = (OUTPUT / f"{icon.name}.svg").read_text()
                if theme == "dark":
                    for token, colour in DARK_REPLACEMENTS.items():
                        svg = svg.replace(token, colour)
                source = Path(tmp) / f"{theme}-{icon.name}.svg"
                source.write_text(svg)
                png = source.with_suffix(".png")
                subprocess.run(["rsvg-convert", "-w", "40", "-h", "40", "-o", str(png), str(source)], check=True)
                tile = source.with_suffix(".tile.png")
                subprocess.run(["magick", str(png), "-background", background, "-flatten",
                                "-bordercolor", background, "-border", "12",
                                "(", "-size", "64x14", "-background", background,
                                "-fill", "#888888", "-pointsize", "8", f"label:{icon.name}", ")",
                                "-append", str(tile)], check=True)
                tiles.append(str(tile))
        subprocess.run(["magick", "montage", *tiles, "-tile", f"{len(ICONS) // 2 + 1}x",
                        "-geometry", "+0+0", str(path)], check=True)


LICENSE = """\
Icons in this directory are derived from Fluent UI System Icons
(https://github.com/microsoft/fluentui-system-icons, npm @fluentui/svg-icons 1.1.344),
recoloured and composed by tools/icons/build_icons.py.

MIT License

Copyright (c) 2020 Microsoft Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
"""


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--fluent", type=Path, required=True, help="the package/icons directory of @fluentui/svg-icons")
    parser.add_argument("--preview", type=Path, help="write a light/dark preview sheet (needs rsvg-convert, magick)")
    options = parser.parse_args()

    if OUTPUT.exists():
        shutil.rmtree(OUTPUT)
    OUTPUT.mkdir(parents=True)
    for icon in ICONS:
        flattened = SVG.fromstring(render(icon, options.fluent)).topicosvg().tostring(pretty_print=False)
        (OUTPUT / f"{icon.name}.svg").write_text(flattened.replace("<defs/>", "") + "\n")
    (OUTPUT / "LICENSE.txt").write_text(LICENSE)
    print(f"wrote {len(ICONS)} icons to {OUTPUT.relative_to(ROOT)}")

    if options.preview:
        write_preview(options.preview)
        print(f"preview: {options.preview}")


if __name__ == "__main__":
    main()
