#include "ui.h"

#include "app.h"
#include "sizes.h"
#include "sprite.h"

#include <imgui.h>
#include <ImGuiFileDialog.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

namespace ui {

namespace {

ImTextureID tex_id(SDL_Texture* t) {
    return (ImTextureID)(intptr_t)t;
}

int snap(int v, int step) {
    if (step <= 0) return v;
    if (v < 0) v = 0;
    return (v / step) * step;
}

// Draw the file menu. Sets app flags to open modals.
void draw_menu(App& a) {
    if (!ImGui::BeginMenuBar()) return;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New", "Ctrl+N")) app::new_project(a);
        if (ImGui::MenuItem("Open...", "Ctrl+O")) a.open_open_modal = true;
        if (ImGui::BeginMenu("Open Recent", !a.recent_files.empty())) {
            std::string to_open;
            for (const std::string& p : a.recent_files) {
                std::string name = std::filesystem::path(p).filename().string();
                if (name.empty()) name = p;
                if (ImGui::MenuItem(name.c_str())) to_open = p;
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", p.c_str());
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Clear Recently Opened")) app::clear_recent(a);
            ImGui::EndMenu();
            if (!to_open.empty() && !app::open_project(a, to_open))
                app::remove_recent(a, to_open);
        }
        ImGui::Separator();
        bool can_save = !a.project_path.empty();
        if (ImGui::MenuItem("Save", "Ctrl+S", false, can_save)) {
            app::save_project(a, a.project_path);
        }
        if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) a.open_save_as_modal = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Export PNG + Manifest", "Ctrl+E")) {
            app::export_all(a);
        }
        if (ImGui::MenuItem("Export Manifest Only", "Ctrl+Shift+E")) {
            app::export_manifest(a);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Ctrl+Q")) a.quit_requested = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Sprite")) {
        if (ImGui::MenuItem("Add PNG...", "Ctrl+A")) a.open_add_sprite_modal = true;
        bool has_sel = a.selected_sprite >= 0 && a.selected_sprite < (int)a.project.sprites.size();
        if (ImGui::MenuItem("Remove selected", "Del", false, has_sel))
            app::remove_sprite(a, a.selected_sprite);
        if (ImGui::MenuItem("Auto-pack all", "Ctrl+P")) app::auto_pack(a);
        ImGui::Separator();
        if (ImGui::MenuItem("Sheet report", "Ctrl+R")) app::report_sheet(a);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        ImGui::SliderFloat("Zoom", &a.canvas_zoom, 0.25f, 8.0f, "%.2fx");
        ImGui::Checkbox("Show base grid", &a.show_grid);
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
}

constexpr const char* kOpenKey      = "OpenProjectDlg";
constexpr const char* kSaveAsKey    = "SaveProjectAsDlg";
constexpr const char* kAddSpriteKey = "AddSpriteDlg";
constexpr const char* kProjectExt   = ".json";
constexpr const char* kSpriteExt    = ".png";

void open_path_or_default(IGFD::FileDialogConfig& cfg, const std::string& current, const char* default_name) {
    if (!current.empty()) {
        cfg.filePathName = current;
    } else {
        cfg.path = ".";
        cfg.fileName = default_name;
    }
}

void draw_file_dialogs(App& a) {
    auto* dlg = ImGuiFileDialog::Instance();

    if (a.open_open_modal) {
        IGFD::FileDialogConfig cfg;
        open_path_or_default(cfg, a.project_path, "project.spritesheet.json");
        cfg.flags = ImGuiFileDialogFlags_Modal;
        dlg->OpenDialog(kOpenKey, "Open Project", kProjectExt, cfg);
        a.open_open_modal = false;
    }
    if (a.open_save_as_modal) {
        IGFD::FileDialogConfig cfg;
        open_path_or_default(cfg, a.project_path, "project.spritesheet.json");
        cfg.flags = ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_ConfirmOverwrite;
        dlg->OpenDialog(kSaveAsKey, "Save Project As", kProjectExt, cfg);
        a.open_save_as_modal = false;
    }
    if (a.open_add_sprite_modal) {
        IGFD::FileDialogConfig cfg;
        cfg.path = ".";
        cfg.flags = ImGuiFileDialogFlags_Modal;
        dlg->OpenDialog(kAddSpriteKey, "Add Sprite PNG", kSpriteExt, cfg);
        a.open_add_sprite_modal = false;
    }

    const ImVec2 min_size(600, 400);
    if (dlg->Display(kOpenKey, ImGuiWindowFlags_NoCollapse, min_size)) {
        if (dlg->IsOk()) app::open_project(a, dlg->GetFilePathName());
        dlg->Close();
    }
    if (dlg->Display(kSaveAsKey, ImGuiWindowFlags_NoCollapse, min_size)) {
        if (dlg->IsOk()) app::save_project(a, dlg->GetFilePathName());
        dlg->Close();
    }
    if (dlg->Display(kAddSpriteKey, ImGuiWindowFlags_NoCollapse, min_size)) {
        if (dlg->IsOk()) app::add_sprite(a, dlg->GetFilePathName());
        dlg->Close();
    }
}

void draw_sprite_list(App& a) {
    ImGui::TextUnformatted("Sprites");
    ImGui::Separator();
    if (ImGui::Button("Add PNG")) a.open_add_sprite_modal = true;
    ImGui::SameLine();
    if (ImGui::Button("Auto-pack")) app::auto_pack(a);
    ImGui::SameLine();
    if (ImGui::Button("Export")) app::export_all(a);
    ImGui::SameLine();
    if (ImGui::Button("Manifest")) app::export_manifest(a);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Write only the manifest JSON (skip PNGs)");

    ImGui::Separator();
    ImGui::Text("%zu sprites · sheet %dx%d", a.project.sprites.size(), a.project.sheet_w, a.project.sheet_h);
    ImGui::Separator();

    int to_remove = -1;
    for (int i = 0; i < (int)a.project.sprites.size(); ++i) {
        Sprite& s = a.project.sprites[i];
        ImGui::PushID(i);
        bool selected = (a.selected_sprite == i);
        const char* status = s.any_placed() ? (s.locked ? "[locked]" : "[placed]") : "[unplaced]";
        char label[256];
        if (s.frame_count() > 1)
            std::snprintf(label, sizeof(label), "%s  (%dx%d · %dx%d frames) %s",
                          s.id.c_str(), s.w, s.h, s.frame_cols, s.frame_rows, status);
        else
            std::snprintf(label, sizeof(label), "%s  (%dx%d) %s",
                          s.id.c_str(), s.w, s.h, status);
        float remove_w = ImGui::GetFrameHeight();
        float row_w = ImGui::GetContentRegionAvail().x
                      - remove_w - ImGui::GetStyle().ItemSpacing.x;
        if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_AllowDoubleClick,
                              ImVec2(row_w, 0)))
            a.selected_sprite = i;

