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
    // is art_w() x art_h() and is packed independently.
    int frame_cols = 1;
    int frame_rows = 1;

    // Optional packing cell, 0 = unset (the cell is the art itself). Never
    // smaller than the art — a cell only ever adds margin, never crops. The
    // art sits centered horizontally and anchored to the bottom of the cell.
    // The cell is what gets packed, and what the manifest reports as `rect`.
    int cell_w = 0;
    int cell_h = 0;

    bool locked = false;  // user pinned positions via UI — packer respects

    // Manifest attributes, set per source image (every frame shares them).
    // size: optional 2D value, (0,0) = unset and omitted from the manifest.
    // volume: always written to the manifest. shadow: name of a project
    // shadow-mesh entry (see Project::shadow_meshes), omitted when empty.
    // heights: height-map path written verbatim, omitted when empty.
    int size_x = 0;
    int size_y = 0;
    int volume = 1;
    std::string shadow;
    std::string heights;

    // Per-frame sheet positions, sized to frame_count(). Index = row*cols + col.
    std::vector<FramePlacement> frames;

    // Runtime-only fields (not serialized)
    SDL_Texture* texture = nullptr;  // owned (full source image, current preview pass)
    std::vector<unsigned char> pixels;  // RGBA8, w*h*4, base pass, kept for export
    bool loaded = false;
    bool pass_missing = false;  // previewed pass variant absent — base shown tinted
    std::string load_error;

    int frame_count() const { return frame_cols * frame_rows; }

    // Art: one frame's real pixels in the source image. Used for reading and
    // blitting the bitmap.
    int art_w() const { return frame_cols > 0 ? w / frame_cols : w; }
    int art_h() const { return frame_rows > 0 ? h / frame_rows : h; }

    // Cell: the slot the frame occupies on the sheet. Everything that reasons
    // about sheet layout — packer, sheet fitting, collisions, manifest rects —
    // goes through these, so padding is transparent to all of it.
    int frame_w() const { return cell_w > art_w() ? cell_w : art_w(); }
    int frame_h() const { return cell_h > art_h() ? cell_h : art_h(); }

    // Where the art sits inside its cell. Widths are forced even upstream
    // (validation::validate_dimensions), so the horizontal centering is exact.
    int art_off_x() const { return (frame_w() - art_w()) / 2; }
    int art_off_y() const { return frame_h() - art_h(); }

    bool has_margin() const { return frame_w() != art_w() || frame_h() != art_h(); }

    int frame_col(int i) const { return frame_cols > 0 ? i % frame_cols : 0; }
    int frame_row(int i) const { return frame_cols > 0 ? i / frame_cols : 0; }
    int frame_src_x(int i) const { return frame_col(i) * art_w(); }
    int frame_src_y(int i) const { return frame_row(i) * art_h(); }

    // Manifest/runtime key: bare id for single-frame, id_index for multi-frame.
    std::string frame_id(int i) const {
        return frame_count() == 1 ? id : id + "_" + std::to_string(i);
    }

    bool is_multi() const { return frame_count() > 1; }

    bool has_size() const { return size_x > 0 && size_y > 0; }

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

// (Re)create the display texture from sprite.pixels (the base pass). Used to
// restore the base appearance after previewing another pass.
bool sprite_texture_from_pixels(Sprite& sprite, SDL_Renderer* renderer);

// Free GPU texture; safe to call multiple times.
void sprite_free_texture(Sprite& sprite);
