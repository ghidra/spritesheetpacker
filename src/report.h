#pragma once

#include <string>
#include <vector>

struct Project;

namespace report {

struct Line {
    enum class Level { Info, Warn, Error } level = Level::Info;
    std::string text;
};

// Plain-language audit of the current sheet: how much of it is art, and whether
// the sheet-fit rules can actually be met by the frame sizes in use. Read-only.
std::vector<Line> sheet(const Project& project);

}  // namespace report