        // Drag source: drag this sprite onto the canvas
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload("SPRITE_IDX", &i, sizeof(int));
            ImGui::Text("Place %s", s.id.c_str());
            if (s.texture) {
                float thumb = 48.0f;
                float scale = std::min(thumb / s.w, thumb / s.h);
                ImGui::Image(tex_id(s.texture), ImVec2(s.w * scale, s.h * scale));
            }
            ImGui::EndDragDropSource();
        }

        // Per-sprite controls
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            ImGui::OpenPopup("sprite_ctx");
        if (ImGui::BeginPopup("sprite_ctx")) {
            if (ImGui::MenuItem("Unplace", nullptr, false, s.any_placed())) {
                s.unplace();
                a.project_dirty = true;
                app::recompute_sheet_dims(a);
            }
            if (ImGui::MenuItem("Toggle lock", nullptr, false, s.is_placed())) {
                s.locked = !s.locked;
                a.project_dirty = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Remove")) to_remove = i;
            ImGui::EndPopup();
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("x")) to_remove = i;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remove %s from project", s.id.c_str());
        ImGui::PopID();
    }
    if (to_remove >= 0) app::remove_sprite(a, to_remove);
}

void draw_messages(App& a) {
    ImGui::TextUnformatted("Messages");
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) a.messages.clear();
    ImGui::Separator();
    for (auto& m : a.messages) {
        ImVec4 col;
        switch (m.level) {
            case AppMessage::Level::Info:  col = ImVec4(0.85f, 0.85f, 0.85f, 1.0f); break;
            case AppMessage::Level::Warn:  col = ImVec4(1.00f, 0.80f, 0.20f, 1.0f); break;
            case AppMessage::Level::Error: col = ImVec4(1.00f, 0.30f, 0.30f, 1.0f); break;
        }
        ImGui::TextColored(col, "%s", m.text.c_str());
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
        ImGui::SetScrollHereY(1.0f);
}

