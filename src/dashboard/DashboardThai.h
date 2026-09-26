#pragma once

#include <GfxRenderer.h>

#include <cstddef>
#include <cstdint>

namespace dashboard::thai {
struct Cluster {
  uint32_t plans[4]{};
  uint8_t count = 0;
  uint8_t bytes = 0;
};
// Includes a leading vowel and trailing spacing vowels so neither wraps alone.
Cluster next(const char* text, size_t available);
int layout(const GfxRenderer& renderer, int font, int x, int y, const Cluster& cluster, bool black, bool bold,
           bool paint);
}  // namespace dashboard::thai
