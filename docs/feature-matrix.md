# Feature matrix

Every user-facing feature of Windows 11 Paint (as of version 11.2605.x, July 2026), how Paint for
Linux maps it, the milestone that delivers it and its status. This is the checklist for "feature
complete". Items marked *(verify)* come from secondary sources and must be checked against the
reference screenshots in `docs/reference/` before implementing.

Status: `todo` · `in progress` · `done` · `excluded` (AI / Microsoft services) · `adapted` (exists in
a Linux-native form).

## Window and layout

| Feature | Paint | Paint for Linux | M | Status |
|---|---|---|---|---|
| Title | "Untitled - Paint" | "Untitled — Paint for Linux" via Qt display name, `[*]` modified marker | M2 | done |
| Window frame | Windows title bar | System decorations (KWin server-side) | M0 | adapted |
| Menu row | File, Edit, View + Save/Undo/Redo buttons + settings gear | Native `QMenuBar` (works with Plasma global menu): File, Edit, Image, View, Help | M1 | done |
| Quick access | Save, Undo, Redo in the menu row | First ribbon group | M1 | done |
| Ribbon | One row of labelled groups | Custom widget drawn with `QStyle`/`QPalette` | M1 | done |
| Ribbon collapse | Narrow window: shapes gallery → single button, labels shorten *(verify thresholds)* | Shapes → Image → Quick access → Colors collapse into drop-downs as the window narrows | M1 | done |
| Auto-hide toolbar | "Always show" / "Automatically hide" | View → Toolbar submenu + ribbon chevron | M1 | done |
| Workspace | Grey area, centred white canvas, 8 resize handles | Same | M1 | done |
| Left side panel | Floating vertical sliders: size; opacity (pencil, brush); eraser transparency; fill tolerance | Same, overlay cards driving size, opacity, eraser transparency and fill tolerance | M1/M2 | done |
| Layers panel | Right strip, toggled from ribbon | `QDockWidget` on the right; strip UI done, operations in M7 | M7 | in progress |
| Status bar | Cursor pos, selection size, canvas size, file size · fit, zoom %, −, slider, + | `QStatusBar` with the same items | M1/M2 | done |
| Settings page | App theme, AI watermark, About | Theme follows the system; Help → About | — | adapted |
| Key tips | Alt / F10 overlays | Native menu mnemonics | M9 | adapted |
| Theme | Light / Dark / System | System colour scheme (Breeze etc.), icons recoloured for dark | M1 | done |

## File menu

| Feature | Paint | Paint for Linux | M | Status |
|---|---|---|---|---|
| New | Ctrl+N, asks to save changes | Same | M2 | done |
| Open | Ctrl+O, Windows file dialog | Ctrl+O, native/portal `QFileDialog` | M2 | done |
| Import to canvas → From a file | Pastes a file as a floating selection | Same | M3 | done |
| Recent | Recent files submenu | Same (`QSettings`) | M8 | todo |
| Save | Ctrl+S; new images default to PNG | Same | M2 | done |
| Save as → PNG, JPEG, BMP, GIF, Other format | Format submenu; F12 | Same; F12 and Ctrl+Shift+S. PNG, JPEG and 24-bit BMP done, GIF and more in M8 | M2/M8 | in progress |
| Other format | Monochrome/16/256/24-bit BMP, JPEG, GIF, TIFF, PNG, HEIC | Same (HEIC via kimageformats) | M8 | todo |
| Save as project | `.paint`, keeps layers | OpenRaster `.ora` (`.paint` is undocumented) | M7 | adapted |
| Print → Print, Page setup, Print preview | Windows dialogs | `QPrintDialog`, `QPageSetupDialog`, `QPrintPreviewDialog` | M8 | todo |
| Share | Windows share sheet | — | — | excluded |
| Set as desktop background → Fill, Tile, Center | Sets wallpaper | XDG Wallpaper portal; modes pre-rendered to screen size | M8 | todo |
| Image properties | Ctrl+E dialog | Same dialog: file attributes, units, black and white, canvas size, Default | M4 | done |
| Exit | Alt+F4 | Quit, Ctrl+Q | M0 | done |

