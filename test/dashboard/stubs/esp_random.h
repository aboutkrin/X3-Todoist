#pragma once
#include <cstddef>
#include <cstdint>
inline void esp_fill_random(void* out, size_t size) {
  static uint8_t sequence = 0;
  auto* bytes = static_cast<uint8_t*>(out);
  while (size--) *bytes++ = ++sequence;
}
