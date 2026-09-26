#pragma once
#include <EpdFontFamily.h>
#include <FontCacheManager.h>

#include <cassert>
#include <cstring>
#include <sstream>
#include <string>

#include "fontIds.h"
class GfxRenderer {
 public:
  FontCacheManager* getFontCacheManager() { return nullptr; }
  void displayBuffer() {}
  mutable std::ostringstream svg;
  mutable size_t blackPixels = 0, whitePixels = 0;
  void drawPixel(int x, int y, bool black = true) const {
    assert(x >= 0 && x < 528 && y >= 0 && y < 792);
    if (black)
      ++blackPixels;
    else
      ++whitePixels;
    svg << "<rect x='" << x << "' y='" << y << "' width='1' height='1' fill='" << (black ? "black" : "white") << "'/>";
  }
  int getScreenWidth() const { return 528; }
  int getScreenHeight() const { return 792; }
  void getOrientedViewableTRBL(int* t, int* r, int* b, int* l) const { *t = *r = *b = *l = 0; }
  int fontSize(int id) const {
    return id == NOTOSANS_18_FONT_ID ? 36 : id == NOTOSANS_16_FONT_ID ? 32 : id == NOTOSANS_14_FONT_ID ? 28 : 24;
  }
  int getLineHeight(int id) const { return fontSize(id) * 5 / 4; }
  int getFontAscenderSize(int id) const { return fontSize(id); }
  int getTextWidth(int id, const char* text, EpdFontFamily::Style style = EpdFontFamily::REGULAR) const {
    double width = 0;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p)
      if ((*p & 0xc0) != 0x80) width += strchr("ilI .,:!", *p) ? .28 : strchr("mwMW", *p) ? .84 : .54;
    return static_cast<int>(width * fontSize(id) * (style == EpdFontFamily::BOLD ? 1.04 : 1));
  }
  void clearScreen() {
    svg.str("");
    svg << "<svg xmlns='http://www.w3.org/2000/svg' width='528' height='792' viewBox='0 0 528 792'><rect width='528' "
           "height='792' fill='white'/>";
  }
  static std::string escape(const char* text) {
    std::string s;
    for (; *text; ++text) {
      if (*text == '&')
        s += "&amp;";
      else if (*text == '<')
        s += "&lt;";
      else if (*text == '>')
        s += "&gt;";
      else
        s += *text;
    }
    return s;
  }
  void drawText(int font, int x, int y, const char* text, bool black = true,
                EpdFontFamily::Style style = EpdFontFamily::REGULAR) const {
    assert(x >= 0 && y >= 0 && y + fontSize(font) <= 792);
    assert(x + getTextWidth(font, text, style) <= 529);
    svg << "<text x='" << x << "' y='" << y + fontSize(font) << "' font-family='Arial,sans-serif' font-size='"
        << fontSize(font) << "' font-weight='" << (style == EpdFontFamily::BOLD ? 700 : 400) << "' fill='"
        << (black ? "black" : "white") << "'>" << escape(text) << "</text>";
  }
  void drawLine(int x, int y, int right, int bottom, bool black = true) const {
    svg << "<path d='M" << x << "," << y << " L" << right << "," << bottom << "' stroke='"
        << (black ? "black" : "white") << "'/>";
  }
  void drawRect(int x, int y, int w, int h, bool black = true) const { rectangle(x, y, w, h, black, false); }
  void fillRect(int x, int y, int w, int h, bool black = true) const { rectangle(x, y, w, h, black, true); }
  void rectangle(int x, int y, int w, int h, bool black, bool fill) const {
    assert(x >= 0 && y >= 0 && w >= 0 && h >= 0 && x + w <= 528 && y + h <= 792);
    svg << "<rect x='" << x << "' y='" << y << "' width='" << w << "' height='" << h << "' fill='"
        << (fill ? (black ? "black" : "white") : "none") << "' stroke='" << (black ? "black" : "white") << "'/>";
  }
};
