#include "DashboardEmoji.h"

#include <algorithm>
#include <cstring>

#include "DashboardThai.h"

namespace dashboard::emoji {
namespace {
struct Entry {
  uint32_t offset;
  uint16_t glyph;
};
#include "emoji/Atlas.inc"
constexpr size_t ENTRY_COUNT = sizeof(ENTRIES) / sizeof(ENTRIES[0]);
constexpr int TILE_SIZE = 32;
constexpr int TILE_BYTES = TILE_SIZE * TILE_SIZE / 8;

size_t bound(size_t first, size_t last, size_t depth, uint8_t byte, bool upper) {
  while (first < last) {
    const size_t middle = first + (last - first) / 2;
    const uint8_t value = KEYS[ENTRIES[middle].offset + depth];
    if (value < byte || (upper && value == byte))
      first = middle + 1;
    else
      last = middle;
  }
  return first;
}
void tile(const GfxRenderer& renderer, uint16_t glyph, int x, int y, int size, bool black) {
  const auto* bitmap = BITMAPS + static_cast<size_t>(glyph) * TILE_BYTES;
  for (int row = 0; row < size; ++row) {
    const int sourceY = row * TILE_SIZE / size;
    for (int col = 0; col < size; ++col) {
      const int bit = sourceY * TILE_SIZE + col * TILE_SIZE / size;
      if (bitmap[bit / 8] & (0x80 >> (bit % 8))) renderer.drawPixel(x + col, y + row, black);
    }
  }
}
int layout(const GfxRenderer& renderer, int font, int x, int y, const char* text, bool black,
           EpdFontFamily::Style style, bool paint) {
  char run[128];
  size_t used = 0, offset = 0;
  const size_t length = strlen(text);
  const int start = x;
  const int size = std::clamp(renderer.getLineHeight(font) * 4 / 5, 16, 36);
  while (offset < length) {
    const Token token = next(text + offset, length - offset);
    if (used && (token.thai || token.glyph != NO_GLYPH || used + token.bytes >= sizeof(run))) {
      run[used] = 0;
      if (paint) renderer.drawText(font, x, y, run, black, style);
      x += renderer.getTextWidth(font, run, style);
      used = 0;
    }
    if (token.thai) {
      x += thai::layout(renderer, font, x, y, thai::next(text + offset, token.bytes), black,
                        style == EpdFontFamily::BOLD, paint);
    } else if (token.glyph == NO_GLYPH) {
      memcpy(run + used, text + offset, token.bytes);
      used += token.bytes;
    } else {
      if (paint) tile(renderer, token.glyph, x, y + (renderer.getLineHeight(font) - size) / 2, size, black);
      x += size + 4;
    }
    offset += token.bytes;
  }
  if (used) {
    run[used] = 0;
    if (paint) renderer.drawText(font, x, y, run, black, style);
    x += renderer.getTextWidth(font, run, style);
  }
  return x - start;
}
}  // namespace

Token next(const char* text, size_t available) {
  Token result{0, NO_GLYPH};
  if (!available || !*text) return result;
  const auto cluster = thai::next(text, available);
  if (cluster.bytes) return {cluster.bytes, NO_GLYPH, true};
  size_t first = 0, last = ENTRY_COUNT;
  for (size_t depth = 0; depth < available && text[depth]; ++depth) {
    const auto byte = static_cast<uint8_t>(text[depth]);
    const size_t begin = bound(first, last, depth, byte, false);
    last = bound(begin, last, depth, byte, true);
    first = begin;
    if (first == last) break;
    if (KEYS[ENTRIES[first].offset + depth + 1] == 0) result = {depth + 1, ENTRIES[first].glyph};
  }
  if (result.bytes) return result;
  const auto lead = static_cast<uint8_t>(*text);
  const size_t bytes = lead < 0x80                    ? 1
                       : lead >= 0xc2 && lead < 0xe0  ? 2
                       : lead < 0xf0 && lead >= 0xe0  ? 3
                       : lead >= 0xf0 && lead <= 0xf4 ? 4
                                                      : 1;
  result.bytes = 1;
  if (bytes > available) return result;
  for (size_t i = 1; i < bytes; ++i)
    if ((static_cast<uint8_t>(text[i]) & 0xc0) != 0x80) return result;
  result.bytes = bytes;
  return result;
}
int width(const GfxRenderer& renderer, int font, const char* text, EpdFontFamily::Style style) {
  return layout(renderer, font, 0, 0, text, true, style, false);
}
void draw(const GfxRenderer& renderer, int font, int x, int y, const char* text, bool black,
          EpdFontFamily::Style style) {
  layout(renderer, font, x, y, text, black, style, true);
}
}  // namespace dashboard::emoji
