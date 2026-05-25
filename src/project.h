#pragma once

#include "sprite.h"

#include <string>
#include <vector>

// How a sheet axis (width or height) is sized to fit placed content:
//  Shared - round up so every sprite's grid meets the edge (LCM of frame sizes)
//  Tight  - round content up to base_unit only
//  Manual - use an explicit value (clamped to at least the content extent)
enum class SheetFit { Shared, Tight, Manual };

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