## Edit menu and clipboard

| Feature | Paint | Paint for Linux | M | Status |
|---|---|---|---|---|
| Undo / Redo | Ctrl+Z / Ctrl+Y | Plus Ctrl+Shift+Z | M2 | done |
| Cut / Copy / Paste | Ctrl+X/C/V; without a selection copies the current layer | Same | M3 | done |
| Copy visible layers | Ctrl+Shift+C | Same (merges what is visible, background included) | M7 | done |
| Paste images from other apps | Any clipboard bitmap | `image/*`, `application/x-qt-image`, `text/uri-list` files, HTML `<img>` data URIs; tested against wl-copy in a headless KWin | M3 | done |
| Paste larger than canvas | Offers to enlarge the canvas | Same | M3 | done |
| Drag and drop into the app | Opens/imports dropped files | Drop on image = import as selection at that point; elsewhere = open; browser image links are downloaded | M3 | done |
| Paste state | — | Paste enabled only when the clipboard holds usable data | M3 | done |
| Extra Edit entries | Only in ribbon/context menu | Native menus also list Paint's ribbon/context-menu commands; Image menu for crop/resize/rotate/flip/invert | M1 | done |

## View menu

| Feature | Paint | Paint for Linux | M | Status |
|---|---|---|---|---|
| Zoom → 100%, in, out, fit | Ctrl+0, Ctrl+PgUp/PgDn, Ctrl+1 | Same + Ctrl+wheel | M2 | done |
| Zoom presets | 12.5–800 %, typed values | Same | M2 | done |
| Rulers | Ctrl+R | Same | M9 | todo |
| Gridlines | Ctrl+G | Same | M9 | todo |
| Status bar | Toggle | Same | M9 | todo |
| Full screen | F11 | Same | M9 | todo |
| Thumbnail | Floating thumbnail window | Same | M9 | todo |

## Selection group

| Feature | Paint | Paint for Linux | M | Status |
|---|---|---|---|---|
| Rectangle / Free-form | Select split button | Same | M3 | done |
| Select all / Invert selection / Delete | Ctrl+A / — / Del | Same | M3 | done |
| Transparent selection | Toggle: colour 2 becomes transparent in the selection | Same | M3 | done |
| Move / resize selection | Handles | Same; moved pixels leave transparency | M3 | done |
| Free rotation | Rotate handle above selection | Rotate handle above the selection; Shift snaps to 15° | M3 | done |
| Nudge | Arrow keys, 1 px | Same | M3 | done |
| Duplicate | Ctrl+drag *(verify)* | Same | M3 | done |
| Crop to selection | Ctrl+Shift+X | Same; free-form crops clear what lies outside the outline | M3 | done |
| Object select | AI | — | — | excluded |

## Image group

| Feature | Paint | Paint for Linux | M | Status |
|---|---|---|---|---|
| Crop | Needs a selection | Same | M3 | todo |
| Resize and skew | Ctrl+W: percentage/pixels, aspect lock, skew degrees | Same dialog; works on the selection when there is one | M4 | done |
| Rotate | Right 90°, left 90°, 180°, Custom rotate… | Same; on the selection when there is one, else the whole image (custom angles grow the canvas) | M4 | done |
| Flip | Vertical, horizontal | Same; on the selection when there is one, else the whole image | M4 | done |
| Invert colors | Ctrl+Shift+I (context menu) | Same + Image menu; the selection, else the current layer | M4 | done |
| Canvas resize | Drag canvas handles | All eight handles, with a dashed preview and the new size in the status bar | M4 | done |
| Remove background | AI | — | — | excluded |

## Tools group

