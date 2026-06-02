# CLAUDE.md

Project notes for Claude. Keep this lean and current.

## What this is

A small SDL2 + Dear ImGui desktop tool for packing PNG sprites into one
sheet and emitting a JSON manifest consumed by a sibling app (an isometric
renderer that lives outside this repo). The manifest format is a hard
contract with that consumer — see "Manifest format" below before editing
`project::save_manifest`.

## Build & run

```sh
make            # produces ./spritesheet
./spritesheet [path/to/project.json]
make clean
```

C++17, `g++`, requires SDL2 (`sdl2-config` on PATH) and `nlohmann/json`
headers. ImGui and ImGuiFileDialog live under `third_party/` as submodules
(`git submodule update --init` after clone). `stb_image*` is vendored.

**clangd vs the build**: clangd in this repo doesn't find `imgui.h` (no
`compile_commands.json` generated), so editor diagnostics about `ImGui` /
`ImTextureID` / `imgui.h` not found are tooling noise. `make` is the
source of truth — if it builds clean, the code is fine.

## Layout

- `src/main.cpp` — SDL/ImGui bootstrapping, event loop.
- `src/app.{h,cpp}` — `App` state struct + top-level actions
  (`new_project`, `open_project`, `save_project`, `add_sprite`,
  `auto_pack`, `export_all`, `export_manifest`). UI calls into these.
- `src/ui.cpp` — all ImGui rendering: menus, panels, canvas, keyboard
  shortcuts. No business logic — delegates to `app::`.
- `src/project.{h,cpp}` — `Project` struct, project-file load/save,
  manifest writer, path resolution, sheet-dim fitting.
- `src/sprite.{h,cpp}` — `Sprite` struct, PNG loading (stb_image),
  per-frame addressing.
- `src/packer.{h,cpp}` — 2D backfilling auto-packer.
- `src/exporter.{h,cpp}` — composites placed sprites onto an RGBA buffer
  and writes the PNG (stb_image_write).
- `src/validation.{h,cpp}` — id/grid/position sanity checks; collects
  warnings rather than aborting where reasonable.

## Project file vs manifest

Two different JSON files, easy to confuse:

- **Project file** (`*.spritesheet.json`) — the editor's save format.
  Read/written by `project::load` / `project::save`. Contains sprite
  sources, frame grids, placements, and project settings. `src` paths
  are stored relative to the project file's directory.
- **Manifest** (`*.manifest.json`) — the export artifact consumed by the
  downstream app. Written by `project::save_manifest`. See below.

Path conventions: anything `*_rel` on `Project` is stored relative to the
project file. Use `project::resolve_relative` (project-rel → absolute) and
`project::store_relative` (absolute → project-rel) at the boundaries.

## Manifest format (downstream contract)

The downstream consumer expects this exact shape — don't reshape without
checking with the user first:

```json
{
  "<kit_name>": {
    "images": [
      {"path": "...", "width": W, "height": H, "spriteWidth": U, "spriteHeight": U}
    ],
    "pieces": {
      "<sid>": {
        "rect": [x, y, w, h],
        "size": [w/U, h/U]
      }
    }
  }
}
```

- `kit_name` is the output PNG's filename stem (no `_kit` suffix, no pass
  suffix stripping — the project's `output_rel` is already the neutral
  name; per-pass filenames are derived via `output_for_pass`).
- `images` lists every per-pass sheet (or a single entry when `passes`
  is empty). `path` is computed relative to the manifest's location.
- `spriteWidth`/`spriteHeight` mirror `baseUnit` and are legacy fallback
  fields; consumers prefer `rect`.
- `pieces` is keyed by sid: bare `id` for single-frame sprites, `id_0`,
  `id_1`, … for multi-frame (see `Sprite::frame_id`). The user has
  explicitly said they prefer treating frames as independent sprites, so
  multi-frame is rare in practice but supported.
- `size` is the piece's footprint in base-unit cells, integer-divided.

`save_manifest` hand-writes the JSON (not `dump()`) to get the specific
formatting: kit/images/pieces pretty-printed, but image entries and
`rect`/`size` arrays compact on a single line. `nlohmann` can't mix those
in one call.

## Render passes

Set `passes` to e.g. `["_D","_N","_P"]` to export one sheet per pass at
identical layout. First entry is the base pass — sprite `src` paths point
at the base files, and sibling pass images are loaded by swapping the
suffix (`project::source_for_pass`). Missing/wrong-sized variants are
logged and left transparent rather than aborting.

The manifest is written once and lists every pass image. Layout is
shared, so editing only ever happens on the base pass.

## Export modes

- `app::export_all` — writes all pass PNGs + manifest. Ctrl+E.
- `app::export_manifest` — writes just the manifest (skips PNGs). Useful
  when only positions/sizes changed and the bitmaps are still fresh.
  Ctrl+Shift+E.

Both require the project to be saved first (need `project_path` to
resolve `output_rel` / `manifest_rel`).

## Conventions

- Headers use `#pragma once`.
- One feature per function; UI in `ui.cpp` is a thin call-through to
  `app::` actions so logic stays testable from the headless side.
- Warnings go through `validation::` (collect-not-throw); hard errors
  throw `std::runtime_error` and bubble to the UI which logs them via
  `app::log`.
- Don't sprinkle comments restating what the code does. The existing
  comment density is intentional — match it.
