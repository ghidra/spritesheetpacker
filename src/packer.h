#pragma once

#include "project.h"

namespace packer {

// Pack all sprites in `project`. Each frame of each sprite is placed
// independently. Locked sprites keep their frame positions and act as
// obstacles; all other frames are auto-placed into the first free slot aligned
// to their own size, backfilling negative space, within max_sheet_width.
//
// Updates project.sheet_w / sheet_h and per-frame positions. Throws
// std::runtime_error on collision or out-of-bounds.
void pack(Project& project);

}  // namespace packer
