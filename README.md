# spritesheet

A small desktop tool for packing PNG sprites into a single sprite sheet and
exporting a JSON manifest. Built with SDL2 and [Dear ImGui](https://github.com/ocornut/imgui).

It supports both single-image sprites and source images that are themselves a
grid of frames (`cols x rows`), packs everything onto a per-sprite-size grid,
and lets you hand-place individual frames when you want manual control.

## Features

- Add PNG sprites (menu, or drag-and-drop a file onto the window).
- Treat a source image as a `cols x rows` frame grid; each frame is packed and
  addressed individually.
- **Auto-pack**: a backfilling 2D packer that reuses negative space. Every frame
  lands on a slot aligned to its own size, largest-first, within `Max sheet width`.
- **Manual placement**: drag a sprite from the list onto the canvas, then drag any
  individual frame to reposition it. Movement snaps to that sprite's frame grid,
  and the selected sprite's grid is overlaid on the canvas.
- **Lock** sprites so auto-pack treats them as fixed obstacles.
- **Per-axis sheet sizing** (width and height independently):
  - `Shared edges` — round up so every sprite's grid meets the edge (LCM of frame sizes).
  - `Tight` — round content up to the base unit.
  - `Manual` — an explicit size (clamped to at least the content extent).
  - Optional power-of-two rounding on top.
- Export a packed PNG plus a JSON manifest of frame rectangles.
- Recently-opened project list and native-feeling in-app file dialogs.

## Dependencies

- A C++17 compiler (`g++`) and `make`.
- **SDL2** (with `sdl2-config` on `PATH`).
- **nlohmann/json** development headers (e.g. `/usr/include/nlohmann/json.hpp`).

Vendored under `third_party/` (no action needed):

- Dear ImGui (+ SDL2 / SDL_Renderer2 backends)
- stb_image / stb_image_write
- ImGuiFileDialog

## Build & run

```sh
make            # produces ./spritesheet
./spritesheet                       # start empty
./spritesheet path/to/project.json  # open a project on launch
make clean
```

## Usage

- **File**: New (Ctrl+N), Open (Ctrl+O), Open Recent, Save (Ctrl+S),
  Save As (Ctrl+Shift+S), Export PNG + Manifest (Ctrl+E), Quit (Ctrl+Q).
- **Sprite**: Add PNG (Ctrl+A), Auto-pack all (Ctrl+P).
- **Canvas**: drag from the Sprites panel to place; drag any placed frame to move
  it on its grid; click to select. The bright outline marks the exact exported
  sheet bounds.
- **Sprites panel**: right-click a sprite for Unplace / Toggle lock / Remove.
- **Inspector** (no sprite selected) holds project settings: base unit,
  max sheet width, POT rounding, per-axis width/height fit, and output paths.
  Select a sprite to edit its id, frame grid, and placement.

## Project file

The editor reads/writes a JSON project (conventionally `*.spritesheet.json`):

```json
{
  "baseUnit": 32,
  "maxSheetWidth": 2048,
  "pot": false,
  "widthFit": "shared",
  "heightFit": "tight",
  "output": "./spritesheet.png",
  "manifest": "./spritesheet.manifest.json",
  "sprites": [
    { "id": "coin", "src": "./coin.png", "x": 0, "y": 0, "locked": true },
    { "id": "walk", "src": "./walk.png", "frameCols": 4, "frameRows": 1,
      "frames": [[0,0],[16,0],[32,0],[48,0]], "locked": true }
  ]
}
```

`src` paths are stored relative to the project file. Single-frame sprites use
`x`/`y`; multi-frame sprites use a `frames` array of `[x, y]` per frame.

## Exported manifest

Export writes the packed PNG and a manifest mapping each frame to its rectangle.
Single-frame sprites use their bare `id`; multi-frame frames are keyed `id#0`,
`id#1`, …:

```json
{
  "version": 1,
  "baseUnit": 32,
  "sheetWidth": 960,
  "sheetHeight": 448,
  "sprites": {
    "coin":   { "x": 0,  "y": 0, "w": 16, "h": 16 },
    "walk#0": { "x": 16, "y": 0, "w": 16, "h": 16 }
  }
}
```

## License

The vendored dependencies under `third_party/` retain their own licenses (see the
files alongside each). Project code is yours to license as you see fit.