void draw_inspector(App& a) {
    ImGui::TextUnformatted("Inspector");
    ImGui::Separator();
    if (a.selected_sprite < 0 || a.selected_sprite >= (int)a.project.sprites.size()) {
        ImGui::TextDisabled("(no sprite selected)");
        ImGui::Separator();
        ImGui::Text("Project");
        ImGui::InputInt("Base unit", &a.project.base_unit);
        if (a.project.base_unit < 1) a.project.base_unit = 1;
        ImGui::InputInt("Max sheet width", &a.project.max_sheet_width);
        if (ImGui::Checkbox("Round sheet to POT", &a.project.pot)) {
            a.project_dirty = true;
            app::recompute_sheet_dims(a);
        }

        // Per-axis sizing: Shared (all grids meet edge) / Tight / Manual.
        const char* fit_items = "Shared edges\0Tight\0Manual\0";
        auto axis_ui = [&](const char* label, const char* man_label,
                           SheetFit& fit, int& manual, int computed) {
            int cur = (int)fit;
            if (ImGui::Combo(label, &cur, fit_items)) {
                SheetFit nf = (SheetFit)cur;
                if (nf == SheetFit::Manual && manual < 1) manual = computed;
                fit = nf;
                a.project_dirty = true;
                app::recompute_sheet_dims(a);
            }
            if (fit == SheetFit::Manual) {
                if (ImGui::InputInt(man_label, &manual)) {
                    if (manual < 1) manual = 1;
                    a.project_dirty = true;
                    app::recompute_sheet_dims(a);
                }
            }
        };
        axis_ui("Width fit", "Width", a.project.width_fit, a.project.manual_width, a.project.sheet_w);
        axis_ui("Height fit", "Height", a.project.height_fit, a.project.manual_height, a.project.sheet_h);
        ImGui::Text("Sheet (export): %d x %d", a.project.sheet_w, a.project.sheet_h);
        if (ImGui::Button("Snap all cells to best size", ImVec2(-1, 0))) app::snap_all_cells(a);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Grow every sprite's packing cell to the smallest tidy size\n"
                              "that fits its art. Unplaces them all — re-pack with Ctrl+P.");
        if (ImGui::Button("Sheet report", ImVec2(-1, 0))) app::report_sheet(a);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Audit sheet usage and whether the fit rules above are\n"
                              "satisfiable (Ctrl+R). Results go to the Messages panel.");

        char buf_out[512];  std::snprintf(buf_out, sizeof(buf_out), "%s", a.project.output_rel.c_str());
        if (ImGui::InputText("Output PNG", buf_out, sizeof(buf_out))) {
            a.project.output_rel = buf_out;
            a.project_dirty = true;
        }
        char buf_mf[512];   std::snprintf(buf_mf, sizeof(buf_mf), "%s", a.project.manifest_rel.c_str());
        if (ImGui::InputText("Manifest JSON", buf_mf, sizeof(buf_mf))) {
            a.project.manifest_rel = buf_mf;
            a.project_dirty = true;
        }

        // Render passes: comma-separated suffixes; first is the base (edited) pass.
        std::string joined;
        for (size_t i = 0; i < a.project.passes.size(); ++i)
            joined += (i ? "," : "") + a.project.passes[i];
        char buf_pass[256]; std::snprintf(buf_pass, sizeof(buf_pass), "%s", joined.c_str());
        if (ImGui::InputText("Passes", buf_pass, sizeof(buf_pass))) {
            a.project.passes.clear();
            std::string tok;
            for (const char* c = buf_pass; ; ++c) {
                if (*c == ',' || *c == '\0') {
                    size_t b = tok.find_first_not_of(" \t");
                    size_t e = tok.find_last_not_of(" \t");
                    if (b != std::string::npos) a.project.passes.push_back(tok.substr(b, e - b + 1));
                    tok.clear();
                    if (*c == '\0') break;
                } else {
                    tok += *c;
                }
            }
            a.project_dirty = true;
            // Pass list changed under an active preview — drop back to base.
            if (a.preview_pass != 0) app::set_preview_pass(a, 0);
        }
        if (a.project.passes.empty())
            ImGui::TextDisabled("single sheet; e.g. _D,_N,_P to export matching passes");
        else
            ImGui::TextDisabled("base pass: %s  (edited/packed); export writes one sheet per pass",
                                a.project.passes.front().c_str());

        // Shadow-mesh registry: name -> consumer-side path. Sprites pick the
        // name in their `shadow` attribute; renames follow the references.
        ImGui::Separator();
        ImGui::TextUnformatted("Shadow meshes");
        int mesh_remove = -1;
        for (int i = 0; i < (int)a.project.shadow_meshes.size(); ++i) {
            ShadowMesh& m = a.project.shadow_meshes[i];
            ImGui::PushID(i);
            char name_buf[128]; std::snprintf(name_buf, sizeof(name_buf), "%s", m.name.c_str());
            ImGui::SetNextItemWidth(110.0f);
            if (ImGui::InputText("##mesh_name", name_buf, sizeof(name_buf))) {
                std::string old = m.name;
                m.name = name_buf;
                if (!old.empty())
                    for (Sprite& sp : a.project.sprites)
                        if (sp.shadow == old) sp.shadow = m.name;
                a.project_dirty = true;
            }
            ImGui::SameLine();
            float remove_w = ImGui::GetFrameHeight();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x
                                    - remove_w - ImGui::GetStyle().ItemSpacing.x);
            char path_buf[512]; std::snprintf(path_buf, sizeof(path_buf), "%s", m.path.c_str());
            if (ImGui::InputText("##mesh_path", path_buf, sizeof(path_buf))) {
                m.path = path_buf;
                a.project_dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("x")) mesh_remove = i;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Remove entry (sprites referencing it keep the stale name)");
            ImGui::PopID();
        }
        if (mesh_remove >= 0) {
            a.project.shadow_meshes.erase(a.project.shadow_meshes.begin() + mesh_remove);
            a.project_dirty = true;
        }
        if (ImGui::Button("Add shadow mesh")) {
            a.project.shadow_meshes.push_back({"", ""});
            a.project_dirty = true;
        }
        if (a.project.shadow_meshes.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("name + mesh path; sprites reference the name");
        }
        return;
    }
    Sprite& s = a.project.sprites[a.selected_sprite];
    ImGui::Text("ID");
    char id_buf[256]; std::snprintf(id_buf, sizeof(id_buf), "%s", s.id.c_str());
    if (ImGui::InputText("##id", id_buf, sizeof(id_buf))) { s.id = id_buf; a.project_dirty = true; }
    ImGui::Text("src: %s", s.src.c_str());
    ImGui::Text("source: %d x %d", s.w, s.h);

    int grid[2] = {s.frame_cols, s.frame_rows};
    if (ImGui::InputInt2("frames (cols, rows)", grid)) {
        int c = std::max(1, grid[0]);
        int r = std::max(1, grid[1]);
        if (s.w % c != 0 || s.h % r != 0) {
            app::log(a, AppMessage::Level::Warn,
                     s.id + ": source not evenly divisible by that frame grid — ignored");
        } else {
            s.frame_cols = c;
            s.frame_rows = r;
            s.unplace();
            s.ensure_frames();
            a.project_dirty = true;
            app::recompute_sheet_dims(a);
        }
    }
    ImGui::Text("art size: %d x %d  (%d frames)", s.art_w(), s.art_h(), s.frame_count());

    // Packing cell: optional margin around the art. Only sizes >= the art are
    // offered — a cell never crops. Art is centered left/right, sits on the
    // bottom edge, and the cell is what the manifest reports as `rect`.
    ImGui::Separator();
    ImGui::TextUnformatted("Packing cell");
    auto cell_combo = [&](const char* label, int art, int cur, int& picked) {
        bool changed = false;
        std::vector<int> opts = sizes::ladder(a.project.base_unit, std::max(art, 1) * 4);
        opts.erase(std::remove_if(opts.begin(), opts.end(),
                                  [&](int v) { return v < art; }), opts.end());
        char preview[64];
        if (cur > art) std::snprintf(preview, sizeof(preview), "%d  (+%d)", cur, cur - art);
        else std::snprintf(preview, sizeof(preview), "fit art (%d)", art);
        if (ImGui::BeginCombo(label, preview)) {
            if (ImGui::Selectable("fit art", cur <= art)) { picked = 0; changed = true; }
            for (int v : opts) {
                char item[64];
                std::snprintf(item, sizeof(item), "%d  (+%d)", v, v - art);
                if (ImGui::Selectable(item, v == cur)) { picked = v; changed = true; }
            }
            ImGui::EndCombo();
        }
        return changed;
    };
    int new_cw = s.frame_w(), new_ch = s.frame_h();
    bool cell_changed = false;
    if (cell_combo("cell width", s.art_w(), s.frame_w(), new_cw)) cell_changed = true;
    if (cell_combo("cell height", s.art_h(), s.frame_h(), new_ch)) cell_changed = true;
    if (cell_changed) app::set_cell(a, a.selected_sprite, new_cw, new_ch);

    if (s.has_margin()) {
        int wasted = s.frame_w() * s.frame_h() - s.art_w() * s.art_h();
        ImGui::TextDisabled("margin: %d each side, %d top, %d bottom  (%.0f%% of cell)",
                            s.art_off_x(), s.frame_h() - s.art_h(), 0,
                            100.0 * wasted / (s.frame_w() * s.frame_h()));
    } else {
        ImGui::TextDisabled("no margin — cell fits the art exactly");
    }
    if (ImGui::Button("Snap to best size")) app::snap_cell(a, a.selected_sprite);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Grow the cell to the smallest tidy size that fits.\n"
                          "Unlocks and unplaces the sprite — re-pack with Ctrl+P.");

    // Manifest attributes — per source image; every frame shares them.
    ImGui::Separator();
    ImGui::TextUnformatted("Manifest attributes");
    int sz[2] = {s.size_x, s.size_y};
    if (ImGui::InputInt2("size", sz)) {
        s.size_x = std::max(0, sz[0]);
        s.size_y = std::max(0, sz[1]);
        a.project_dirty = true;
    }
    if (!s.has_size()) {
        ImGui::SameLine();
        ImGui::TextDisabled("(unset — omitted)");
    }
    int vol = s.volume;
    if (ImGui::InputInt("volume", &vol)) {
        s.volume = vol;
        a.project_dirty = true;
    }
    char heights_buf[512]; std::snprintf(heights_buf, sizeof(heights_buf), "%s", s.heights.c_str());
    if (ImGui::InputText("heights", heights_buf, sizeof(heights_buf))) {
        s.heights = heights_buf;
        a.project_dirty = true;
    }
    if (s.heights.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("(unset — omitted)");
    }

    // shadow: pick a project shadow-mesh by name (registry lives in the
    // project panel — deselect the sprite to edit it).
    const char* shadow_preview = s.shadow.empty() ? "(none)" : s.shadow.c_str();
    if (ImGui::BeginCombo("shadow", shadow_preview)) {
        if (ImGui::Selectable("(none)", s.shadow.empty()) && !s.shadow.empty()) {
            s.shadow.clear();
            a.project_dirty = true;
        }
        for (const ShadowMesh& m : a.project.shadow_meshes) {
            if (m.name.empty()) continue;
            if (ImGui::Selectable(m.name.c_str(), s.shadow == m.name) && s.shadow != m.name) {
                s.shadow = m.name;
                a.project_dirty = true;
            }
        }
        ImGui::EndCombo();
    }
    if (s.shadow.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("(unset — omitted)");
    } else if (std::none_of(a.project.shadow_meshes.begin(), a.project.shadow_meshes.end(),
                            [&](const ShadowMesh& m) { return m.name == s.shadow; })) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "(no mesh entry)");
    }

    if (s.frame_count() == 1) {
        bool placed = s.is_placed();
        if (ImGui::Checkbox("Placed", &placed)) {
            if (!placed) s.unplace();
            else { s.frames[0].x = 0; s.frames[0].y = 0; }
            a.project_dirty = true;
            app::recompute_sheet_dims(a);
        }
        if (s.is_placed()) {
            int xy[2] = {s.frames[0].x, s.frames[0].y};
            if (ImGui::InputInt2("x, y", xy)) {
                s.frames[0].x = snap(xy[0], s.frame_w());
                s.frames[0].y = snap(xy[1], s.frame_h());
                a.project_dirty = true;
                app::recompute_sheet_dims(a);
            }
            bool locked = s.locked;
            if (ImGui::Checkbox("Locked", &locked)) { s.locked = locked; a.project_dirty = true; }
        }
    } else {
        ImGui::TextDisabled("multi-frame: placed by Auto-pack");
        if (s.is_placed()) {
            bool locked = s.locked;
            if (ImGui::Checkbox("Locked (keep frame positions)", &locked)) {
                s.locked = locked;
                a.project_dirty = true;
            }
        }
        if (s.any_placed() && ImGui::Button("Unplace")) {
            s.unplace();
            a.project_dirty = true;
            app::recompute_sheet_dims(a);
        }
    }

    if (s.texture) {
        ImGui::Separator();
        float maxw = ImGui::GetContentRegionAvail().x;
        float sc = std::min(maxw / s.w, 4.0f);
        if (sc < 1.0f) sc = maxw / s.w;
        ImGui::Image(tex_id(s.texture), ImVec2(s.w * sc, s.h * sc));
    }
}

