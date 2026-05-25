#pragma once

#include <SDL2/SDL.h>
#include <string>
#include <vector>

// Placement of a single frame on the sheet. (-1,-1) means unplaced.
struct FramePlacement {
    int x = -1;
    int y = -1;
};

struct Sprite {
    std::string id;
    std::string src;      // path stored in project (often relative to project file)

    int w = 0;            // full source image width
    int h = 0;            // full source image height

    // The source image is treated as a frame_cols x frame_rows grid. Each frame
    // is (w/frame_cols) x (h/frame_rows) and is packed independently.
    int frame_cols = 1;
    int frame_rows = 1;

    bool locked = false;  // user pinned positions via UI — packer respects

    // Per-frame sheet positions, sized to frame_count(). Index = row*cols + col.
    std::vector<FramePlacement> frames;

    // Runtime-only fields (not serialized)
    SDL_Texture* texture = nullptr;  // owned (full source image)
    std::vector<unsigned char> pixels;  // RGBA8, w*h*4, kept for export
    bool loaded = false;
    std::string load_error;

    int frame_count() const { return frame_cols * frame_rows; }
    int frame_w() const { return frame_cols > 0 ? w / frame_cols : w; }
    int frame_h() const { return frame_rows > 0 ? h / frame_rows : h; }
    int frame_col(int i) const { return frame_cols > 0 ? i % frame_cols : 0; }
    int frame_row(int i) const { return frame_cols > 0 ? i / frame_cols : 0; }
    int frame_src_x(int i) const { return frame_col(i) * frame_w(); }
    int frame_src_y(int i) const { return frame_row(i) * frame_h(); }

    // Manifest/runtime key: bare id for single-frame, id#index for multi-frame.
    std::string frame_id(int i) const {
        return frame_count() == 1 ? id : id + "#" + std::to_string(i);
    }

    bool is_multi() const { return frame_count() > 1; }

    // True only when every frame has a valid position.
    bool is_placed() const {
        if (frames.empty()) return false;
        for (const FramePlacement& f : frames)
            if (f.x < 0 || f.y < 0) return false;
        return true;
    }

    bool any_placed() const {
        for (const FramePlacement& f : frames)
            if (f.x >= 0 && f.y >= 0) return true;
        return false;
    }

    void unplace() {
        for (FramePlacement& f : frames) { f.x = -1; f.y = -1; }
        locked = false;
    }

    // Resize `frames` to match frame_count(), preserving existing entries.
    void ensure_frames() {
        int n = frame_count();
        if (n < 0) n = 0;
        frames.resize((size_t)n);
    }
};

// Load PNG via stb_image, populate w/h/pixels. Returns true on success.
// If `renderer` is non-null, also creates an SDL_Texture for display.
// Sets sprite.load_error on failure.
bool sprite_load_image(Sprite& sprite, const std::string& abs_path, SDL_Renderer* renderer);

// Free GPU texture; safe to call multiple times.
void sprite_free_texture(Sprite& sprite);