| Feature | Paint | Paint for Linux | M | Status |
|---|---|---|---|---|
| Pencil | 1 px default, opacity slider, circular cursor | Same: hard-edged, opacity without build-up, size outline cursor | M2 | done |
| Fill | Flood fill with tolerance slider | Same: scanline flood fill on the active layer, 0–100 % tolerance | M2 | done |
| Text | Text box + format bar | `TextFormatBar` toolbar | M6 | todo |
| Eraser | Erases to transparency, transparency slider | Same: square, to transparency, strength from the transparency slider | M2 | done |
| Color picker | Left → colour 1, right → colour 2 | Same, then returns to the previous tool | M2 | done |
| Magnifier | Click zooms in, right-click zooms out | Same | M2 | done |
| Tool keys | P, E, B, I, T, Z, S *(verify)* | P, B, T, E, I, Z; S toggles rectangle/free-form selection | M9 | done |
| Generative erase | AI | — | — | excluded |

## Brushes

| Feature | Paint | Paint for Linux | M | Status |
|---|---|---|---|---|
| Brush, Calligraphy brush, Calligraphy pen, Airbrush, Oil brush, Crayon, Marker, Natural pencil, Watercolor brush | Stamp-based, size + opacity | Round brush with opacity done; the other eight engines in M6 | M6 | in progress |

## Shapes

| Feature | Paint | Paint for Linux | M | Status |
|---|---|---|---|---|
| Line, Curve, Oval, Rectangle, Rounded rectangle, Polygon, Triangle, Right triangle, Diamond, Pentagon, Hexagon, 4 arrows, 4/5/6-point stars, 3 callouts, Heart, Lightning | 23 shapes *(verify names)* | Same | M5 | todo |
| Outline / Fill styles | None, Solid, Crayon, Marker, Oil, Natural pencil, Watercolor | Same | M5 | todo |
| Editable until committed | Handles + rotate handle | Same | M5 | todo |
| Shift constraint | Squares/circles, 45° lines | Same | M5 | todo |

## Text tool

| Feature | Paint | Paint for Linux | M | Status |
|---|---|---|---|---|
| Font, size, B/I/U/S, alignment | Floating format bar | Same | M6 | todo |
| Background fill | Checkbox, colour 2 | Same | M6 | todo |
| Rotation | Rotate handle | Same | M6 | todo |

## Colors

| Feature | Paint | Paint for Linux | M | Status |
|---|---|---|---|---|
| Colour 1 / Colour 2 | Two circles | Same | M1/M2 | done |
| Palette | 10×2 presets | Same | M1 | done |
| Custom colours | Row of 10, persisted | Same, remembered between sessions | M4 | done |
| Edit colors dialog | Hue×saturation field, brightness, preview, hex, RGB/HSV, 48 basic, 24 custom, colour names | Same | M4 | done |

## Layers

| Feature | Paint | Paint for Linux | M | Status |
|---|---|---|---|---|
| Add layer | "+", Ctrl+Shift+N | Same | M7 | todo |
| Delete / Duplicate / Hide / Merge down | Layer context menu | Same | M7 | todo |
| Reorder | Drag tiles | Same | M7 | todo |
| Thumbnails, active highlight | Accent border | Accent from system palette | M7 | todo |
| Background tile | Hide (Ctrl+Shift+H), change colour (Ctrl+Shift+B) | Same | M7 | todo |
| Transparency | Checkerboard | Same | M7 | todo |
| Layer opacity / blend modes | Not supported | Not supported | — | n/a |

## Excluded (AI and Microsoft services)

Copilot menu with Cocreator, Image Creator, Generative fill, Generative erase, Object select,
Remove background, Restyle, Sticker generator (and the Stickers group), Coloring book; the AI
watermark setting; Microsoft account / avatar; Share; What's new; Send feedback; session identifier;
scanner import.

## Linux-specific additions

| Feature | Notes | M | Status |
|---|---|---|---|
| "Open with" integration | `.desktop` with image MimeTypes, app ID `pl.mikigal.PaintForLinux` | M8 | in progress |
| Open files from the command line | `paint-for-linux FILE` | M2 | done |
| Extra read formats | WebP, AVIF, JXL, PSD, XCF via kimageformats | M8 | todo |
| Packaging | PKGBUILD (Arch/CachyOS), `cmake --install` | M10 | todo |
