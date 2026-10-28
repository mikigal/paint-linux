#!/usr/bin/env python3
"""Draws the README's screenshots with the real app, in Breeze Dark, into docs/screenshots/.

    tools/readme-screenshots.py            all screenshots (needs the dev build)
    tools/readme-screenshots.py --only main

Each picture is drawn with Paint's own tools through the automation server, in a fresh app on the
offscreen platform (see tests/visual), so nothing appears on the desktop.
"""

from __future__ import annotations

import argparse
import math
import shutil
import sys
import tempfile
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tests/visual"))

from run import capture  # noqa: E402
from scenarios import AWAY, BRUSHES, CHEVRON, Scenario, Step, click, draw, invoke, slider, wave  # noqa: E402

OUTPUT_DIR = ROOT / "docs/screenshots"

# Palette swatches, row by row as in Paint.
BLACK, GRAY, DARK_RED, RED, ORANGE, YELLOW, GREEN, BLUE, INDIGO, PURPLE = range(10)
WHITE, LIGHT_GRAY, BROWN, PINK, GOLD, CREAM, LIME, LIGHT_BLUE, STEEL_BLUE, LAVENDER = range(10, 20)

# The 900×540 picture of the main screenshot sits at this point of the canvas viewport when the
# window is 1280×800 with the Layers panel open.
ORIGIN = (58, 43)


def at(x: int, y: int) -> list[int]:
    """A point of the main screenshot's picture, in canvas viewport coordinates."""
    return [ORIGIN[0] + x, ORIGIN[1] + y]


def colors(outline: int, fill: int) -> list[Step]:
    return [click("ribbon.color2"), click(f"ribbon.palette.{fill}"),
            click("ribbon.color1"), click(f"ribbon.palette.{outline}")]


def shape(name: str, start: list[int], end: list[int], outline: str = "solidColor", fill: str = "solidColor",
          modifiers: str = "", place: bool = True) -> list[Step]:
    # Outline and fill styles can be chosen once a shape tool is active. A shape stays editable,
    # taking up colour changes, until Enter places it.
    drag: dict = {"relativeTo": "canvasView.viewport", "from": start, "to": end}
    if modifiers:
        drag["modifiers"] = modifiers
    steps = [select(f"shape.{name}"), select(f"outline.{outline}"), select(f"fill.{fill}"), ("drag", drag)]
    return steps + [("key", {"keys": "Return"})] if place else steps


def select(action: str) -> Step:
    # Checks a tool or style; triggering one that is already checked would toggle it.
    return ("invoke", {"action": f"action.{action}", "checked": True})


def text_size(points: int) -> list[Step]:
    # Typed into the format bar like a user does; it applies to what is typed next.
    return [click("textFormat.size"), ("key", {"keys": "Ctrl+A"}), ("type", {"text": str(points)}),
            ("key", {"keys": "Return"})]


def fill_at(swatch: int, point: list[int]) -> list[Step]:
    return [invoke("tool.fill"), click(f"ribbon.palette.{swatch}"),
            ("click", {"relativeTo": "canvasView.viewport", "x": point[0], "y": point[1]})]


def arc(cx: int, cy: int, radius: int) -> Step:
    # The upper half of a circle, left to right, in picture coordinates.
    points = [at(cx - int(radius * math.cos(a / 40 * math.pi)), cy - int(radius * math.sin(a / 40 * math.pi)))
              for a in range(41)]
    return draw(points, steps=2)


# Two columns of waves beside the brush menu and a row below it.
BRUSH_WAVES = [(60, 30), (780, 30), (60, 180), (780, 180), (60, 330), (780, 330), (60, 480), (420, 480), (780, 480)]
BRUSH_SWATCHES = [RED, ORANGE, GOLD, GREEN, BLUE, INDIGO, PURPLE, BROWN, STEEL_BLUE]


