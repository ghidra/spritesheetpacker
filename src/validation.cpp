#include "validation.h"

#include "sprite.h"

#include <stdexcept>
#include <string>

namespace validation {

void validate_frame_grid(const std::string& sprite_id, int w, int h, int cols, int rows) {
    if (cols < 1 || rows < 1)
        throw std::runtime_error(sprite_id + ": frame grid must be at least 1x1 (got " +
                                 std::to_string(cols) + "x" + std::to_string(rows) + ")");
    if (w % cols != 0)
        throw std::runtime_error(sprite_id + ": width " + std::to_string(w) +
                                 " is not divisible by " + std::to_string(cols) + " frame columns");
    if (h % rows != 0)
        throw std::runtime_error(sprite_id + ": height " + std::to_string(h) +
                                 " is not divisible by " + std::to_string(rows) + " frame rows");
}

void validate_dimensions(const std::string& sprite_id, int w, int h, int base_unit,
                         std::vector<std::string>& warnings) {
    if (w <= 0 || h <= 0)
        throw std::runtime_error(sprite_id + ": dimensions must be positive (got " +
                                 std::to_string(w) + "x" + std::to_string(h) + ")");
    if (w % 2 != 0)
        throw std::runtime_error(sprite_id + ": width must be even for UV symmetry (got " +
                                 std::to_string(w) + ")");
    if (w % 8 != 0)
        throw std::runtime_error(sprite_id + ": width must be a multiple of 8 (got " +
                                 std::to_string(w) + ")");
    if (h % 8 != 0)
        throw std::runtime_error(sprite_id + ": height must be a multiple of 8 (got " +
                                 std::to_string(h) + ")");
    if (w % base_unit != 0 && base_unit % w != 0)
        warnings.push_back(sprite_id + ": width " + std::to_string(w) +
                           " is not a multiple of base unit " + std::to_string(base_unit) +
                           " (still valid)");
    if (h % base_unit != 0 && base_unit % h != 0)
        warnings.push_back(sprite_id + ": height " + std::to_string(h) +
                           " is not a multiple of base unit " + std::to_string(base_unit) +
                           " (still valid)");
}

void validate_position(const std::string& sprite_id, int x, int y, int w, int h) {
    if (x < 0 || y < 0)
        throw std::runtime_error(sprite_id + ": position must be non-negative (got " +
                                 std::to_string(x) + "," + std::to_string(y) + ")");
    if (x % w != 0)
        throw std::runtime_error(sprite_id + ": x=" + std::to_string(x) +
                                 " is not a multiple of own width " + std::to_string(w));
    if (y % h != 0)
        throw std::runtime_error(sprite_id + ": y=" + std::to_string(y) +
                                 " is not a multiple of own height " + std::to_string(h));
}

static bool rects_overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

namespace {
struct PlacedRect { int x, y, w, h; std::string label; };

std::vector<PlacedRect> expand_frames(const std::vector<Sprite>& sprites) {
    std::vector<PlacedRect> out;
    for (const Sprite& s : sprites) {
        for (int i = 0; i < s.frame_count(); ++i) {
            const FramePlacement& f = s.frames[i];
            if (f.x < 0 || f.y < 0) continue;
            out.push_back({f.x, f.y, s.frame_w(), s.frame_h(), s.frame_id(i)});
        }
    }
    return out;
}
}  // namespace

std::vector<std::string> check_collisions(const std::vector<Sprite>& placed) {
    std::vector<std::string> out;
    std::vector<PlacedRect> rects = expand_frames(placed);
    for (size_t i = 0; i < rects.size(); ++i)
        for (size_t j = i + 1; j < rects.size(); ++j)
            if (rects_overlap(rects[i].x, rects[i].y, rects[i].w, rects[i].h,
                              rects[j].x, rects[j].y, rects[j].w, rects[j].h))
                out.push_back("collision: " + rects[i].label + " overlaps " + rects[j].label);
    return out;
}

std::vector<std::string> check_bounds(const std::vector<Sprite>& placed, int sheet_w, int sheet_h) {
    std::vector<std::string> out;
    for (const PlacedRect& r : expand_frames(placed)) {
        if (r.x + r.w > sheet_w || r.y + r.h > sheet_h)
            out.push_back(r.label + ": extends beyond sheet bounds (" +
                          std::to_string(sheet_w) + "x" + std::to_string(sheet_h) + ")");
    }
    return out;
}

}  // namespace validation
