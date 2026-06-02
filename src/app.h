#pragma once

#include "project.h"

#include <SDL2/SDL.h>

#include <string>
#include <vector>

struct AppMessage {
    enum class Level { Info, Warn, Error } level = Level::Info;
    std::string text;
};

struct App {
    SDL_Window*   window   = nullptr;
    SDL_Renderer* renderer = nullptr;

    Project     project;
    std::string project_path;     // empty if unsaved
    bool        project_dirty = false;

    std::vector<std::string> recent_files;  // most-recent-first, absolute paths

    // UI state
    float canvas_zoom = 2.0f;
    int   selected_sprite = -1;   // index into project.sprites
    std::vector<AppMessage> messages;
    bool  show_grid = true;
    int   left_column_width = 400;

    // Pending modal requests
    bool open_open_modal = false;
    bool open_save_as_modal = false;
    bool open_add_sprite_modal = false;
    bool quit_requested = false;

    // On-canvas drag of a single placed frame (-1 = none). Offsets are the grab
    // point relative to that frame's own origin, in sprite pixels.
    int canvas_drag = -1;        // sprite index
    int canvas_drag_frame = -1;  // frame index within that sprite
    int canvas_drag_dx = 0;
    int canvas_drag_dy = 0;
};

namespace app {

void log(App& a, AppMessage::Level lvl, std::string text);

void new_project(App& a);
bool open_project(App& a, const std::string& path);
bool save_project(App& a, const std::string& path);   // saves to path; updates project_path

// Recently-opened project list, persisted to a user config file.
void load_recent(App& a);
void add_recent(App& a, const std::string& path);
void remove_recent(App& a, const std::string& path);
void clear_recent(App& a);

// Add a sprite PNG to the current project (auto-id from filename). If id is
// blank, derived from basename. Returns false on error (msg goes to App log).
bool add_sprite(App& a, const std::string& src_path, const std::string& id_override = "");

// Remove a sprite by index. Index is into project.sprites.
void remove_sprite(App& a, int index);

// Run packer for any sprites without locked positions. Updates project + dims.
bool auto_pack(App& a);

// Export PNG + manifest using paths from project, resolved relative to project_path.
bool export_all(App& a);

// Write just the manifest (skip the PNG passes). Useful when sheet PNGs are
// already up to date and only the JSON layout needs refreshing.
bool export_manifest(App& a);

// Refresh sheet dims based on currently placed sprites (no packing).
void recompute_sheet_dims(App& a);

}  // namespace app
