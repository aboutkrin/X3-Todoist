#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

#include "DashboardEmoji.h"
#include "fontIds.h"

namespace atlas {
struct Entry {
  uint32_t offset;
  uint16_t glyph;
};
#include "emoji/Atlas.inc"
}  // namespace atlas

int main() {
  using namespace dashboard::emoji;
  // Every key must consume its complete sequence without consuming following text.
  for (const auto& entry : atlas::ENTRIES) {
    const auto* key = reinterpret_cast<const char*>(atlas::KEYS + entry.offset);
    const std::string text = std::string(key) + " next";
    const auto token = next(text.c_str(), text.size());
    assert(token.bytes == strlen(key) && token.glyph == entry.glyph);
    assert((static_cast<size_t>(token.glyph) + 1) * 128 <= sizeof(atlas::BITMAPS));
    for (size_t bytes = 1; bytes < strlen(key); ++bytes) {
      const auto partial = next(key, bytes);
      assert(partial.bytes > 0 && partial.bytes <= bytes);
    }
  }
  for (const char* text : {"A", "1", "#", "*", "Thai", "\u0e44\u0e17\u0e22", "\xe2\x82"}) {
    const auto token = next(text, strlen(text));
    assert(token.glyph == NO_GLYPH && token.bytes > 0);
  }
  assert(next("", 0).bytes == 0);
  for (const char* text : {"\U0001f4da", "\u2764\ufe0f", "\U0001f469\u200d\U0001f4bb", "\U0001f44d\U0001f3fd",
                           "\U0001f1f9\U0001f1ed", "1\ufe0f\u20e3"}) {
    const auto token = next(text, strlen(text));
    assert(token.bytes == strlen(text) && token.glyph != 0 && token.glyph != NO_GLYPH);
  }
  assert(next("\U0001faea", 4).glyph == 0);  // Unicode 17 glyph absent from the pinned font.
  for (int font : {NOTOSANS_14_FONT_ID, NOTOSANS_16_FONT_ID, NOTOSANS_18_FONT_ID}) {
    GfxRenderer renderer;
    const auto style = EpdFontFamily::REGULAR;
    assert(width(renderer, font, "Reading list", style) == renderer.getTextWidth(font, "Reading list", style));
    const char* mixed = "Read \U0001f4da today";
    const int icon = width(renderer, font, "\U0001f4da", style);
    assert(width(renderer, font, mixed, style) ==
           renderer.getTextWidth(font, "Read ", style) + icon + renderer.getTextWidth(font, " today", style));
    draw(renderer, font, 20, 20, mixed, true, style);
    assert(renderer.blackPixels > 0 && renderer.whitePixels == 0);
    draw(renderer, font, 20, 100, mixed, false, style);
    assert(renderer.whitePixels == renderer.blackPixels);
    assert(renderer.svg.str().find("\U0001f4da") == std::string::npos);
  }
  std::cout << "Emoji: all 5225 sequences, prefix bounds, joined/flag/modifier/keycap glyphs, mixed widths and "
               "inversion passed\n";
}
