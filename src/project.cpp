#include "project.h"

#include "validation.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <stdexcept>

namespace fs = std::filesystem;
using nlohmann::json;

namespace project {

namespace {

int next_pot(int n) {
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}

// LCM accumulator (0 acts as identity). Refuses to grow past `cap` so a
// pathological mix of sizes can't blow the sheet up.
long long lcm_into(long long acc, int v, long long cap) {
    if (v <= 0) return acc;
    if (acc == 0) return v;
    long long g = std::gcd(acc, (long long)v);
    long long r = acc / g * (long long)v;
    return r > cap ? acc : r;
}

long long round_up(long long v, long long g) { return g <= 0 ? v : (v + g - 1) / g * g; }

SheetFit parse_fit(const std::string& s) {
    if (s == "tight") return SheetFit::Tight;
    if (s == "manual") return SheetFit::Manual;
    return SheetFit::Shared;
}

const char* fit_str(SheetFit f) {
    switch (f) {
        case SheetFit::Tight:  return "tight";
        case SheetFit::Manual: return "manual";
        default:               return "shared";
    }
}

}  // namespace

void fit_sheet_dims(Project& p) {
    constexpr long long kCap = 1 << 20;
    long long content_w = 0, content_h = 0, lcm_w = 0, lcm_h = 0;
    for (const Sprite& s : p.sprites) {
        int fw = s.frame_w(), fh = s.frame_h();
        bool placed = false;
        for (int i = 0; i < s.frame_count(); ++i) {
            const FramePlacement& f = s.frames[i];
            if (f.x < 0 || f.y < 0) continue;
            content_w = std::max(content_w, (long long)f.x + fw);
            content_h = std::max(content_h, (long long)f.y + fh);
            placed = true;
        }
        if (placed) {
            lcm_w = lcm_into(lcm_w, fw, kCap);
            lcm_h = lcm_into(lcm_h, fh, kCap);
        }
    }
    long long unit = p.base_unit > 0 ? p.base_unit : 1;
    auto axis = [&](SheetFit fit, long long content, long long lcm, int manual) -> int {
        switch (fit) {
            case SheetFit::Tight:  return (int)round_up(content, unit);
            case SheetFit::Manual: return (int)std::max(content, (long long)manual);
            case SheetFit::Shared:
            default:               return (int)round_up(content, lcm > 0 ? lcm : unit);
        }
    };
    int w = axis(p.width_fit, content_w, lcm_w, p.manual_width);
    int h = axis(p.height_fit, content_h, lcm_h, p.manual_height);
    if (p.pot) {
        w = next_pot(std::max(w, 1));
        h = next_pot(std::max(h, 1));
    }
    p.sheet_w = w;
    p.sheet_h = h;
}

std::string resolve_relative(const std::string& project_path, const std::string& target) {
    fs::path t(target);
    if (t.is_absolute()) return t.lexically_normal().string();
    fs::path base = fs::absolute(fs::path(project_path)).parent_path();
    return (base / t).lexically_normal().string();
}

std::string store_relative(const std::string& project_path, const std::string& target) {
    fs::path abs_t = fs::absolute(fs::path(target)).lexically_normal();
    fs::path base = fs::absolute(fs::path(project_path)).parent_path();
    std::error_code ec;
    fs::path rel = fs::relative(abs_t, base, ec);
    if (ec || rel.empty() || rel.string().rfind("..", 0) == 0)
        return abs_t.string();
    std::string s = rel.generic_string();
    if (s.rfind("./", 0) != 0) s = "./" + s;
    return s;
}

Project load(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot open project file: " + path);
    json j;
    try {
        f >> j;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("invalid JSON in project file: ") + e.what());
    }

    Project p;
    p.version = j.value("version", 1);
    p.base_unit = j.value("baseUnit", 32);
    p.output_rel = j.value("output", std::string("./spritesheet.png"));
    p.manifest_rel = j.value("manifest", std::string("./spritesheet.manifest.json"));
    p.max_sheet_width = j.value("maxSheetWidth", 2048);
    p.pot = j.value("pot", false);
    p.width_fit = parse_fit(j.value("widthFit", std::string("shared")));
    p.height_fit = parse_fit(j.value("heightFit", std::string("shared")));
    p.manual_width = j.value("manualWidth", 0);
    p.manual_height = j.value("manualHeight", 0);

