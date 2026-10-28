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
| Layers panel | Right strip, toggled from ribbon | `QDockWidget` on the right with every layer operation | M7 | done |
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
| Recent | Recent files submenu | Same: the last 10 opened or saved, newest first, missing files dropped; Clear list | M8 | done |
| Save | Ctrl+S; new images default to PNG | Same | M2 | done |
| Save as → PNG, JPEG, BMP, GIF, Other format | Format submenu; F12 | Same; F12 and Ctrl+Shift+S. Save keeps the format chosen last (e.g. a 256-colour bitmap); files are replaced only once completely written | M2/M8 | done |
| Other format | Monochrome/16/256/24-bit BMP, JPEG, GIF, TIFF, PNG, HEIC | Same: our own BMP (1/4/8/24-bit, Windows 16-colour palette) and GIF encoders, TIFF with LZW, HEIC when kimageformats can encode it; palette formats warn about color loss first | M8 | done |
| Save as project | `.paint`, keeps layers | OpenRaster `.ora` (`.paint` is undocumented): layers, names, visibility, the active layer, the background and DPI; GIMP, Krita and MyPaint open it. Any OpenRaster file opens, nested stacks, offsets and opacity baked into layers | M7 | adapted |
| Print → Print, Page setup, Print preview | Windows dialogs | `QPrintDialog`, `QPageSetupDialog`, `QPrintPreviewDialog`; the picture prints at its own DPI from the top-left, shrunk to fit; page setup lasts the session | M8 | done |
| Share | Windows share sheet | — | — | excluded |
| Set as desktop background → Fill, Tile, Center | Sets wallpaper | XDG Wallpaper portal (KDE, GNOME…); modes pre-rendered to the screen's size; tested against a fake portal on a private bus | M8 | done |
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
| Rulers | Ctrl+R | Same: image pixels from the picture's corner, following zoom and scrolling, the pointer marked | M9 | done |
| Gridlines | Ctrl+G | Same; at least 8 screen pixels apart, so up close they outline every pixel | M9 | done |
| Status bar | Toggle | Same | M9 | todo |
| Full screen | F11 | Same: the picture alone on black until a key or click | M9 | done |
| Thumbnail | Floating thumbnail window | A card in the canvas's top-right corner (Wayland does not let apps place windows); outlines what is in view, a click moves the view | M9 | adapted |

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
| Text | Text box + format bar | Text box on the canvas with a native format bar under the ribbon while the tool is active | M6 | done |
| Eraser | Erases to transparency, transparency slider | Same: square, to transparency, strength from the transparency slider | M2 | done |
| Color picker | Left → colour 1, right → colour 2 | Same, then returns to the previous tool | M2 | done |
| Magnifier | Click zooms in, right-click zooms out | Same | M2 | done |
| Tool keys | P, E, B, I, T, Z, S *(verify)* | P, B, T, E, I, Z; S toggles rectangle/free-form selection | M9 | done |
| Generative erase | AI | — | — | excluded |

## Brushes

| Feature | Paint | Paint for Linux | M | Status |
|---|---|---|---|---|
| Brush, Calligraphy brush, Calligraphy pen, Airbrush, Oil brush, Crayon, Marker, Natural pencil, Watercolor brush | Stamp-based, size + opacity | All nine: round brush; slanted nibs for the calligraphy brush and pen; an airbrush that also sprays while the pointer rests; oil bristles in varied shades; crayon, marker, natural pencil and watercolour with paper grain fixed to the document. Opacity without build-up within a stroke; random parts seeded per stroke | M6 | done |

## Shapes

| Feature | Paint | Paint for Linux | M | Status |
|---|---|---|---|---|
| Line, Curve, Oval, Rectangle, Rounded rectangle, Polygon, Triangle, Right triangle, Diamond, Pentagon, Hexagon, 4 arrows, 4/5/6-point stars, 3 callouts, Heart, Lightning | 23 shapes *(verify names)* | Same; the outline is colour 1 and the fill colour 2 (swapped when drawn with the right button); the outline grows inside the dragged box | M5 | done |
| Curve and polygon | Curve: line, then two bends; polygon: click corners | Same; a polygon closes on its first corner, a double click or Enter | M5 | done |
| Outline / Fill styles | None, Solid, Crayon, Marker, Oil, Natural pencil, Watercolor | Same textures as the brushes, moving with the shape; oil outlines are painted with the oil brush's bristles | M5/M6 | done |
| Editable until committed | Handles + rotate handle | Same: move, resize and rotate with the frame, live restyling from colours, size and styles; arrow keys nudge, Enter/Esc place, Delete discards, undo drops the shape; lines get end handles | M5 | done |
| Shift constraint | Squares/circles, 45° lines | Same, also for resizing corners and rotating in 15° steps | M5 | done |

## Text tool

| Feature | Paint | Paint for Linux | M | Status |
|---|---|---|---|---|
| Font, size, B/I/U/S, alignment | Floating format bar | A native tool bar under the ribbon (Ctrl+B/I/U); formats the selected text, or what is typed next | M6 | done |
| Text box | Drag or click, text in colour 1, wraps and grows | Same; the border moves the box, handles resize it; full keyboard and mouse editing, clipboard, undo of typing, input methods; tool shortcuts do not fire while typing | M6 | done |
| Background fill | Checkbox, colour 2 | Same ("Fill background") | M6 | done |
| Rotation | Rotate handle | Same; Shift turns in 15° steps | M6 | done |

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
| Add layer | "+", Ctrl+Shift+N | Same: above the active layer, which it replaces as active | M7 | done |
| Delete / Duplicate / Hide / Merge down | Layer context menu | Same, also in the Layers menu; each one undo step | M7 | done |
| Reorder | Drag tiles | Same, with a drop marker | M7 | done |
| Thumbnails, active highlight | Accent border | Accent from system palette; hidden layers fade and show a crossed eye, the eye toggles them | M7 | done |
| Background tile | Hide (Ctrl+Shift+H), change colour (Ctrl+Shift+B) | Same; a click on the tile changes the colour | M7 | done |
| Transparency | Checkerboard | Same, on the canvas and the tiles | M7 | done |
| Layer opacity / blend modes | Not supported | Not supported | — | n/a |

## Excluded (AI and Microsoft services)

Copilot menu with Cocreator, Image Creator, Generative fill, Generative erase, Object select,
Remove background, Restyle, Sticker generator (and the Stickers group), Coloring book; the AI
watermark setting; Microsoft account / avatar; Share; What's new; Send feedback; session identifier;
scanner import.

## Linux-specific additions

| Feature | Notes | M | Status |
|---|---|---|---|
| "Open with" integration | `.desktop` with image MimeTypes (incl. OpenRaster), app ID `pl.mikigal.PaintForLinux`; file:// URLs; several files open one window each | M8 | done |
| Open files from the command line | `paint-for-linux FILE` | M2 | done |
| Extra read formats | WebP, AVIF, JXL, PSD, XCF, RAW and more via kimageformats when installed | M8 | done |
| Packaging | PKGBUILD (Arch/CachyOS) and `cmake --install`: executable, `.desktop` entry, AppStream metadata (validated), icon; no RPATH | M10 | done |
| Large pictures | Benchmarked on 4K with three layers (`ctest --preset bench`); undo history capped at 100 steps and 1 GiB | M10 | done |
| Keyboard and screen readers | Every control has an accessible name (icon-only ones from their tooltip, swatches with their colour); Tab reaches the ribbon, palette and status bar; Space works the focused control | M9 | done |
