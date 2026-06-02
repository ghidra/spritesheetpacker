#pragma once

#include "project.h"

#include <vector>

namespace exporter {

// Composite all placed sprites onto an RGBA buffer of size sheet_w x sheet_h
// and write to `abs_png_path`. Throws std::runtime_error on failure.
//
// By default each sprite is drawn from its own loaded `pixels`. If
// `pixel_override` is non-null it must be indexed by sprite (parallel to
// project.sprites); each entry, when non-null, supplies that sprite's RGBA
// pixels for this pass (size sprite.w*sprite.h*4). A null/empty entry skips
// that sprite. Used to render alternate render passes (_N, _P) at the same
// layout as the base pass.
void export_png(const Project& project, const std::string& abs_png_path,
                const std::vector<const std::vector<unsigned char>*>* pixel_override = nullptr);

}  // namespace exporter
