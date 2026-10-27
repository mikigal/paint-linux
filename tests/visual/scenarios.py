"""Visual scenarios: each one starts a fresh app, runs automation steps and takes one screenshot.

Steps are (command, arguments) pairs for the automation server (see `paintctl.py help`).
Screenshots are compared against tests/visual/baselines/<name>.png by run.py.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Any

Step = tuple[str, dict[str, Any]]


@dataclass(frozen=True)
class Scenario:
    name: str
    theme: str = "light"
    size: tuple[int, int] = (1280, 800)
    steps: list[Step] = field(default_factory=list)
    target: str | None = None  # screenshot target; defaults to the main window with popups


def click(target: str, **extra: Any) -> Step:
    return ("click", {"target": target, **extra})


def invoke(action: str) -> Step:
    return ("invoke", {"action": f"action.{action}"})


def draw(path: list[list[int]], steps: int = 10) -> Step:
    # A polyline drag on the canvas; segments are interpolated so strokes look hand-drawn.
    points: list[list[int]] = [path[0]]
    for start, end in zip(path, path[1:]):
        for i in range(1, steps + 1):
            points.append([start[0] + (end[0] - start[0]) * i // steps, start[1] + (end[1] - start[1]) * i // steps])
    return ("drag", {"relativeTo": "canvasView.viewport", "path": points})


def slider(name: str, value: int) -> Step:
    return ("property", {"target": f"sidePanel.{name}.slider", "name": "value", "value": value})


# Large split buttons open their menu from the chevron in their lower part.
CHEVRON = {"at": [22, 53]}

# A small picture to select from: blue background with a thick red V.
PICTURE: list[Step] = [
    ("invoke", {"action": "action.tool.fill"}), ("click", {"target": "ribbon.palette.7"}),
    ("click", {"relativeTo": "canvasView.viewport", "x": 900, "y": 500}),
    ("invoke", {"action": "action.tool.brush"}), ("click", {"target": "ribbon.palette.3"}),
    ("property", {"target": "sidePanel.size.slider", "name": "value", "value": 25}),
]
PICTURE.append(draw([[200, 150], [350, 300], [500, 150]], steps=20))
BRUSHES = ["brush", "calligraphyBrush", "calligraphyPen", "airbrush", "oilBrush", "crayon", "marker",
           "naturalPencil", "watercolorBrush"]


def wave(left: int, top: int) -> Step:
    # One period of a sine-like wave, 280 px wide, in canvas viewport coordinates.
    points = [[left + x, top + 50 - int(45 * math.sin(x / 280 * 2 * math.pi))]
              for x in range(0, 281, 8)]
    return draw(points, steps=2)


# Moves the pointer off the canvas so hover effects do not depend on timing.
AWAY: Step = ("mouse", {"action": "move", "x": 1200, "y": 700})

SCENARIOS: list[Scenario] = [
    # Whole window
    Scenario("main-window-light"),
    Scenario("main-window-dark", theme="dark"),
    Scenario("main-window-paint-size-light", size=(1442, 910)),
    Scenario("narrow-950-light", size=(950, 700)),
    Scenario("narrow-800-dark", theme="dark", size=(800, 700)),
    Scenario("narrow-minimum-light", size=(600, 600)),
    # Menu bar
    Scenario("menu-file-light", steps=[click("menu.file")]),
    Scenario("menu-edit-dark", theme="dark", steps=[click("menu.edit")]),
    Scenario("menu-image-light", steps=[click("menu.image")]),
    Scenario("menu-view-light", steps=[click("menu.view")]),
    Scenario("menu-help-dark", theme="dark", steps=[click("menu.help")]),
    # Ribbon drop-downs
    Scenario("dropdown-select-light", steps=[click("ribbon.select", **CHEVRON)]),
    Scenario("dropdown-rotate-light", steps=[click("ribbon.rotate")]),
    Scenario("dropdown-flip-dark", theme="dark", steps=[click("ribbon.flip")]),
    Scenario("dropdown-brushes-light", steps=[click("ribbon.brushes", **CHEVRON)]),
    Scenario("dropdown-brushes-dark", theme="dark", steps=[click("ribbon.brushes", **CHEVRON)]),
    Scenario("dropdown-outline-light", steps=[invoke("shape.heart"), click("ribbon.outline")]),
    Scenario("dropdown-shape-fill-dark", theme="dark", steps=[invoke("shape.oval"), click("ribbon.shapeFill")]),
    Scenario("dropdown-shapes-compact-light", size=(1000, 700), steps=[click("ribbon.shapesCompact")]),
    Scenario("dropdown-colors-compact-light", size=(700, 600), steps=[click("ribbon.colorsCompact")]),
    Scenario("dropdown-toolbar-options-light", steps=[click("ribbon.toolbarOptions")]),
    # Tool states
    Scenario("tool-shape-selected-light", steps=[invoke("shape.fivePointStar")]),
    Scenario("tool-fill-tolerance-light", steps=[invoke("tool.fill")]),
    Scenario("tool-text-no-panel-dark", theme="dark", steps=[invoke("tool.text")]),
    Scenario("colors-secondary-active-light", steps=[click("ribbon.color2"), click("ribbon.palette.7")]),
    # Panels and modes
    Scenario("layers-panel-light", steps=[invoke("view.layersPanel")]),
    Scenario("layers-panel-dark", theme="dark", steps=[invoke("view.layersPanel")]),
    Scenario("autohide-strip-light", steps=[invoke("view.toolbarAutoHide")]),
    # The pointer is moved off the dropped-down ribbon so no hover effect depends on timing.
    Scenario("autohide-open-light", steps=[invoke("view.toolbarAutoHide"), click("ribbon.showToolbar"),
                                           ("mouse", {"action": "move", "x": 640, "y": 600})]),
    Scenario("context-menu-light",
             steps=[invoke("tool.rectangleSelect"), ("click", {"x": 400, "y": 400, "button": "right"})]),
    Scenario("zoom-200-light", steps=[invoke("view.zoomIn")]),
    Scenario("zoom-fit-dark", theme="dark", steps=[invoke("view.zoomFit")]),
    # Drawing (paths are in canvas viewport coordinates)
    Scenario("draw-pencil-light", steps=[draw([[150, 150], [300, 300], [450, 150], [600, 300], [750, 150]])]),
    Scenario("draw-brush-opacity-light", steps=[
        click("ribbon.palette.3"), invoke("tool.brush"), slider("size", 18), slider("opacity", 50),
        draw([[150, 200], [700, 380], [150, 380], [700, 200]], steps=40),
    ]),
    Scenario("draw-fill-dark", theme="dark", steps=[
        slider("size", 3), draw([[200, 150], [500, 150], [500, 400], [200, 400], [200, 150]]),
        invoke("tool.fill"), click("ribbon.palette.7"), ("click", {"relativeTo": "canvasView.viewport", "x": 350,
                                                                   "y": 280}),
    ]),
    Scenario("draw-eraser-outline-light", steps=[
        invoke("tool.fill"), click("ribbon.palette.8"), ("click", {"x": 600, "y": 400}),
        invoke("tool.eraser"), slider("size", 40), draw([[300, 250], [800, 450]], steps=30),
        ("mouse", {"action": "move", "x": 700, "y": 300}),
    ]),
    Scenario("draw-zoom-400-light", steps=[
        draw([[625, 305], [640, 311], [652, 324]]), invoke("view.zoomIn"), invoke("view.zoomIn"),
        invoke("view.zoomIn"),
    ]),
    # Selections (on a picture: a red V on blue)
    Scenario("select-rectangle-light", steps=[*PICTURE, invoke("tool.rectangleSelect"),
                                              ("drag", {"relativeTo": "canvasView.viewport", "from": [180, 120],
                                                        "to": [520, 320]}), AWAY]),
    Scenario("select-moved-light", steps=[*PICTURE, invoke("tool.rectangleSelect"),
                                          ("drag", {"relativeTo": "canvasView.viewport", "from": [180, 120],
                                                    "to": [520, 320]}),
                                          ("drag", {"relativeTo": "canvasView.viewport", "from": [350, 220],
                                                    "to": [650, 300]}), AWAY]),
    Scenario("select-rotated-dark", theme="dark", steps=[
        *PICTURE, invoke("tool.rectangleSelect"),
        ("drag", {"relativeTo": "canvasView.viewport", "from": [180, 120], "to": [520, 320]}),
        # The rotate handle sits 24 px above the middle of the top edge.
        ("drag", {"relativeTo": "canvasView.viewport", "from": [350, 96], "to": [560, 150], "modifiers": "shift"}),
        AWAY]),
    Scenario("select-freeform-light", steps=[*PICTURE, invoke("tool.freeformSelect"),
                                             draw([[200, 150], [450, 110], [560, 300], [300, 340], [200, 150]])]),
    Scenario("context-menu-selection-light", steps=[
        *PICTURE, invoke("tool.rectangleSelect"),
        ("drag", {"relativeTo": "canvasView.viewport", "from": [180, 120], "to": [520, 320]}),
        ("click", {"relativeTo": "canvasView.viewport", "x": 350, "y": 220, "button": "right"})]),
    Scenario("drop-image-light", steps=[
        ("drop", {"relativeTo": "canvasView.viewport", "x": 300, "y": 200,
                  "imagePath": "tests/golden/document/composite-two-layers.png"}), AWAY]),
    # Shapes: outline in colour 1, fill in colour 2; the last shape drawn stays editable.
    Scenario("shape-editing-light", steps=[
        invoke("shape.rectangle"), slider("size", 5), click("ribbon.palette.3"),
        ("drag", {"relativeTo": "canvasView.viewport", "from": [200, 150], "to": [500, 350]}), AWAY]),
    Scenario("shape-styles-light", steps=[
        click("ribbon.color2"), click("ribbon.palette.14"), click("ribbon.color1"), click("ribbon.palette.8"),
        invoke("shape.oval"), slider("size", 12), invoke("outline.crayon"), invoke("fill.watercolor"),
        ("drag", {"relativeTo": "canvasView.viewport", "from": [60, 60], "to": [320, 260]}),
        invoke("shape.heart"), invoke("outline.marker"), invoke("fill.oil"),
        ("drag", {"relativeTo": "canvasView.viewport", "from": [380, 60], "to": [620, 280]}),
        invoke("shape.fivePointStar"), invoke("outline.naturalPencil"), invoke("fill.solidColor"),
        ("drag", {"relativeTo": "canvasView.viewport", "from": [680, 50], "to": [940, 300]}),
        invoke("shape.cloudCallout"), invoke("outline.oil"), invoke("fill.crayon"),
        ("drag", {"relativeTo": "canvasView.viewport", "from": [100, 340], "to": [460, 580]}),
        invoke("shape.lightning"), invoke("outline.watercolor"), invoke("fill.marker"),
        ("drag", {"relativeTo": "canvasView.viewport", "from": [560, 340], "to": [760, 590]}),
        invoke("tool.pencil"), AWAY]),
    Scenario("shape-rotated-dark", theme="dark", steps=[
        invoke("shape.rightArrow"), click("ribbon.color2"), click("ribbon.palette.5"), click("ribbon.color1"),
        click("ribbon.palette.6"), invoke("fill.solidColor"),
        ("drag", {"relativeTo": "canvasView.viewport", "from": [250, 200], "to": [550, 350]}),
        # The rotate handle sits 24 px above the middle of the top edge.
        ("drag", {"relativeTo": "canvasView.viewport", "from": [400, 176], "to": [560, 230], "modifiers": "shift"}),
        AWAY]),
    Scenario("shape-line-light", steps=[
        invoke("shape.line"), slider("size", 8), click("ribbon.palette.4"),
        ("drag", {"relativeTo": "canvasView.viewport", "from": [150, 400], "to": [600, 150]}), AWAY]),
    Scenario("shape-polygon-curve-light", steps=[
        invoke("shape.polygon"), slider("size", 4), click("ribbon.palette.8"),
        ("drag", {"relativeTo": "canvasView.viewport", "from": [150, 300], "to": [300, 120]}),
        ("click", {"relativeTo": "canvasView.viewport", "x": 420, "y": 330}),
        ("click", {"relativeTo": "canvasView.viewport", "x": 150, "y": 300}),
        invoke("shape.curve"),
        ("drag", {"relativeTo": "canvasView.viewport", "from": [600, 350], "to": [950, 350]}),
        ("drag", {"relativeTo": "canvasView.viewport", "from": [700, 350], "to": [700, 150]}),
        ("drag", {"relativeTo": "canvasView.viewport", "from": [880, 350], "to": [880, 480]}), AWAY]),
    # Brushes: every brush type draws the same wave.
    Scenario("brushes-all-light", steps=[
        click("ribbon.palette.8"), invoke("tool.brush"), slider("size", 16),
        *[step for i, brush in enumerate(BRUSHES)
          for step in (invoke(f"brush.{brush}"), wave(80 + (i % 3) * 360, 40 + (i // 3) * 190))],
        AWAY]),
    # Text: the format bar shows under the ribbon while the text tool is active.
    Scenario("text-editing-light", steps=[
        invoke("tool.text"),
        ("drag", {"relativeTo": "canvasView.viewport", "from": [150, 120], "to": [560, 200]}),
        ("type", {"text": "Hello from Paint for Linux, typed into a wrapping text box"}),
        ("key", {"keys": "Ctrl+Shift+Left,Ctrl+Shift+Left"}), AWAY]),
    Scenario("text-formatted-dark", theme="dark", steps=[
        invoke("tool.text"), click("ribbon.color2"), click("ribbon.palette.5"), click("ribbon.color1"),
        click("ribbon.palette.2"), invoke("text.background"),
        ("drag", {"relativeTo": "canvasView.viewport", "from": [300, 200], "to": [700, 290]}),
        ("type", {"text": "Plain "}), ("key", {"keys": "Ctrl+B"}), ("type", {"text": "bold "}),
        ("key", {"keys": "Ctrl+B,Ctrl+I"}), ("type", {"text": "italic "}), ("key", {"keys": "Ctrl+I,Ctrl+U"}),
        ("type", {"text": "underlined"}), ("key", {"keys": "Ctrl+U"}),
        # The rotate handle sits 24 px above the middle of the top edge.
        ("drag", {"relativeTo": "canvasView.viewport", "from": [500, 176], "to": [700, 120], "modifiers": "shift"}),
        AWAY]),
    # Image commands
    Scenario("image-rotated-right-light", steps=[*PICTURE, invoke("image.rotateRight"), AWAY]),
    Scenario("image-inverted-dark", theme="dark", steps=[*PICTURE, invoke("image.invertColors"), AWAY]),
    Scenario("selection-flipped-light", steps=[
        *PICTURE, invoke("tool.rectangleSelect"),
        ("drag", {"relativeTo": "canvasView.viewport", "from": [180, 120], "to": [520, 320]}),
        invoke("image.flipVertical"), AWAY]),
    # Dragging the canvas's bottom-right handle shows the new size before it is applied. At 50 %
    # the 1152×648 canvas sits centred in the 1280×626 viewport, its corner handle at (932, 479).
    Scenario("canvas-resize-preview-light", steps=[
        invoke("view.zoomOut"), invoke("view.zoomOut"),
        ("mouse", {"action": "press", "relativeTo": "canvasView.viewport", "x": 932, "y": 479}),
        ("mouse", {"action": "move", "relativeTo": "canvasView.viewport", "x": 1100, "y": 560}),
    ]),
    # Dialogs
    Scenario("dialog-about-light", steps=[invoke("help.about")], target="active"),
    Scenario("dialog-resize-skew-light", steps=[invoke("image.resizeSkew")], target="active"),
    Scenario("dialog-resize-skew-dark", theme="dark", steps=[invoke("image.resizeSkew")], target="active"),
    Scenario("dialog-custom-rotate-light", steps=[invoke("image.rotateCustom")], target="active"),
    Scenario("dialog-image-properties-light", steps=[invoke("file.imageProperties")], target="active"),
    Scenario("dialog-image-properties-dark", theme="dark", steps=[invoke("file.imageProperties")],
             target="active"),
    Scenario("dialog-edit-colors-light", steps=[click("ribbon.palette.4"), invoke("color.edit")], target="active"),
    Scenario("dialog-edit-colors-dark", theme="dark", steps=[click("ribbon.palette.8"), invoke("color.edit")],
             target="active"),
    Scenario("dialog-save-prompt-light", steps=[draw([[100, 100], [200, 200]]), invoke("file.new")],
             target="active"),
]
