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
};
