#include "app.h"

#include "exporter.h"
#include "packer.h"
#include "project.h"
#include "report.h"
#include "sizes.h"
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

// Swap one sprite's display texture to `pass`'s variant of its base source.
// On missing/wrong-sized variant, falls back to the base texture with
// pass_missing set so the canvas can tint it.
// A piece's "shadow" key is only useful if the manifest's shadowMeshes block
// defines it — flag dangling references before writing.
void warn_unknown_shadow_refs(App& a) {
    for (const Sprite& s : a.project.sprites) {
        if (s.shadow.empty()) continue;
        if (std::none_of(a.project.shadow_meshes.begin(), a.project.shadow_meshes.end(),
                         [&](const ShadowMesh& m) { return m.name == s.shadow; }))
            log(a, AppMessage::Level::Warn,
                s.id + ": shadow \"" + s.shadow + "\" has no shadow-mesh entry — not in manifest");
    }
}

bool apply_pass_texture(App& a, Sprite& s, const std::string& base, const std::string& pass) {
    std::string vsrc = project::source_for_pass(s.src, base, pass);
    std::string abs = project::resolve_relative(a.project_path, vsrc);
    Sprite tmp;
    if (sprite_load_image(tmp, abs, a.renderer) && tmp.w == s.w && tmp.h == s.h) {
        sprite_free_texture(s);
        s.texture = tmp.texture;
        tmp.texture = nullptr;
        s.pass_missing = false;
        return true;
    }
    sprite_free_texture(tmp);
    sprite_texture_from_pixels(s, a.renderer);
    s.pass_missing = true;
    log(a, AppMessage::Level::Warn,
        s.id + ": pass " + pass + " image missing or wrong size (" + vsrc + ")");
    return false;
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
    a.preview_pass = 0;
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
    a.preview_pass = 0;

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
        validation::validate_dimensions(s.id, s.art_w(), s.art_h(), a.project.base_unit, warnings);
    } catch (const std::exception& e) {
        sprite_free_texture(s);
        log(a, AppMessage::Level::Error, e.what());
        return false;
    }
    for (auto& w : warnings) log(a, AppMessage::Level::Warn, w);

    a.project.sprites.push_back(std::move(s));
    if (a.preview_pass > 0 && a.preview_pass < (int)a.project.passes.size())
        apply_pass_texture(a, a.project.sprites.back(), a.project.passes.front(),
                           a.project.passes[a.preview_pass]);
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