void draw_canvas(App& a) {
    ImGui::Text("Drag from the Sprites panel to place; drag any placed frame to move it on its grid.");
    ImGui::SliderFloat("Zoom", &a.canvas_zoom, 0.25f, 8.0f, "%.2fx");
    ImGui::SameLine();
    ImGui::Checkbox("Show base grid", &a.show_grid);

    if (!a.project.passes.empty()) {
        if (a.preview_pass >= (int)a.project.passes.size()) a.preview_pass = 0;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140.0f);
        std::string cur = a.project.passes[a.preview_pass] + (a.preview_pass == 0 ? " (base)" : "");
        if (ImGui::BeginCombo("Pass", cur.c_str())) {
            for (int i = 0; i < (int)a.project.passes.size(); ++i) {
                std::string label = a.project.passes[i] + (i == 0 ? " (base)" : "");
                if (ImGui::Selectable(label.c_str(), i == a.preview_pass))
                    app::set_preview_pass(a, i);
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Preview this pass's images on the canvas.\n"
                              "Variants missing or wrong-sized show the base image tinted red.");
        if (a.preview_pass > 0) {
            int missing = 0;
            for (const Sprite& s : a.project.sprites)
                if (s.pass_missing) ++missing;
            if (missing) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%d missing", missing);
            }
        }
    }

    float z = a.canvas_zoom;
    // The exported sheet is sheet_w x sheet_h. The workbench is at least the
    // budgeted width so there's room to drop beyond the current content.
    int real_w = a.project.sheet_w;
    int real_h = a.project.sheet_h;
    int work_w = std::max({real_w, a.project.max_sheet_width, 32});
    int work_h = std::max(real_h, 32);
    ImVec2 canvas_sz(work_w * z, work_h * z);

    ImGui::BeginChild("canvas_scroll", ImVec2(0, 0), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Dark workbench behind everything, then the lighter actual sheet on top.
    dl->AddRectFilled(origin, ImVec2(origin.x + canvas_sz.x, origin.y + canvas_sz.y),
                      IM_COL32(24, 24, 28, 255));
    if (real_w > 0 && real_h > 0)
        dl->AddRectFilled(origin, ImVec2(origin.x + real_w * z, origin.y + real_h * z),
                          IM_COL32(40, 40, 46, 255));

    // Base-unit grid lines (span the whole workbench so guides extend past content)
    if (a.show_grid && a.project.base_unit > 0) {
        ImU32 gcol = IM_COL32(70, 70, 80, 200);
        int bu = a.project.base_unit;
        for (int x = 0; x <= work_w; x += bu) {
            float px = origin.x + x * z;
            dl->AddLine(ImVec2(px, origin.y), ImVec2(px, origin.y + canvas_sz.y), gcol, 1.0f);
        }
        for (int y = 0; y <= work_h; y += bu) {
            float py = origin.y + y * z;
            dl->AddLine(ImVec2(origin.x, py), ImVec2(origin.x + canvas_sz.x, py), gcol, 1.0f);
        }
    }

    // Grid of the selected sprite's own frame size — brighter, tinted to match
    // the selection so it reads as "where this sprite snaps."
    if (a.selected_sprite >= 0 && a.selected_sprite < (int)a.project.sprites.size()) {
        const Sprite& sel = a.project.sprites[a.selected_sprite];
        int fw = sel.frame_w(), fh = sel.frame_h();
        if (fw > 0 && fh > 0) {
            ImU32 scol = IM_COL32(255, 220, 80, 90);
            for (int x = 0; x <= work_w; x += fw) {
                float px = origin.x + x * z;
                dl->AddLine(ImVec2(px, origin.y), ImVec2(px, origin.y + canvas_sz.y), scol, 1.0f);
            }
            for (int y = 0; y <= work_h; y += fh) {
                float py = origin.y + y * z;
                dl->AddLine(ImVec2(origin.x, py), ImVec2(origin.x + canvas_sz.x, py), scol, 1.0f);
            }
        }
    }

    // Hit area first so we can drop on it
    ImGui::InvisibleButton("canvas_hit", canvas_sz);

    // Drag-drop target
    if (ImGui::BeginDragDropTarget()) {
        const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SPRITE_IDX",
                                                                  ImGuiDragDropFlags_AcceptBeforeDelivery);
        if (payload && payload->IsDataType("SPRITE_IDX") && payload->IsDelivery()) {
            int idx = *(const int*)payload->Data;
            if (idx >= 0 && idx < (int)a.project.sprites.size()) {
                Sprite& s = a.project.sprites[idx];
                s.ensure_frames();
                ImVec2 mp = ImGui::GetIO().MousePos;
                int px = snap((int)((mp.x - origin.x) / z), s.frame_w());
                int py = snap((int)((mp.y - origin.y) / z), s.frame_h());
                if (px < 0) px = 0;
                if (py < 0) py = 0;
                // Drop the source as one block; frames keep their grid layout.
                for (int fi = 0; fi < s.frame_count(); ++fi) {
                    s.frames[fi].x = px + s.frame_col(fi) * s.frame_w();
                    s.frames[fi].y = py + s.frame_row(fi) * s.frame_h();
                }
                s.locked = true;
                a.project_dirty = true;
                a.selected_sprite = idx;
                app::recompute_sheet_dims(a);
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Draw placed frames
    for (int i = 0; i < (int)a.project.sprites.size(); ++i) {
        Sprite& s = a.project.sprites[i];
        if (!s.texture) continue;
        int fw = s.frame_w(), fh = s.frame_h();
        int aw = s.art_w(), ah = s.art_h();
        int ox = s.art_off_x(), oy = s.art_off_y();
        for (int fi = 0; fi < s.frame_count(); ++fi) {
            const FramePlacement& f = s.frames[fi];
            if (f.x < 0 || f.y < 0) continue;
            ImVec2 tl(origin.x + f.x * z, origin.y + f.y * z);
            ImVec2 br(tl.x + fw * z, tl.y + fh * z);
            // Art sits inside the cell; the gap between them is the margin.
            ImVec2 atl(tl.x + ox * z, tl.y + oy * z);
            ImVec2 abr(atl.x + aw * z, atl.y + ah * z);
            ImVec2 uv0((float)s.frame_src_x(fi) / s.w, (float)s.frame_src_y(fi) / s.h);
            ImVec2 uv1((float)(s.frame_src_x(fi) + aw) / s.w, (float)(s.frame_src_y(fi) + ah) / s.h);
            ImU32 tint = s.pass_missing ? IM_COL32(255, 90, 90, 255) : IM_COL32(255, 255, 255, 255);
            dl->AddImage(tex_id(s.texture), atl, abr, uv0, uv1, tint);
            if (s.has_margin()) dl->AddRect(atl, abr, IM_COL32(120, 255, 160, 90), 0.0f, 0, 1.0f);
            if (i == a.selected_sprite)
                dl->AddRect(tl, br, IM_COL32(255, 220, 80, 255), 0.0f, 0, 2.0f);
            else if (s.locked)
                dl->AddRect(tl, br, IM_COL32(80, 200, 255, 200), 0.0f, 0, 1.0f);
            else
                dl->AddRect(tl, br, IM_COL32(255, 255, 255, 60), 0.0f, 0, 1.0f);
        }
    }

    ImVec2 mp = ImGui::GetIO().MousePos;
    int mpx = (int)((mp.x - origin.x) / z);
    int mpy = (int)((mp.y - origin.y) / z);

    // Find the topmost placed frame under a point. Returns (sprite, frame) or (-1,-1).
    auto frame_at = [&](int px, int py, int& out_s, int& out_f) {
        out_s = out_f = -1;
        for (int i = (int)a.project.sprites.size() - 1; i >= 0; --i) {
            const Sprite& s = a.project.sprites[i];
            int fw = s.frame_w(), fh = s.frame_h();
            for (int fi = 0; fi < s.frame_count(); ++fi) {
                const FramePlacement& f = s.frames[fi];
                if (f.x < 0 || f.y < 0) continue;
                if (px >= f.x && px < f.x + fw && py >= f.y && py < f.y + fh) {
                    out_s = i; out_f = fi; return;
                }
            }
        }
    };

    // Press: select the sprite under the cursor (or clear), and arm a frame drag.
    if (ImGui::IsItemActivated()) {
        int si = -1, fi = -1;
        frame_at(mpx, mpy, si, fi);
        a.selected_sprite = si;
        a.canvas_drag = si;
        a.canvas_drag_frame = fi;
        if (si >= 0) {
            const FramePlacement& f = a.project.sprites[si].frames[fi];
            a.canvas_drag_dx = mpx - f.x;
            a.canvas_drag_dy = mpy - f.y;
        }
    }

    // Drag: move just the grabbed frame, snapping to the sprite's own frame grid.
    if (a.canvas_drag >= 0 && a.canvas_drag < (int)a.project.sprites.size() &&
        a.canvas_drag_frame >= 0 && ImGui::IsItemActive()) {
        Sprite& s = a.project.sprites[a.canvas_drag];
        FramePlacement& f = s.frames[a.canvas_drag_frame];
        int nx = snap(mpx - a.canvas_drag_dx, s.frame_w());
        int ny = snap(mpy - a.canvas_drag_dy, s.frame_h());
        if (f.x != nx || f.y != ny) {
            f.x = nx;
            f.y = ny;
            s.locked = true;
            a.project_dirty = true;
            app::recompute_sheet_dims(a);
        }
    }

    if (ImGui::IsItemDeactivated()) { a.canvas_drag = -1; a.canvas_drag_frame = -1; }

    // Bright outline marking the exact exported sheet bounds.
    if (real_w > 0 && real_h > 0)
        dl->AddRect(origin, ImVec2(origin.x + real_w * z, origin.y + real_h * z),
                    IM_COL32(200, 200, 210, 255), 0.0f, 0, 2.0f);

    ImGui::EndChild();
}

void process_shortcuts(App& a) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return;
    bool ctrl = io.KeyCtrl;
    bool shift = io.KeyShift;
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_N, false)) app::new_project(a);
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) a.open_open_modal = true;
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        if (!a.project_path.empty()) app::save_project(a, a.project_path);
        else a.open_save_as_modal = true;
    }
    if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_S, false)) a.open_save_as_modal = true;
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_E, false)) app::export_all(a);
    if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_E, false)) app::export_manifest(a);
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_A, false)) a.open_add_sprite_modal = true;
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_P, false)) app::auto_pack(a);
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_R, false)) app::report_sheet(a);
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Q, false)) a.quit_requested = true;
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) &&
        a.selected_sprite >= 0 && a.selected_sprite < (int)a.project.sprites.size())
        app::remove_sprite(a, a.selected_sprite);
}

}  // namespace

void draw(App& a) {
    // Single fullscreen host that owns all panels — no floating windows.
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGuiWindowFlags host_flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##host", nullptr, host_flags);
    ImGui::PopStyleVar();

    draw_menu(a);
    process_shortcuts(a);

    const float left_w = (float)a.left_column_width;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float left_h = avail.y;
    float sprites_h   = left_h * 0.40f;
    float inspector_h = left_h * 0.35f;

    ImGui::BeginChild("##left_col", ImVec2(left_w, 0), false);
    ImGui::BeginChild("##sprites_pane", ImVec2(0, sprites_h), true);
    draw_sprite_list(a);
    ImGui::EndChild();
    ImGui::BeginChild("##inspector_pane", ImVec2(0, inspector_h), true);
    draw_inspector(a);
    ImGui::EndChild();
    ImGui::BeginChild("##messages_pane", ImVec2(0, 0), true);
    draw_messages(a);
    ImGui::EndChild();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##canvas_pane", ImVec2(0, 0), true);
    draw_canvas(a);
    ImGui::EndChild();

    ImGui::End();

    draw_file_dialogs(a);
}

}  // namespace ui
