# Design notes

Measurements and observations taken from Windows 11 Paint reference screenshots (Paint 11.2508–11.2601,
100 % scale, light theme unless noted; sources in `docs/reference/SOURCES.md`, images kept locally).

Paint for Linux keeps Paint's **layout, proportions and icons** but takes **colours, fonts, frames and
control styles from the system** (Breeze on KDE). Where Paint uses its accent blue, we use the system
highlight/accent colour; where Paint uses Mica greys, we use the palette's window/base colours.

## Window structure

| Area | Paint | Paint for Linux |
|---|---|---|
| Title bar + menu row | 70 px, `#F3F3F3`; menu row: File, Edit, View │ Save, Share │ Undo, Redo; right: what's new, account, settings | System decorations; native `QMenuBar`; Save/Undo/Redo move to the ribbon |
| Ribbon | 108 px tall, `#F9F9F9`, 1 px `#EBEBEB` lines above and below | Same height and structure, palette `Window`/`Mid` colours |
| Workspace | Grey area with the white canvas | Palette `Dark`-ish workspace, white canvas |
| Status bar | 37 px, `#F3F3F3`, `#EBEBEB` top line, `#E5E5E5` dividers | `QStatusBar` with the same items |

## Ribbon groups (left to right)

| Group | Contents | Width (px) |
|---|---|---|
| Selection | Large dashed-rectangle split button ▾ (Object select is AI: excluded) | 104 |
| Image | 2×2 small: Crop, Rotate ▾ / (Remove background: excluded), Flip ▾; large Resize and skew | 159 |
| Tools | 3×2 small: Pencil, Fill, Text / Eraser, Color picker, Magnifier | 137 |
| Brushes | Large button ▾ | 68 |
| Shapes | Inline 3-row scrollable gallery (171 px box) + Outline ▾, Fill ▾ (+ line width) | 289 |
| Colors | Colour 1/2, 10×2 palette, 10 custom slots, Edit colors | 353 |
| Layers | Large toggle button | ~70 |

Our ribbon starts with a **Quick access** group (Save, Undo, Redo). Stickers and Copilot groups are excluded.
Group labels sit centred under each group; groups are separated by 1 px `#EAEAEA` vertical lines.
In narrow windows the Shapes gallery collapses into one large "Shapes ▾" button, and the Image group
collapses too (thresholds still to be measured).

## Buttons

- Small buttons: 32×32, icon 20 px. Rows of small buttons are ~40 px apart.
- Large buttons: 32×48 (icon on top, ▾ chevron below).
- Selected tool: rounded rectangle (~4 px radius), 1 px accent border, subtle fill.
- Button with an open dropdown: lighter accent border.
- Disabled icons (e.g. Crop without a selection): light grey.
- Dropdown menus mark the current item with a short accent bar on the left.

## Icons

Fluent "regular" line icons (~20 px, `#1A1A1A` light / white dark) with colour accents; see
`tools/icons/build_icons.py`. Measured accents: blue `#005FB8` (rotate/flip wedges, resize, bucket
paint, picker bulb, magnifier plus, shapes circle), pencil `#EECE82`, eraser tip `#F6AEAC`,
fill drop `#FF6327`, brush bristles grey. Dark theme: blue becomes `#4CC2FF`.

## Colours group

- Swatches: 16 px circles on a 24 px pitch, 3 rows × 10; row 3 = empty custom slots (1 px `#A8A8A8` ring).
- Row 1: `#000000 #7F7F7F #880015 #ED1C24 #FF7F27 #FFF200 #22B14C #00A2E8 #3F48CC #A349A4`
- Row 2: `#FFFFFF #C3C3C3 #B97A57 #FFAEC9 #FFC90E #EFE4B0 #B5E61D #99D9EA #7092BE #C8BFE7`
- Colour 1: 24 px disc inside a 32 px accent ring (when active); Colour 2: 24 px disc, grey outline.
- Edit colors: rainbow wheel with a "+" badge.

## Side panels

- Left: floating cards ~40 px wide, ~300 px tall each (size slider; opacity / eraser transparency /
  fill tolerance depending on the tool), `#F9F9F9`.
- Layers (right): ~100 px wide, ~80 px thumbnails, active layer with a 2 px accent border; "+" on top;
  Background tile at the bottom.

## Status bar

Left: cursor position ("394, 124px"), selection size, canvas size ("800 × 512px"), file size.
Right: fit to window, zoom box (~91×29), zoom out, slider (~105 px), zoom in.

## Dark theme (Paint)

Chrome ~`#1F1F1F`, ribbon ~`#262626`, accent light blue `#4CC2FF`. We follow the system dark palette.