    if (j.contains("sprites") && j["sprites"].is_array()) {
        for (const auto& e : j["sprites"]) {
            Sprite s;
            s.id = e.at("id").get<std::string>();
            s.src = e.at("src").get<std::string>();
            s.frame_cols = e.value("frameCols", 1);
            s.frame_rows = e.value("frameRows", 1);
            if (s.frame_cols < 1) s.frame_cols = 1;
            if (s.frame_rows < 1) s.frame_rows = 1;
            s.ensure_frames();
            if (s.frame_count() == 1) {
                if (e.contains("x") && !e["x"].is_null()) s.frames[0].x = e["x"].get<int>();
                if (e.contains("y") && !e["y"].is_null()) s.frames[0].y = e["y"].get<int>();
            } else if (e.contains("frames") && e["frames"].is_array()) {
                const auto& arr = e["frames"];
                for (int i = 0; i < s.frame_count() && i < (int)arr.size(); ++i) {
                    if (arr[i].is_array() && arr[i].size() == 2) {
                        s.frames[i].x = arr[i][0].get<int>();
                        s.frames[i].y = arr[i][1].get<int>();
                    }
                }
            }
            s.locked = e.value("locked", s.is_placed());
            p.sprites.push_back(std::move(s));
        }
    }
    return p;
}

void save(const std::string& path, const Project& project) {
    json j;
    j["version"] = project.version;
    j["baseUnit"] = project.base_unit;
    j["output"] = project.output_rel;
    j["manifest"] = project.manifest_rel;
    j["maxSheetWidth"] = project.max_sheet_width;
    j["pot"] = project.pot;
    j["widthFit"] = fit_str(project.width_fit);
    j["heightFit"] = fit_str(project.height_fit);
    if (project.width_fit == SheetFit::Manual) j["manualWidth"] = project.manual_width;
    if (project.height_fit == SheetFit::Manual) j["manualHeight"] = project.manual_height;
    j["sprites"] = json::array();
    for (const Sprite& s : project.sprites) {
        json e;
        e["id"] = s.id;
        e["src"] = s.src;
        if (s.frame_cols != 1) e["frameCols"] = s.frame_cols;
        if (s.frame_rows != 1) e["frameRows"] = s.frame_rows;
        if (s.frame_count() == 1) {
            if (s.frames[0].x >= 0 && s.frames[0].y >= 0) {
                e["x"] = s.frames[0].x;
                e["y"] = s.frames[0].y;
            }
        } else if (s.any_placed()) {
            json arr = json::array();
            for (const FramePlacement& f : s.frames) arr.push_back({f.x, f.y});
            e["frames"] = arr;
        }
        if (s.locked && s.is_placed()) e["locked"] = true;
        j["sprites"].push_back(e);
    }
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream out(path);
    if (!out) throw std::runtime_error("cannot write project file: " + path);
    out << j.dump(2) << "\n";
}

void save_manifest(const std::string& path, const Project& project) {
    json j;
    j["version"] = 1;
    j["baseUnit"] = project.base_unit;
    j["sheetWidth"] = project.sheet_w;
    j["sheetHeight"] = project.sheet_h;
    j["sprites"] = json::object();
    for (const Sprite& s : project.sprites) {
        int fw = s.frame_w(), fh = s.frame_h();
        for (int i = 0; i < s.frame_count(); ++i) {
            const FramePlacement& f = s.frames[i];
            if (f.x < 0 || f.y < 0) continue;
            json e;
            e["x"] = f.x;
            e["y"] = f.y;
            e["w"] = fw;
            e["h"] = fh;
            j["sprites"][s.frame_id(i)] = e;
        }
    }
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream out(path);
    if (!out) throw std::runtime_error("cannot write manifest: " + path);
    out << j.dump(2) << "\n";
}

void hydrate(Project& project, const std::string& project_path, SDL_Renderer* renderer,
             std::vector<std::string>& warnings) {
    for (Sprite& s : project.sprites) {
        std::string abs_src = resolve_relative(project_path, s.src);
        if (!sprite_load_image(s, abs_src, renderer))
            throw std::runtime_error(s.id + ": " + s.load_error);
        validation::validate_frame_grid(s.id, s.w, s.h, s.frame_cols, s.frame_rows);
        s.ensure_frames();
        validation::validate_dimensions(s.id, s.frame_w(), s.frame_h(), project.base_unit, warnings);
        for (int i = 0; i < s.frame_count(); ++i) {
            const FramePlacement& f = s.frames[i];
            if (f.x >= 0 && f.y >= 0)
                validation::validate_position(s.frame_id(i), f.x, f.y, s.frame_w(), s.frame_h());
        }
    }
}

}  // namespace project
