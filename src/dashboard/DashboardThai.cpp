#include "DashboardThai.h"

#include <algorithm>

namespace dashboard::thai {
namespace {
struct Entry {
  uint32_t key, plan;
};
struct Glyph {
  uint32_t offset;
  uint8_t width, height;
};
#include "thai/Atlas.inc"
constexpr uint32_t MISSING = UINT32_MAX;
constexpr size_t ENTRY_COUNT = sizeof(ENTRIES) / sizeof(ENTRIES[0]);
constexpr int EM_SIZE = 32;

uint8_t code(const char* text, size_t length) {
  if (length < 3) return 0;
  const auto* bytes = reinterpret_cast<const uint8_t*>(text);
  if (bytes[0] != 0xe0 || (bytes[1] != 0xb8 && bytes[1] != 0xb9) || (bytes[2] & 0xc0) != 0x80) return 0;
  return ((bytes[1] & 1) << 6) | (bytes[2] & 0x3f);
}
bool mark(uint8_t cp) { return cp == 0x31 || (cp >= 0x34 && cp <= 0x3a) || (cp >= 0x47 && cp <= 0x4e); }
uint32_t find(uint32_t key) {
  size_t first = 0, last = ENTRY_COUNT;
  while (first < last) {
    const size_t middle = first + (last - first) / 2;
    if (ENTRIES[middle].key < key)
      first = middle + 1;
    else
      last = middle;
  }
  return first < ENTRY_COUNT && ENTRIES[first].key == key ? ENTRIES[first].plan : MISSING;
}
int signedByte(uint8_t value) { return value < 128 ? value : static_cast<int>(value) - 256; }
int scale(int value, int size) { return value * size / EM_SIZE; }
void ink(const GfxRenderer& renderer, const Glyph& glyph, int x, int y, int size, bool black, bool bold) {
  if (!glyph.width || !glyph.height) return;
  const int width = std::max(1, scale(glyph.width, size));
  const int height = std::max(1, scale(glyph.height, size));
  for (int row = 0; row < height; ++row)
    for (int col = 0; col < width; ++col) {
      const int bit = row * glyph.height / height * glyph.width + col * glyph.width / width;
      if (!(PIXELS[glyph.offset + bit / 8] & (0x80 >> (bit % 8)))) continue;
      renderer.drawPixel(x + col, y + row, black);
      if (bold) renderer.drawPixel(x + col + 1, y + row, black);
    }
}
}  // namespace

Cluster next(const char* text, size_t available) {
  Cluster result;
  uint8_t cp = code(text, available);
  if (!cp || find(cp) == MISSING) return result;
  if (cp >= 0x40 && cp <= 0x44 && available >= 6) {
    const uint8_t base = code(text + 3, available - 3);
    if (base >= 1 && base <= 0x2e) {
      result.plans[result.count++] = find(cp);
      result.bytes = 3;
      cp = base;
    }
  }
  uint32_t key = cp, plan = find(cp);
  result.bytes += 3;
  if (cp >= 1 && cp <= 0x2e) {
    for (int i = 0; i < 2; ++i) {
      const uint8_t following = code(text + result.bytes, available - result.bytes);
      if (!mark(following)) break;
      const uint32_t candidate = (key << 7) | following;
      const uint32_t shaped = find(candidate);
      if (shaped == MISSING) break;
      key = candidate;
      plan = shaped;
      result.bytes += 3;
    }
    if (code(text + result.bytes, available - result.bytes) == 0x33) {
      const uint32_t shaped = find((key << 7) | 0x33);
      if (shaped != MISSING) {
        plan = shaped;
        result.bytes += 3;
      }
    }
  }
  result.plans[result.count++] = plan;
  while (result.count < 4) {
    const uint8_t following = code(text + result.bytes, available - result.bytes);
    if (following != 0x30 && following != 0x32 && following != 0x45) break;
    result.plans[result.count++] = find(following);
    result.bytes += 3;
  }
  return result;
}

int layout(const GfxRenderer& renderer, int font, int x, int y, const Cluster& cluster, bool black, bool bold,
           bool paint) {
  const int start = x;
  const int ascender = renderer.getFontAscenderSize(font);
  const int height = renderer.getLineHeight(font);
  int size = std::clamp(height * 4 / 5, 16, 36);
  // Keep stacked marks inside the line box, including unusual double-mark input.
  for (size_t i = 0; i < cluster.count; ++i) {
    const auto* plan = PLANS + cluster.plans[i];
    for (size_t j = 0; j < plan[1]; ++j) {
      const auto* step = plan + 2 + j * 3;
      const int top = signedByte(step[2]);
      const int bottom = top + GLYPHS[step[0]].height;
      if (top < 0) size = std::min(size, ascender * EM_SIZE / -top);
      if (bottom > 0) size = std::min(size, (height - ascender) * EM_SIZE / bottom);
    }
  }
  size = std::max(1, size);
  for (size_t i = 0; i < cluster.count; ++i) {
    const auto* plan = PLANS + cluster.plans[i];
    if (paint)
      for (size_t j = 0; j < plan[1]; ++j) {
        const auto* step = plan + 2 + j * 3;
        ink(renderer, GLYPHS[step[0]], x + scale(step[1], size), y + ascender + scale(signedByte(step[2]), size), size,
            black, bold);
      }
    x += std::max(1, scale(plan[0], size)) + (bold ? 1 : 0);
  }
  return x - start;
}
}  // namespace dashboard::thai
