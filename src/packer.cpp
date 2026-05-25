#include "packer.h"

#include "validation.h"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace packer {

namespace {

// One frame waiting to be placed.
struct Item {
    Sprite* sprite;
    int fi;
    int w;
    int h;
};

// Occupancy grid in units of `cell` pixels. Grows downward (rows) on demand.
struct Grid {
    int cols = 0;
    std::vector<uint8_t> cells;  // row-major, size = rows*cols

    int rows() const { return cols ? (int)(cells.size() / cols) : 0; }
    void ensure_rows(int r) {
        if (r > rows()) cells.resize((size_t)r * cols, 0);
    }
    bool block_free(int c0, int r0, int cw, int ch) {
        ensure_rows(r0 + ch);
        for (int r = r0; r < r0 + ch; ++r)
            for (int c = c0; c < c0 + cw; ++c)
                if (cells[(size_t)r * cols + c]) return false;
        return true;
    }
    void fill(int c0, int r0, int cw, int ch) {
        ensure_rows(r0 + ch);
        for (int r = r0; r < r0 + ch; ++r)
            for (int c = c0; c < c0 + cw; ++c)
                cells[(size_t)r * cols + c] = 1;
    }
};

}  // namespace

void pack(Project& project) {
    // Partition sprites into locked obstacles vs auto-placeable.
    std::vector<Sprite*> locked, autop;
    for (Sprite& s : project.sprites) {
        s.ensure_frames();
        if (s.frame_count() <= 0) continue;
        if (s.locked && s.is_placed()) locked.push_back(&s);
        else { s.unplace(); autop.push_back(&s); }
    }

    // Validate locked frame positions.
    for (Sprite* s : locked)
        for (int i = 0; i < s->frame_count(); ++i)
            validation::validate_position(s->frame_id(i), s->frames[i].x, s->frames[i].y,
                                          s->frame_w(), s->frame_h());

    // Build the auto item list.
    std::vector<Item> items;
    for (Sprite* s : autop)
        for (int i = 0; i < s->frame_count(); ++i)
            items.push_back({s, i, s->frame_w(), s->frame_h()});

    // Cell size = gcd of every frame dimension (auto + locked). All sheet
    // positions are multiples of each frame's own size, hence of this cell,
    // so a single occupancy grid can represent every per-size alignment.
    int cell = 0;
    auto fold = [&](int v) { cell = std::gcd(cell, v); };
    for (const Item& it : items) { fold(it.w); fold(it.h); }
    for (Sprite* s : locked) { fold(s->frame_w()); fold(s->frame_h()); }
    if (cell <= 0) cell = 1;

    const int max_w = project.max_sheet_width;
    int place_cols = max_w / cell;          // columns available for auto placement
    if (place_cols < 1) place_cols = 1;

    // Grid must also span any locked frame that extends past max_sheet_width.
    int grid_cols = place_cols;
    for (Sprite* s : locked)
        for (int i = 0; i < s->frame_count(); ++i) {
            int extent = (s->frames[i].x + s->frame_w() + cell - 1) / cell;
            grid_cols = std::max(grid_cols, extent);
        }

    Grid grid;
    grid.cols = grid_cols;

    // Mark locked frames as occupied obstacles.
    for (Sprite* s : locked)
        for (int i = 0; i < s->frame_count(); ++i)
            grid.fill(s->frames[i].x / cell, s->frames[i].y / cell,
                      s->frame_w() / cell, s->frame_h() / cell);

    // Largest-area first; deterministic tiebreak so packing is stable.
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        long long aa = (long long)a.w * a.h, bb = (long long)b.w * b.h;
        if (aa != bb) return aa > bb;
        if (a.h != b.h) return a.h > b.h;
        if (a.w != b.w) return a.w > b.w;
        if (a.sprite->id != b.sprite->id) return a.sprite->id < b.sprite->id;
        return a.fi < b.fi;
    });

    for (const Item& it : items) {
        int cw = it.w / cell, ch = it.h / cell;
        if (cw < 1) cw = 1;
        if (ch < 1) ch = 1;
        if (cw > place_cols)
            throw std::runtime_error(it.sprite->frame_id(it.fi) +
                                     ": frame width exceeds max sheet width");

        // Scan top-to-bottom, snapping to this frame's own grid (col % cw == 0,
        // row % ch == 0). First free aligned slot wins, which backfills holes.
        bool placed = false;
        for (int r0 = 0; !placed; r0 += ch) {
            for (int c0 = 0; c0 + cw <= place_cols; c0 += cw) {
                if (grid.block_free(c0, r0, cw, ch)) {
                    grid.fill(c0, r0, cw, ch);
                    it.sprite->frames[it.fi].x = c0 * cell;
                    it.sprite->frames[it.fi].y = r0 * cell;
                    placed = true;
                    break;
                }
            }
        }
    }

    // Tightest grid-aligned sheet dims from all placed frames.
    project::fit_sheet_dims(project);

    // Final verification.
    auto coll = validation::check_collisions(project.sprites);
    if (!coll.empty()) {
        std::string msg = "packer produced collisions:";
        for (auto& c : coll) msg += "\n  " + c;
        throw std::runtime_error(msg);
    }
    auto oob = validation::check_bounds(project.sprites, project.sheet_w, project.sheet_h);
    if (!oob.empty()) {
        std::string msg = "out of bounds:";
        for (auto& o : oob) msg += "\n  " + o;
        throw std::runtime_error(msg);
    }
}

}  // namespace packer
