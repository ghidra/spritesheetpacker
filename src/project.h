#pragma once

#include "sprite.h"

#include <string>
#include <vector>

// How a sheet axis (width or height) is sized to fit placed content:
//  Shared - round up so every sprite's grid meets the edge (LCM of frame sizes)
//  Tight  - round content up to base_unit only
//  Manual - use an explicit value (clamped to at least the content extent)
enum class SheetFit { Shared, Tight, Manual };

// Named shadow-mesh entry, e.g. "palmtree1" -> "src/scenes/blob/palmtree1_mesh.json".
// Sprites reference the name via their `shadow` attribute. The path is written
// to the manifest verbatim (no relativizing — it's a consumer-side path).
struct ShadowMesh {
    std::string name;
    std::string path;
};

struct Project {
    int version = 1;
    int base_unit = 32;
    std::string output_rel = "./spritesheet.png";
    std::string manifest_rel = "./spritesheet.manifest.json";
    int max_sheet_width = 2048;
    bool pot = false;
    SheetFit width_fit = SheetFit::Shared;
    SheetFit height_fit = SheetFit::Shared;
    int manual_width = 0;
    int manual_height = 0;

    // Render-pass suffixes (e.g. "_D","_N","_P"). When non-empty, export packs
    // one sheet per pass sharing the same layout; the first entry is the base
    // pass whose images are loaded for display/packing and stored in sprite.src.
    // Empty = single-pass export using sprite.src and output as-is.
    std::vector<std::string> passes;

    std::vector<ShadowMesh> shadow_meshes;

    std::vector<Sprite> sprites;

    // Computed after packing
    int sheet_w = 0;
    int sheet_h = 0;
};

namespace project {

// Recompute sheet_w / sheet_h from placed content using each axis's SheetFit
// mode (see Project::width_fit / height_fit). Never clips a sprite (the result
// is always >= the content extent). Honors `pot` by rounding up to a power of
// two afterward.
void fit_sheet_dims(Project& project);

// Insert a pass suffix before the extension, e.g. ("./sheet.png","_N") -> "./sheet_N.png".
std::string output_for_pass(const std::string& output, const std::string& pass);

// Derive a sprite source path for `pass` by swapping the base suffix in the
// filename stem, e.g. ("a/foo_D.png","_D","_N") -> "a/foo_N.png".
std::string source_for_pass(const std::string& src, const std::string& base, const std::string& pass);

// Resolve a project-relative path to an absolute path.
std::string resolve_relative(const std::string& project_path, const std::string& target);

// Convert an abs/relative-to-cwd path to a path relative to the project file
// when possible. Falls back to absolute.
std::string store_relative(const std::string& project_path, const std::string& target);

// JSON load/save of the project file. Throws std::runtime_error on parse error.
Project load(const std::string& path);
void save(const std::string& path, const Project& project);

// Write the manifest JSON for the runtime.
void save_manifest(const std::string& path, const Project& project);

// Hydrate textures + pixel data for all sprites in the project.
// `project_path` is needed to resolve relative `src` entries.
// Renderer is used to create GPU textures for display.
void hydrate(Project& project, const std::string& project_path, SDL_Renderer* renderer,
             std::vector<std::string>& warnings);

}  // namespace project
