#include "sizes.h"

namespace sizes {

namespace {
constexpr int kMax = 1 << 20;
}

bool is_tidy(int v) {
    if (v <= 0) return false;
    if (v % 3 == 0) v /= 3;
    return (v & (v - 1)) == 0;
}

int next_up(int v, int unit) {
    if (unit < 1) unit = 1;
    if (v < 1) v = 1;
    for (int i = (v + unit - 1) / unit * unit; i < kMax; i += unit)
        if (is_tidy(i)) return i;
    return v;
}

int prev_down(int v, int unit) {
    if (unit < 1) unit = 1;
    for (int i = v / unit * unit; i > 0; i -= unit)
        if (is_tidy(i)) return i;
    return v;
}

std::vector<int> ladder(int unit, int upto) {
    std::vector<int> out;
    if (unit < 1) unit = 1;
    for (int v = unit; v <= upto; v += unit)
        if (is_tidy(v)) out.push_back(v);
    return out;
}

}  // namespace sizes
