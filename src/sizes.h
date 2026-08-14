#pragma once

#include <vector>

// The size ladder used for packing cells and for the sheet report's advice.
//
// A size is "tidy" when it is a power of two, optionally times three: 32, 64,
// 96, 128, 192, 256, 384, 512, 768... Any mix of these shares a small common
// multiple, so SheetFit::Shared rounds to something sane. Anything else — a
// second factor of three, or a prime like 5, 7, 11 — drags that factor into the
// multiple and the sheet explodes.
namespace sizes {

bool is_tidy(int v);

// Smallest tidy multiple of `unit` that is >= v. An already-tidy v is returned
// unchanged: cells only ever grow, and an exact fit needs no margin.
int next_up(int v, int unit);

// Largest tidy multiple of `unit` that is <= v. Advice only — never used to
// size a cell, since that would crop art.
int prev_down(int v, int unit);

std::vector<int> ladder(int unit, int upto);

}  // namespace sizes