def pixel_heart() -> Image.Image:
    # A 64×40 picture of a heart, (x² + y² − 1)³ ≤ x²y³, outlined a pixel wide.
    width, height = 64, 40
    inside = {(x, y) for x in range(width) for y in range(height)
              if ((u := (x - 31.5) / 14) ** 2 + (v := (19.5 - y) / 14 + 0.25) ** 2 - 1) ** 3 <= u * u * v ** 3}
    image = Image.new("RGB", (width, height), (153, 217, 234))
    for x, y in inside:
        edge = any((x + dx, y + dy) not in inside for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
        image.putpixel((x, y), (136, 0, 21) if edge else (237, 28, 36))
    for x, y in [(26, 12), (27, 12), (26, 13)]:
        image.putpixel((x, y), (255, 255, 255))
    return image


def main_scene(blank: Path) -> list[Step]:
    hill = [at(x, 400 - int(28 * math.sin(x / 900 * 2.4 * math.pi))) for x in [*range(2, 900, 30), 899]]
    steps: list[Step] = [
        ("call", {"target": "mainWindow", "method": "openFile", "args": [str(blank)]}),
        invoke("view.layersPanel"),
        # Sky and a hill.
        *fill_at(LIGHT_BLUE, at(450, 60)),
        invoke("tool.brush"), slider("size", 6), click(f"ribbon.palette.{GREEN}"), draw(hill, steps=3),
        *fill_at(LIME, at(450, 500)),
        # Sun, tree and house, from Paint's shapes.
        slider("size", 4),
        *colors(ORANGE, YELLOW), *shape("oval", at(50, 40), at(160, 150), modifiers="shift"),
        *colors(BROWN, BROWN), *shape("rectangle", at(112, 330), at(136, 440)),
        *colors(GREEN, GREEN), *shape("oval", at(55, 220), at(195, 350), fill="watercolor"),
        *colors(BROWN, CREAM), *shape("rectangle", at(240, 270), at(430, 420)),
        *colors(DARK_RED, RED), *shape("triangle", at(215, 165), at(455, 270)),
        *colors(BROWN, BROWN), *shape("rectangle", at(315, 340), at(360, 420)),
        *colors(STEEL_BLUE, LIGHT_BLUE), *shape("rectangle", at(262, 295), at(302, 335)),
        *shape("rectangle", at(372, 295), at(412, 335)),
        # A rainbow on a layer of its own.
        invoke("layer.add"), invoke("tool.brush"), invoke("brush.brush"), slider("size", 14),
    ]
    for i, swatch in enumerate([RED, ORANGE, YELLOW, GREEN, BLUE, PURPLE]):
        steps += [click(f"ribbon.palette.{swatch}"), arc(680, 440, 185 - i * 13)]
    # A caption on a third layer, left in the text box so the format bar shows.
    steps += [
        invoke("layer.add"), invoke("tool.text"), click("ribbon.color1"), click(f"ribbon.palette.{INDIGO}"),
        *text_size(28),
        ("drag", {"relativeTo": "canvasView.viewport", "from": at(470, 40), "to": at(880, 110)}),
        ("key", {"keys": "Ctrl+B"}), ("type", {"text": "Hello from Linux!"}),
        invoke("tool.pencil"), AWAY, ("wait", {"ms": 500}),
    ]
    return steps


def screenshots(blank: Path, heart: Path) -> list[Scenario]:
    return [
        Scenario("main", theme="dark", steps=main_scene(blank)),
        # Every brush draws the same wave around the open brush menu.
        Scenario("brushes", theme="dark", steps=[
            invoke("tool.brush"), slider("size", 16),
            *[step for (brush, swatch, (left, top)) in zip(BRUSHES, BRUSH_SWATCHES, BRUSH_WAVES)
              for step in (invoke(f"brush.{brush}"), click(f"ribbon.palette.{swatch}"), wave(left, top))],
            AWAY, click("ribbon.brushes", **CHEVRON)]),
        # Outline and fill styles; the star is still being edited, with its frame and rotate handle.
        Scenario("shapes", theme="dark", steps=[
            slider("size", 12),
            *colors(INDIGO, LIGHT_BLUE), *shape("oval", [80, 70], [340, 280], "crayon", "watercolor"),
            *colors(DARK_RED, PINK), *shape("heart", [420, 70], [660, 290], "marker", "oil"),
            *colors(PURPLE, LAVENDER), *shape("cloudCallout", [140, 350], [500, 570], "oil", "crayon"),
            *colors(ORANGE, YELLOW), *shape("lightning", [590, 350], [790, 590], "watercolor", "marker"),
            *colors(GREEN, LIME), *shape("fivePointStar", [780, 70], [1060, 330], "naturalPencil", place=False),
            ("drag", {"relativeTo": "canvasView.viewport", "from": [920, 46], "to": [990, 60], "modifiers": "shift"}),
            AWAY]),
        # Text with the format bar, rotated by its handle.
        Scenario("text", theme="dark", steps=[
            invoke("tool.text"), *colors(WHITE, BLUE), invoke("text.background"),
            *text_size(26),
            ("drag", {"relativeTo": "canvasView.viewport", "from": [260, 190], "to": [820, 260]}),
            ("type", {"text": "Plain, "}), ("key", {"keys": "Ctrl+B"}), ("type", {"text": "bold, "}),
            ("key", {"keys": "Ctrl+B,Ctrl+I"}), ("type", {"text": "italic "}), ("key", {"keys": "Ctrl+I,Ctrl+U"}),
            ("type", {"text": "and underlined"}), ("key", {"keys": "Ctrl+U"}),
            # The rotate handle sits 24 px above the middle of the top edge; Shift turns in 15° steps.
            ("drag", {"relativeTo": "canvasView.viewport", "from": [540, 166], "to": [556, 168], "modifiers": "shift"}),
            AWAY]),
        # A pixel heart at 800 % with rulers and gridlines.
        Scenario("pixels", theme="dark", steps=[
            ("call", {"target": "mainWindow", "method": "openFile", "args": [str(heart)]}),
            invoke("view.rulers"), invoke("view.gridlines"), *[invoke("view.zoomIn")] * 7,
            ("mouse", {"action": "move", "relativeTo": "canvasView.viewport", "x": 600, "y": 260})]),
    ]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--only", default="", help="substring filter on screenshot names")
    parser.add_argument("--build", type=Path, default=ROOT / "build/dev", help="build directory")
    options = parser.parse_args()

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as scratch:
        blank = Path(scratch) / "picture.png"
        Image.new("RGB", (900, 540), "white").save(blank)
        heart = Path(scratch) / "heart.png"
        pixel_heart().save(heart)
        for scenario in screenshots(blank, heart):
            if options.only in scenario.name:
                shutil.copyfile(capture(scenario, options.build), OUTPUT_DIR / f"{scenario.name}.png")
                print(OUTPUT_DIR / f"{scenario.name}.png")
    return 0


if __name__ == "__main__":
    sys.exit(main())
