#include "app.h"

#include "exporter.h"
#include "packer.h"
#include "project.h"
#include "sprite.h"
#include "validation.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace app {

namespace {

constexpr size_t kMaxRecent = 10;

std::string recent_store_path() {
    fs::path base;
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
        base = xdg;
    } else if (const char* home = std::getenv("HOME"); home && *home) {
        base = fs::path(home) / ".config";
    } else {
        return {};
    }
    return (base / "spritesheet" / "recent.txt").string();
}

void save_recent(const App& a) {
    std::string p = recent_store_path();
    if (p.empty()) return;
    std::error_code ec;
    fs::create_directories(fs::path(p).parent_path(), ec);
    std::ofstream f(p, std::ios::trunc);
    if (!f) return;
    for (const std::string& s : a.recent_files) f << s << "\n";
}

std::string canonical_abs(const std::string& path) {
    return fs::absolute(fs::path(path)).lexically_normal().string();
}

}  // namespace

void log(App& a, AppMessage::Level lvl, std::string text) {
    a.messages.push_back({lvl, std::move(text)});
    if (a.messages.size() > 200)
        a.messages.erase(a.messages.begin(), a.messages.begin() + (a.messages.size() - 200));
}

static void clear_project_textures(App& a) {
    for (Sprite& s : a.project.sprites) sprite_free_texture(s);
}

void new_project(App& a) {
    clear_project_textures(a);
    a.project = Project{};
    a.project_path.clear();
    a.project_dirty = false;
    a.selected_sprite = -1;
    log(a, AppMessage::Level::Info, "new project");
}

bool open_project(App& a, const std::string& path) {
    Project np;
    try {
        np = project::load(path);
    } catch (const std::exception& e) {
        log(a, AppMessage::Level::Error, std::string("open failed: ") + e.what());
        return false;
    }

    clear_project_textures(a);
    a.project = std::move(np);
    a.project_path = path;
    a.project_dirty = false;
    a.selected_sprite = -1;

    std::vector<std::string> warnings;
    try {
        project::hydrate(a.project, a.project_path, a.renderer, warnings);
    } catch (const std::exception& e) {
        log(a, AppMessage::Level::Error, std::string("hydrate failed: ") + e.what());
        return false;
    }
    for (auto& w : warnings) log(a, AppMessage::Level::Warn, w);

    recompute_sheet_dims(a);
    add_recent(a, path);
    log(a, AppMessage::Level::Info, "opened " + path);
    return true;
}

bool save_project(App& a, const std::string& path) {
    // If saving to a different path, rewrite sprite.src as relative to new path
    if (path != a.project_path && !a.project_path.empty()) {
        for (Sprite& s : a.project.sprites) {
            std::string abs = project::resolve_relative(a.project_path, s.src);
            s.src = project::store_relative(path, abs);
        }
    } else if (a.project_path.empty()) {
        // First save — sprite.src should already be cwd-relative or absolute;
        // normalize to project-relative.
        for (Sprite& s : a.project.sprites) {
            std::string abs = fs::absolute(fs::path(s.src)).lexically_normal().string();
            s.src = project::store_relative(path, abs);
        }
    }

    try {
        project::save(path, a.project);
    } catch (const std::exception& e) {
        log(a, AppMessage::Level::Error, std::string("save failed: ") + e.what());
        return false;
    }
    a.project_path = path;
    a.project_dirty = false;
    add_recent(a, path);
    log(a, AppMessage::Level::Info, "saved " + path);
    return true;
}

