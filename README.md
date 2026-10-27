# Paint for Linux

A native Linux take on the Windows 11 Paint app, built with Qt 6 Widgets and C++20. It keeps Paint's
icons, layout and workflow, but looks and behaves like a Linux desktop application: system menu bar
and window decorations, native dialogs, and the system colour scheme. AI features and Microsoft
services are out of scope. See [`docs/feature-matrix.md`](docs/feature-matrix.md) for the full
feature list and progress.

App ID: `pl.mikigal.PaintForLinux` · executable: `paint-for-linux`

## Building

Requirements: CMake ≥ 3.25, Ninja, a C++20 compiler and Qt ≥ 6.8 (Core, Gui, Widgets, Svg; Network
and Test for development builds). Optional at runtime: KDE's `kimageformats` for HEIC/AVIF/JXL.

```bash
cmake --preset dev            # Debug, tests and automation server enabled
cmake --build --preset dev
./build/dev/src/app/paint-for-linux
```

Presets: `dev`, `asan` (AddressSanitizer + UBSan) and `release` (no tests, no automation).

`tools/install-dev-desktop.sh` registers the development build with your desktop (a
`pl.mikigal.PaintForLinux.desktop` entry and icon in `~/.local/share`), so the window gets its
name and icon and xdg-desktop-portal recognises the app ID; `--uninstall` removes them.

## Testing

```bash
tools/verify.sh               # build, tests, formatting, visual regression — run before committing
tools/verify.sh --tidy --asan # plus clang-tidy and a sanitizer build
tools/verify.sh --integration # plus cross-application tests in a headless KWin (needs wl-clipboard)
```

- **Unit and widget tests** (`ctest --preset dev`) run headless on Qt's offscreen platform with a
  checked-in, deterministic KDE environment (`tests/env`: Breeze Light/Dark, Noto Sans 10), so the
  developer's desktop settings never affect results.
- **Golden images**: `PAINT_COMPARE_GOLDEN(image, "name")` compares against `tests/golden/name.png`;
  failures write `*.actual.png` and `*.diff.png` to `build/dev/test-output/`. Accept intended changes
  with `PAINT_UPDATE_GOLDEN=1 ctest --preset dev -R <test>`.
- **Integration**: `tests/integration/*.py` run the app in a headless KWin and exchange images
  with other Wayland clients (`wl-copy`/`wl-paste`) through the real clipboard.
- **Visual regression**: `tests/visual/run.py` drives the real app through scenarios
  (`tests/visual/scenarios.py`) and compares screenshots with `tests/visual/baselines/`; review the
  output and run `tests/visual/run.py --update` to accept changes.

## Driving the app (automation)

Development builds contain an automation server (never in release builds). Start the app in an
isolated session and control it with `paintctl`:

```bash
tools/headless-session.sh start --theme dark            # offscreen; or --platform kwin
tools/paintctl.py tree                                    # widgets, object names, actions
tools/paintctl.py click target=menu.file
tools/paintctl.py screenshot path=/tmp/shot.png
tools/paintctl.py drag relativeTo=canvasView.viewport path='[[100,100],[300,200]]'   # draw
tools/paintctl.py call target=mainWindow method=openFile args='["image.png"]'
tools/paintctl.py call target=mainWindow method=documentPixel args='[10,10]'
tools/paintctl.py drop target=canvasView urls='["image.png"]'
tools/headless-session.sh stop
```

`--platform kwin` runs a headless KWin on a private D-Bus session (useful for cross-application
clipboard tests with `wl-copy`); popups only open on the offscreen platform because synthetic input
has no Wayland serial.

## Layout

```
src/core/      image model and algorithms (QtGui only, fully unit-tested)
src/app/       widgets, main window, automation server
tests/         unit/widget tests, golden images, visual scenarios, deterministic environment
tools/         paintctl.py, headless-session.sh, verify.sh
docs/          feature matrix and design notes (reference screenshots stay local)
```
