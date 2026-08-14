#include "report.h"

#include "project.h"
#include "sizes.h"
#include "sprite.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <set>

namespace report {

namespace {

constexpr long long kLcmCap = 1 << 20;  // must match project::fit_sheet_dims

template <typename... A>
std::string fmt(const char* f, A... a) {
    char buf[512];
    std::snprintf(buf, sizeof(buf), f, a...);
    return buf;
}

std::string px(long long n) {
    return n < 1000000 ? fmt("%lld px", n) : fmt("%.1f MP", (double)n / 1e6);
}

std::string join(const std::vector<int>& v) {
    std::string s;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) s += ", ";
        s += std::to_string(v[i]);
    }
    return s;
}

// Smallest sheet size that every listed frame size divides into. Mirrors
// project::lcm_into: a size that would push past the cap is skipped rather than
// allowed to run away, so this matches the dimension the tool actually emits.
long long common_multiple(const std::vector<int>& dims, bool* clamped = nullptr) {
    long long acc = 0;
    for (int d : dims) {
        if (d <= 0) continue;
        if (acc == 0) { acc = d; continue; }
        long long r = acc / std::gcd(acc, (long long)d) * (long long)d;
        if (r > kLcmCap) { if (clamped) *clamped = true; continue; }
        acc = r;
    }
    return acc;
}

struct Axis {
    const char* name;
    SheetFit fit;
    int manual;
    std::vector<int> dims;      // distinct frame sizes on this axis
    std::vector<int> awkward;   // the ones that aren't 2^a * 3^b
    long long need = 0;         // sheet must be a multiple of this under Shared
    bool clamped = false;
    long long need_if_fixed = 0;
};

void fill_axis(Axis& ax, const std::set<int>& in_dims, int unit) {
    ax.dims.assign(in_dims.begin(), in_dims.end());
    std::vector<int> repaired;
    for (int d : ax.dims) {
        if (!sizes::is_tidy(d)) {
            ax.awkward.push_back(d);
            int down = sizes::prev_down(d, unit), up = sizes::next_up(d, unit);
            repaired.push_back(d - down <= up - d ? down : up);
        } else {
            repaired.push_back(d);
        }
    }
    ax.need = common_multiple(ax.dims, &ax.clamped);
    ax.need_if_fixed = common_multiple(repaired);
}

// One line per axis describing what its fit rule demands of the sheet.
void describe_axis(std::vector<Line>& out, const Axis& ax, int base_unit, int max_w) {
    const bool is_width = ax.name[0] == 'w';
    switch (ax.fit) {
        case SheetFit::Tight:
            out.push_back({Line::Level::Info,
                           fmt("%s fit=tight: rounds up to a multiple of %d", ax.name, base_unit)});
            break;
        case SheetFit::Manual:
            out.push_back({Line::Level::Info,
                           fmt("%s fit=manual: pinned at %d", ax.name, ax.manual)});
            break;
        case SheetFit::Shared: {
            bool over = is_width && ax.need > max_w;
            out.push_back({over ? Line::Level::Error : Line::Level::Info,
                           fmt("%s fit=shared: rounds up to a multiple of %lld%s%s", ax.name,
                               ax.need, ax.clamped ? " (clamped)" : "",
                               over ? "  <-- larger than the max sheet width" : "")});
            if (is_width && !over && ax.need > 0 && max_w % ax.need != 0)
                out.push_back({Line::Level::Warn,
                               fmt("  max sheet width %d is not a multiple of %lld - shared fit "
                                   "will overshoot it; use %lld or %lld",
                                   max_w, ax.need, max_w / ax.need * ax.need,
                                   (max_w / ax.need + 1) * ax.need)});
            break;
        }
    }
}

}  // namespace

