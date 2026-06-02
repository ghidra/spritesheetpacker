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
- Export a packed PNG plus a JSON manifest of sprite pieces (or just the
  manifest when only the layout changed).
- **Render passes**: pack/edit one base pass (e.g. diffuse `_D`) and export matching
  sheets for sibling passes (`_N`, `_P`, …) that share the exact same layout — one
  project, one manifest, no duplicated arranging.
- Recently-opened project list and native-feeling in-app file dialogs.

## Dependencies

- A C++17 compiler (`g++`) and `make`.
- **SDL2** (with `sdl2-config` on `PATH`).
- **nlohmann/json** development headers (e.g. `/usr/include/nlohmann/json.hpp`).

Under `third_party/`:

- Dear ImGui (+ SDL2 / SDL_Renderer2 backends) — git submodule, pinned to `v1.91.6`
- ImGuiFileDialog — git submodule
- stb_image / stb_image_write — vendored directly

## Build & run

This repo uses git submodules for ImGui and ImGuiFileDialog. After cloning:

```sh
git clone --recurse-submodules <repo-url>
# or, if already cloned:
git submodule update --init
```

```sh
make            # produces ./spritesheet
./spritesheet                       # start empty
./spritesheet path/to/project.json  # open a project on launch
make clean
```

## Usage

- **File**: New (Ctrl+N), Open (Ctrl+O), Open Recent, Save (Ctrl+S),
  Save As (Ctrl+Shift+S), Export PNG + Manifest (Ctrl+E),
  Export Manifest Only (Ctrl+Shift+E), Quit (Ctrl+Q).
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

## Render passes

For multi-pass render output (diffuse / normal / position, etc.), set **Passes**
in the project settings to a comma-separated suffix list, e.g. `_D,_N,_P`. The
first entry is the **base** pass: its images are what you load, pack, and edit
(sprite `src` paths point at the `_D` files). On export the layout is computed
once, then for each pass the matching image is loaded (by swapping the base
suffix in each filename) and composited at the identical positions:

```
sprite src   beachsand_D.png  ->  beachsand_N.png, beachsand_P.png
output       spritesheet.png  ->  spritesheet_D.png, _N.png, _P.png
```

A missing or wrong-sized variant for a pass is logged and left transparent
rather than aborting the export. Leave **Passes** empty for a normal single
sheet. The manifest is written once (the layout is shared) and lists every
pass image in its `images` array.

## Exported manifest

Export writes the packed PNG(s) and a manifest wrapped in a single named "kit"
keyed by the output file stem. `images` lists every packed sheet (one per pass,
or just one entry without passes) at full atlas dimensions; `pieces` is keyed
by sid (single-frame sprites use the bare `id`, multi-frame frames are keyed
`id_0`, `id_1`, …) and each piece carries `rect` (`[x, y, w, h]` in atlas
pixels) plus `size` (the piece's footprint in base-unit cells:
`[w/baseUnit, h/baseUnit]`). `spriteWidth`/`spriteHeight` on each image mirror
`baseUnit` and act only as a legacy fallback for consumers that don't read
`rect`.

```json
{
  "spritesheet": {
    "images": [
      {"path": "spritesheet_D.png", "width": 960, "height": 448, "spriteWidth": 32, "spriteHeight": 32},
      {"path": "spritesheet_N.png", "width": 960, "height": 448, "spriteWidth": 32, "spriteHeight": 32}
    ],
    "pieces": {
      "coin": {
        "rect": [0, 0, 32, 32],
        "size": [1, 1]
      },
      "beachsand_0": {
        "rect": [0, 0, 160, 128],
        "size": [5, 4]
      }
    }
  }
}
```

## License

The vendored dependencies under `third_party/` retain their own licenses (see the
files alongside each). Project code is yours to license as you see fit.