bool add_sprite(App& a, const std::string& src_path, const std::string& id_override) {
    if (!fs::is_regular_file(src_path)) {
        log(a, AppMessage::Level::Error, "file not found: " + src_path);
        return false;
    }

    std::string id = id_override;
    if (id.empty()) id = fs::path(src_path).stem().string();
    {
        std::string base = id;
        int n = 1;
        while (std::any_of(a.project.sprites.begin(), a.project.sprites.end(),
                           [&](const Sprite& s){ return s.id == id; })) {
            ++n;
            id = base + "_" + std::to_string(n);
        }
    }

    Sprite s;
    s.id = id;
    s.src = a.project_path.empty()
        ? fs::absolute(src_path).string()
        : project::store_relative(a.project_path, src_path);

    std::string abs_src = a.project_path.empty()
        ? fs::absolute(src_path).string()
        : project::resolve_relative(a.project_path, s.src);

    if (!sprite_load_image(s, abs_src, a.renderer)) {
        log(a, AppMessage::Level::Error, s.id + ": " + s.load_error);
        return false;
    }

    std::vector<std::string> warnings;
    try {
        validation::validate_frame_grid(s.id, s.w, s.h, s.frame_cols, s.frame_rows);
        validation::validate_dimensions(s.id, s.frame_w(), s.frame_h(), a.project.base_unit, warnings);
    } catch (const std::exception& e) {
        sprite_free_texture(s);
        log(a, AppMessage::Level::Error, e.what());
        return false;
    }
    for (auto& w : warnings) log(a, AppMessage::Level::Warn, w);

    a.project.sprites.push_back(std::move(s));
    a.project_dirty = true;
    log(a, AppMessage::Level::Info, "added " + id + " (" +
        std::to_string(a.project.sprites.back().w) + "x" +
        std::to_string(a.project.sprites.back().h) + ")");
    return true;
}

void remove_sprite(App& a, int index) {
    if (index < 0 || (size_t)index >= a.project.sprites.size()) return;
    std::string id = a.project.sprites[index].id;
    sprite_free_texture(a.project.sprites[index]);
    a.project.sprites.erase(a.project.sprites.begin() + index);
    a.project_dirty = true;
    if (a.selected_sprite == index) a.selected_sprite = -1;
    else if (a.selected_sprite > index) --a.selected_sprite;
    recompute_sheet_dims(a);
    log(a, AppMessage::Level::Info, "removed " + id);
}

bool auto_pack(App& a) {
    try {
        packer::pack(a.project);
    } catch (const std::exception& e) {
        log(a, AppMessage::Level::Error, std::string("pack failed: ") + e.what());
        return false;
    }
    a.project_dirty = true;
    log(a, AppMessage::Level::Info, "packed " + std::to_string(a.project.sheet_w) + "x" +
                                     std::to_string(a.project.sheet_h));
    return true;
}

bool export_all(App& a) {
    if (a.project_path.empty()) {
        log(a, AppMessage::Level::Error, "save the project first");
        return false;
    }
    // Make sure dims are up to date
    recompute_sheet_dims(a);

    std::string png = project::resolve_relative(a.project_path, a.project.output_rel);
    std::string mf  = project::resolve_relative(a.project_path, a.project.manifest_rel);
    try {
        exporter::export_png(a.project, png);
        project::save_manifest(mf, a.project);
    } catch (const std::exception& e) {
        log(a, AppMessage::Level::Error, std::string("export failed: ") + e.what());
        return false;
    }
    log(a, AppMessage::Level::Info, "exported " + png + " + " + mf);
    return true;
}

void load_recent(App& a) {
    a.recent_files.clear();
    std::string p = recent_store_path();
    if (p.empty()) return;
    std::ifstream f(p);
    if (!f) return;
    std::string line;
    while (std::getline(f, line) && a.recent_files.size() < kMaxRecent)
        if (!line.empty()) a.recent_files.push_back(line);
}

void add_recent(App& a, const std::string& path) {
    std::string abs = canonical_abs(path);
    a.recent_files.erase(std::remove(a.recent_files.begin(), a.recent_files.end(), abs),
                         a.recent_files.end());
    a.recent_files.insert(a.recent_files.begin(), abs);
    if (a.recent_files.size() > kMaxRecent) a.recent_files.resize(kMaxRecent);
    save_recent(a);
}

void remove_recent(App& a, const std::string& path) {
    std::string abs = canonical_abs(path);
    size_t before = a.recent_files.size();
    a.recent_files.erase(
        std::remove_if(a.recent_files.begin(), a.recent_files.end(),
                       [&](const std::string& s) { return s == abs || s == path; }),
        a.recent_files.end());
    if (a.recent_files.size() != before) save_recent(a);
}

void clear_recent(App& a) {
    a.recent_files.clear();
    save_recent(a);
}

void recompute_sheet_dims(App& a) {
    project::fit_sheet_dims(a.project);
}

}  // namespace app
