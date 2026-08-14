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
  `remove_sprite`, `auto_pack`, `export_all`, `export_manifest`).
  UI calls into these.
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
- `src/report.{h,cpp}` — read-only sheet audit (see "Sheet report").
- `src/sizes.{h,cpp}` — the tidy size ladder shared by the report and the
  packing-cell picker.

## Project file vs manifest

Two different JSON files, easy to confuse:

- **Project file** (`*.spritesheet.json`) — the editor's save format.
  Read/written by `project::load` / `project::save`. Contains sprite
  sources, frame grids, placements, per-sprite manifest attributes
  (`size`/`volume`/`shadow`, defaults omitted), and project settings.
  `src` paths are stored relative to the project file's directory.
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
    "shadowMeshes": {
      "<name>": "path/to/mesh.json"
    },
    "pieces": {
      "<sid>": {
        "rect": [x, y, w, h],
        "size": [sx, sy],
        "volume": V,
        "heights": "path/to/heights.json",
        "shadow": "<name>"
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
- `size`, `volume`, `shadow`, `heights` are user-set attributes on the
  source image (every frame of a multi-frame sprite shares them; edited
  in the inspector, persisted in the project file). `size` is an optional
  2D int pair — (0,0) means unset and the key is omitted. `volume` is an
  int defaulting to 1 and is always written. `shadow` names an entry in
  the project's shadow-mesh registry, omitted when empty. `heights` is a
  height-map path written verbatim, omitted when empty.
- `shadowMeshes` (omitted when empty) lists the project's shadow-mesh
  registry entries (`Project::shadow_meshes`, edited in the project
  panel, stored in the project file) — but only those referenced by at
  least one exported piece's `shadow`. Mesh paths are consumer-side
  strings written verbatim, not relativized to the manifest. Dangling
  `shadow` references are warned about at export.

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

The canvas toolbar gains a "Pass" dropdown when passes are set
(`app::set_preview_pass`): it swaps each sprite's display texture to that
pass's variant so you can preview it and verify coverage before export.
Display-only — `sprite.pixels` always holds the base pass for export.
Missing/wrong-sized variants keep the base texture tinted red
(`sprite.pass_missing`) and are logged, with a "N missing" counter next
to the dropdown.

## Export modes

- `app::export_all` — writes all pass PNGs + manifest. Ctrl+E.
- `app::export_manifest` — writes just the manifest (skips PNGs). Useful
  when only positions/sizes changed and the bitmaps are still fresh.
  Ctrl+Shift+E.

Both require the project to be saved first (need `project_path` to
resolve `output_rel` / `manifest_rel`).

## Packing cells (margin padding)

A sprite may declare an optional **cell** (`Sprite::cell_w` / `cell_h`,
`cellW`/`cellH` in the project file, both omitted when 0) that is larger
than its art. This trades wasted pixels for uniform, tidy frame sizes —
which is what keeps `SheetFit::Shared` satisfiable and keeps the door
open for the engine's flipbook path, where a sprite's frames must be a
regular grid of identical cells.

The whole feature rests on one split in `sprite.h`:

- `art_w()` / `art_h()` — the frame's real pixels. Used only to read and
  blit the bitmap (`frame_src_x/y`, `exporter`, the canvas image quad).
- `frame_w()` / `frame_h()` — the **cell**, i.e. the slot on the sheet.
  Everything that reasons about layout — packer, `fit_sheet_dims`,
  collision/bounds checks, manifest `rect` — goes through these, so
  padding is transparent to all of it and needed no changes there.

A cell never crops: `frame_w()` clamps to at least `art_w()`, and the
picker only offers ladder values >= the art. Cells only grow.

The art is **centered horizontally and anchored to the bottom** of its
cell (`art_off_x/y`), so all vertical slack sits on top. Widths are
already forced even by `validate_dimensions`, so the centering is exact.

`app::set_cell` **unlocks and unplaces** the sprite. This is required,
not tidiness: `project::hydrate` runs `validate_position` on every placed
sprite, which throws when `x % frame_w != 0`. Leaving a stale position
after a cell change would load fine now and throw on the next open.

`app::snap_cell` / `snap_all_cells` grow cells to the smallest tidy size
that fits. Rounding each axis up independently is minimum-padding by
construction, so no search is needed. On the CityBuildings scene it takes
10 distinct frame sizes down to 5, costs +20% in margin, and drops the
shared multiple from 887,040 to 768.

Manifest `rect` reports the **cell**, not the art — the downstream engine
addresses cells. Note the engine's quad pivot is `anchor - rect.w/2`
(`shaders_basic.js`, fed by `io.js` `aSpriteSize = rect[2]`), so widening
a cell lowers that sprite on screen by half the added width. Height
padding is free; width padding is not.

## Sheet report

`app::report_sheet` (the "Sheet report" button in the project inspector,
also Sprite menu / Ctrl+R) writes a read-only audit into the message log: sprite/frame counts, art pixels vs sheet area and how
full the sheet is, what each axis's `SheetFit` demands, and — the point
of it — whether the frame sizes in use can actually satisfy that demand.

Under `SheetFit::Shared` an axis is rounded up to the least common
multiple of every frame size on it, so one badly sized sprite can demand
an impossible sheet. A size is **tidy** when it's a power of two,
optionally times three (32, 64, 96, 128, 192, 256, 384, 512, 768…); a mix
of tidy sizes always shares a small multiple. A size carrying a second
factor of three, or a prime like 5/7/11, drags that factor into the LCM —
`{160, 224, 320, 352}` forces a multiple of 887,040. The report flags
those sizes, names the sprites using them, suggests the nearest tidy
size in each direction (constrained to whole `base_unit`s), and shows
what the multiple drops to once they're fixed.

It also warns when `max_sheet_width` isn't itself a multiple of the width
LCM, since shared fit will then round the sheet straight past the cap.

Note `project::fit_sheet_dims` clamps its LCM at `1 << 20` and skips any
size that would exceed it; `report::common_multiple` mirrors that exactly
so the reported number is the one the tool really emits, marking the
result `(clamped)` when a size was skipped.

## Conventions

- Headers use `#pragma once`.
- One feature per function; UI in `ui.cpp` is a thin call-through to
  `app::` actions so logic stays testable from the headless side.
- Warnings go through `validation::` (collect-not-throw); hard errors
  throw `std::runtime_error` and bubble to the UI which logs them via
  `app::log`.
- Don't sprinkle comments restating what the code does. The existing
  comment density is intentional — match it.