std::vector<Line> sheet(const Project& p) {
    std::vector<Line> out;
    out.push_back({Line::Level::Info, "--- sheet report ---"});

    std::set<int> widths, heights;
    std::set<std::pair<int, int>> distinct;
    // Ink counts art, not cells — otherwise padding would make the sheet look
    // fuller than it is. The size sets are cells, since those are what the fit
    // rules round to.
    long long ink = 0, cells = 0;
    int frames = 0, sprites = 0, padded = 0;
    for (const Sprite& s : p.sprites) {
        int fw = s.frame_w(), fh = s.frame_h();
        if (fw <= 0 || fh <= 0) continue;
        sprites++;
        frames += s.frame_count();
        ink += (long long)s.art_w() * s.art_h() * s.frame_count();
        cells += (long long)fw * fh * s.frame_count();
        if (s.has_margin()) padded++;
        widths.insert(fw);
        heights.insert(fh);
        distinct.insert({fw, fh});
    }
    if (frames == 0) {
        out.push_back({Line::Level::Warn, "no sprites loaded"});
        return out;
    }

    int ideal = (int)std::ceil(std::sqrt((double)ink));
    out.push_back({Line::Level::Info,
                   fmt("%d sprites, %d frames, %d distinct frame sizes", sprites, frames,
                       (int)distinct.size())});
    out.push_back({Line::Level::Info, fmt("art pixels: %s (a perfect square would be %dx%d)",
                                          px(ink).c_str(), ideal, ideal)});
    if (padded > 0)
        out.push_back({Line::Level::Info,
                       fmt("%d sprite%s padded: %s of cells, %s of margin (+%.0f%%)", padded,
                           padded == 1 ? "" : "s", px(cells).c_str(), px(cells - ink).c_str(),
                           100.0 * ((double)cells / (double)ink - 1.0))});

    long long area = (long long)p.sheet_w * p.sheet_h;
    if (area > 0) {
        double full = 100.0 * (double)ink / (double)area;
        out.push_back({full < 60.0 ? Line::Level::Warn : Line::Level::Info,
                       fmt("sheet: %dx%d = %s, %.0f%% full (%s empty)", p.sheet_w, p.sheet_h,
                           px(area).c_str(), full, px(area - ink).c_str())});
    } else {
        out.push_back({Line::Level::Warn, "sheet: not packed yet (Ctrl+P)"});
    }

    Axis w{"width", p.width_fit, p.manual_width, {}, {}, 0, false, 0};
    Axis h{"height", p.height_fit, p.manual_height, {}, {}, 0, false, 0};
    fill_axis(w, widths, p.base_unit);
    fill_axis(h, heights, p.base_unit);
    describe_axis(out, w, p.base_unit, p.max_sheet_width);
    describe_axis(out, h, p.base_unit, p.max_sheet_width);

    // Awkward sizes are the usual cause of a runaway "shared" multiple, so name
    // them, the sprites using them, and what the multiple drops to once fixed.
    std::set<int> awkward(w.awkward.begin(), w.awkward.end());
    awkward.insert(h.awkward.begin(), h.awkward.end());
    if (!awkward.empty()) {
        out.push_back({Line::Level::Warn,
                       fmt("awkward sizes: %s", join({awkward.begin(), awkward.end()}).c_str())});
        for (int d : awkward)
            out.push_back({Line::Level::Warn,
                           fmt("  %d -> %d or %d", d, sizes::prev_down(d, p.base_unit),
                               sizes::next_up(d, p.base_unit))});

        std::vector<std::string> ids;
        for (const Sprite& s : p.sprites)
            if (awkward.count(s.frame_w()) || awkward.count(s.frame_h())) ids.push_back(s.id);
        std::string list;
        for (size_t i = 0; i < ids.size() && i < 6; ++i) list += (i ? ", " : "") + ids[i];
        if (ids.size() > 6) list += fmt(", and %d more", (int)ids.size() - 6);
        out.push_back({Line::Level::Warn, fmt("  used by %d sprites: %s", (int)ids.size(),
                                              list.c_str())});
        out.push_back({Line::Level::Info,
                       fmt("  resize those and shared fit needs multiples of %lld (width) "
                           "and %lld (height)",
                           w.need_if_fixed, h.need_if_fixed)});
        int biggest = std::max(*widths.rbegin(), *heights.rbegin());
        out.push_back({Line::Level::Info,
                       fmt("  good sizes: %s", join(sizes::ladder(p.base_unit, biggest * 2)).c_str())});
    }

    return out;
}

}  // namespace report
