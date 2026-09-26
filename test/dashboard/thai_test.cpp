#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

#include "DashboardEmoji.h"
#include "DashboardThai.h"
#include "fontIds.h"

namespace atlas {
struct Entry {
  uint32_t key, plan;
};
struct Glyph {
  uint32_t offset;
  uint8_t width, height;
};
#include "thai/Atlas.inc"
}  // namespace atlas

int main() {
  using namespace dashboard;
  for (const auto& entry : atlas::ENTRIES) {
    uint32_t key = entry.key;
    std::string text;
    while (key) {
      const uint32_t cp = 0xe00 + (key & 127);
      char utf8[] = {static_cast<char>(0xe0 | (cp >> 12)), static_cast<char>(0x80 | ((cp >> 6) & 63)),
                     static_cast<char>(0x80 | (cp & 63)), 0};
      text = std::string(utf8) + text;
      key >>= 7;
    }
    const auto cluster = thai::next(text.c_str(), text.size());
    assert(cluster.bytes == text.size() && cluster.count == 1 && cluster.plans[0] == entry.plan);
    for (size_t length = 0; length < text.size(); ++length) assert(thai::next(text.c_str(), length).bytes <= length);
  }
  for (const char* sequence : {"น้ำ", "ปู่", "ซื้", "ญู", "ฐุ", "เก้า", "กำ", "นํ้า", "เกาะ"}) {
    const auto cluster = thai::next(sequence, strlen(sequence));
    assert(cluster.bytes == strlen(sequence));
    const auto token = emoji::next(sequence, strlen(sequence));
    assert(token.thai && token.bytes == strlen(sequence));
  }
  assert(thai::next("English", 7).bytes == 0);
  assert(thai::next("\xe0\xb8", 2).bytes == 0);
  for (int font : {NOTOSANS_14_FONT_ID, NOTOSANS_16_FONT_ID, NOTOSANS_18_FONT_ID}) {
    for (const char* text : {"ประชุมทีม", "ซื้อของ", "อ่านหนังสือ", "น้ำ", "ปู่", "ญู ฐุ", "๑๒๓ ฿", "ไทย English 📚"}) {
      for (auto style : {EpdFontFamily::REGULAR, EpdFontFamily::BOLD}) {
        GfxRenderer renderer;
        const int width = emoji::width(renderer, font, text, style);
        assert(width > 0 && width <= 480);
        emoji::draw(renderer, font, 20, 20, text, true, style);
        assert(renderer.blackPixels > 0 && renderer.whitePixels == 0);
        emoji::draw(renderer, font, 20, 100, text, false, style);
        assert(renderer.blackPixels == renderer.whitePixels);
        // Thai must be emitted as glyph pixels, never unsupported SVG font text.
        assert(renderer.svg.str().find(text) == std::string::npos);
      }
    }
  }
  std::cout
      << "Thai: all 12829 clusters, truncated UTF-8, sara am, vowel/tone stacking, mixed text and inversion passed\n";
}