bool set_preview_pass(App& a, int pass_index) {
    if (pass_index < 0 || pass_index >= (int)a.project.passes.size()) pass_index = 0;
    a.preview_pass = pass_index;
    if (pass_index == 0) {
        for (Sprite& s : a.project.sprites) {
            sprite_texture_from_pixels(s, a.renderer);
            s.pass_missing = false;
        }
        return true;
    }
    const std::string& base = a.project.passes.front();
    const std::string& pass = a.project.passes[pass_index];
    int missing = 0;
    for (Sprite& s : a.project.sprites)
        if (!apply_pass_texture(a, s, base, pass)) ++missing;
    size_t total = a.project.sprites.size();
    if (missing)
        log(a, AppMessage::Level::Warn, "pass " + pass + ": " + std::to_string(missing) +
            "/" + std::to_string(total) + " images missing or wrong size");
    else
        log(a, AppMessage::Level::Info, "pass " + pass + ": all " +
            std::to_string(total) + " images present");
    return missing == 0;
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

void set_cell(App& a, int index, int cell_w, int cell_h) {
    if (index < 0 || index >= (int)a.project.sprites.size()) return;
    Sprite& s = a.project.sprites[index];
    int cw = cell_w > 0 ? std::max(cell_w, s.art_w()) : 0;
    int ch = cell_h > 0 ? std::max(cell_h, s.art_h()) : 0;
    if (cw == s.cell_w && ch == s.cell_h) return;
    s.cell_w = cw;
    s.cell_h = ch;
    s.locked = false;
    s.unplace();
    a.project_dirty = true;
    recompute_sheet_dims(a);
}

void snap_cell(App& a, int index) {
    if (index < 0 || index >= (int)a.project.sprites.size()) return;
    const Sprite& s = a.project.sprites[index];
    int unit = a.project.base_unit;
    set_cell(a, index, sizes::next_up(s.art_w(), unit), sizes::next_up(s.art_h(), unit));
}

void snap_all_cells(App& a) {
    long long art = 0, cell = 0;
    int changed = 0;
    for (int i = 0; i < (int)a.project.sprites.size(); ++i) {
        const Sprite& s = a.project.sprites[i];
        if (s.art_w() <= 0 || s.art_h() <= 0) continue;
        int before_w = s.frame_w(), before_h = s.frame_h();
        snap_cell(a, i);
        const Sprite& after = a.project.sprites[i];
        if (after.frame_w() != before_w || after.frame_h() != before_h) changed++;
        art += (long long)after.art_w() * after.art_h() * after.frame_count();
        cell += (long long)after.frame_w() * after.frame_h() * after.frame_count();
    }
    if (art <= 0) {
        log(a, AppMessage::Level::Warn, "no sprites to snap");
        return;
    }
    char buf[160];
    std::snprintf(buf, sizeof(buf),
                  "snapped %d sprite%s: %.1f MP art in %.1f MP of cells (+%.0f%% margin)",
                  changed, changed == 1 ? "" : "s", (double)art / 1e6, (double)cell / 1e6,
                  100.0 * ((double)cell / (double)art - 1.0));
    log(a, AppMessage::Level::Info, buf);
}

bool export_all(App& a) {
    if (a.project_path.empty()) {
        log(a, AppMessage::Level::Error, "save the project first");
        return false;
    }
    recompute_sheet_dims(a);

    const std::string& base = a.project.passes.empty() ? std::string() : a.project.passes.front();

    // One PNG per pass (or a single pass with output as-is when none defined).
    std::vector<std::string> passes = a.project.passes;
    if (passes.empty()) passes.push_back("");  // sentinel: write output_rel unchanged

    try {
        for (const std::string& pass : passes) {
            // Gather this pass's pixels per sprite; base pass reuses loaded pixels.
            std::vector<std::vector<unsigned char>> storage(a.project.sprites.size());
            std::vector<const std::vector<unsigned char>*> px(a.project.sprites.size(), nullptr);
            for (size_t i = 0; i < a.project.sprites.size(); ++i) {
                Sprite& s = a.project.sprites[i];
                if (pass.empty() || pass == base) {
                    px[i] = &s.pixels;
                    continue;
                }
                std::string vsrc = project::source_for_pass(s.src, base, pass);
                std::string abs = project::resolve_relative(a.project_path, vsrc);
                Sprite tmp;
                if (sprite_load_image(tmp, abs, nullptr) && tmp.w == s.w && tmp.h == s.h) {
                    storage[i] = std::move(tmp.pixels);
                    px[i] = &storage[i];
                } else {
                    log(a, AppMessage::Level::Warn,
                        s.id + ": pass " + pass + " image missing or wrong size (" +
                        vsrc + ") — left transparent");
                }
            }

            std::string out_rel = pass.empty() ? a.project.output_rel
                                               : project::output_for_pass(a.project.output_rel, pass);
            std::string png = project::resolve_relative(a.project_path, out_rel);
            exporter::export_png(a.project, png, &px);
            log(a, AppMessage::Level::Info, "exported " + png);
        }

        // Layout is identical across passes — manifest written once.
        warn_unknown_shadow_refs(a);
        std::string mf = project::resolve_relative(a.project_path, a.project.manifest_rel);
        project::save_manifest(mf, a.project);
        log(a, AppMessage::Level::Info, "wrote manifest " + mf);
    } catch (const std::exception& e) {
        log(a, AppMessage::Level::Error, std::string("export failed: ") + e.what());
        return false;
    }
    return true;
}

bool export_manifest(App& a) {
    if (a.project_path.empty()) {
        log(a, AppMessage::Level::Error, "save the project first");
        return false;
    }
    recompute_sheet_dims(a);
    warn_unknown_shadow_refs(a);
    try {
        std::string mf = project::resolve_relative(a.project_path, a.project.manifest_rel);
        project::save_manifest(mf, a.project);
        log(a, AppMessage::Level::Info, "wrote manifest " + mf);
    } catch (const std::exception& e) {
        log(a, AppMessage::Level::Error, std::string("manifest export failed: ") + e.what());
        return false;
    }
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

void report_sheet(App& a) {
    for (const report::Line& l : report::sheet(a.project)) {
        AppMessage::Level lvl = AppMessage::Level::Info;
        if (l.level == report::Line::Level::Warn) lvl = AppMessage::Level::Warn;
        if (l.level == report::Line::Level::Error) lvl = AppMessage::Level::Error;
        log(a, lvl, l.text);
    }
}

}  // namespace app
