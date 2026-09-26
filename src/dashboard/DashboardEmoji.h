#pragma once

#include <GfxRenderer.h>

#include <cstddef>
#include <cstdint>

namespace dashboard::emoji {
constexpr uint16_t NO_GLYPH = UINT16_MAX;
struct Token {
  size_t bytes;
  uint16_t glyph;
  bool thai = false;
};
// Longest registered sequence, otherwise one complete UTF-8 character.
Token next(const char* text, size_t available);
int width(const GfxRenderer& renderer, int font, const char* text, EpdFontFamily::Style style);
void draw(const GfxRenderer& renderer, int font, int x, int y, const char* text, bool black,
          EpdFontFamily::Style style);
}  // namespace dashboard::emoji
