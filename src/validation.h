#pragma once

#include <string>
#include <vector>

struct Sprite;

namespace validation {

struct Issue {
    std::string sprite_id;
    std::string message;
    bool fatal;
};

// Validate that a source image of (w,h) divides evenly into cols x rows frames.
// Throws std::runtime_error on fatal failure.
void validate_frame_grid(const std::string& sprite_id, int w, int h, int cols, int rows);

// Hard-fail dimension checks on a single frame's dimensions. Returns warnings
// via `warnings`. Throws std::runtime_error with message on fatal failure.
void validate_dimensions(const std::string& sprite_id, int w, int h, int base_unit,
                         std::vector<std::string>& warnings);

// Throws std::runtime_error on fatal failure (non-aligned x/y).
void validate_position(const std::string& sprite_id, int x, int y, int w, int h);

// Returns descriptions of overlapping rectangles. Empty if none.
std::vector<std::string> check_collisions(const std::vector<Sprite>& placed);

// Returns descriptions of out-of-bounds sprites.
std::vector<std::string> check_bounds(const std::vector<Sprite>& placed, int sheet_w, int sheet_h);

}  // namespace validation
