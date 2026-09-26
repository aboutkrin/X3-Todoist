#pragma once
#include <GfxRenderer.h>

#include "components/themes/BaseTheme.h"
class UITheme {
 public:
  static UITheme& getInstance() {
    static UITheme instance;
    return instance;
  }
  Rect getScreenSafeArea(const GfxRenderer&, bool) { return Rect{0, 0, 528, 752}; }
  const ThemeMetrics& getMetrics() const { return BaseMetrics::values; }
  static void drawCenteredWrappedText(GfxRenderer&, Rect, int, const char*, int) {}
  static void drawCenteredText(GfxRenderer&, Rect, int, int, const char*) {}
};
struct SetupThemeStub {
  void drawHeader(GfxRenderer&, Rect, const char*) {}
  void drawButtonHints(GfxRenderer&, const char*, const char*, const char*, const char*) {}
};
inline SetupThemeStub setupThemeStub;
#define GUI setupThemeStub
