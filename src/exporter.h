#pragma once

#include "project.h"

namespace exporter {

// Composite all placed sprites onto an RGBA buffer of size sheet_w x sheet_h
// and write to `abs_png_path`. Throws std::runtime_error on failure.
void export_png(const Project& project, const std::string& abs_png_path);

}  // namespace exporter
