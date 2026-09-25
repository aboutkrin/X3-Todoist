#pragma once
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>
class Preferences {
  std::string space;
  inline static std::map<std::string, std::vector<unsigned char>> values;

 public:
  bool begin(const char* name, bool) {
    space = name;
    return true;
  }
  size_t putBytes(const char* key, const void* data, size_t size) {
    const auto p = static_cast<const unsigned char*>(data);
    values[space + key] = {p, p + size};
    return size;
  }
  size_t getBytesLength(const char* key) { return values[space + key].size(); }
  size_t getBytes(const char* key, void* data, size_t size) {
    auto& bytes = values[space + key];
    if (bytes.size() > size) return 0;
    memcpy(data, bytes.data(), bytes.size());
    return bytes.size();
  }
  size_t getString(const char* key, char* out, size_t size) {
    auto& bytes = values[space + key];
    if (bytes.size() >= size) return 0;
    if (!bytes.empty()) memcpy(out, bytes.data(), bytes.size());
    out[bytes.size()] = 0;
    return bytes.size();
  }
  size_t putString(const char* key, const char* value) { return putBytes(key, value, strlen(value)); }
  uint8_t getUChar(const char* key, uint8_t fallback) {
    auto& bytes = values[space + key];
    return bytes.empty() ? fallback : bytes[0];
  }
  size_t putUChar(const char* key, uint8_t value) { return putBytes(key, &value, 1); }
  int64_t getLong64(const char* key, int64_t fallback) {
    int64_t value = fallback;
    if (getBytesLength(key) == sizeof(value)) getBytes(key, &value, sizeof(value));
    return value;
  }
  size_t putLong64(const char* key, int64_t value) { return putBytes(key, &value, sizeof(value)); }
  bool isKey(const char* key) { return values.count(space + key) > 0; }
  bool remove(const char* key) {
    values.erase(space + key);
    return true;
  }
};
