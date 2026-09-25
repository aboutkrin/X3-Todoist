#pragma once
#include <HalStorage.h>

#include <memory>

#include "DashboardModel.h"

namespace dashboard {
struct CacheHeader {
  uint32_t magic = 0x58445431;
  uint32_t version = 1;
  uint32_t generation = 0;
  uint32_t count = 0;
  int64_t syncedAt = 0;
  uint32_t crc = 0;
};
class Store {
 public:
  bool begin();
  bool load(time_t now);
  bool startWrite();
  bool append(const Task& task);
  bool commit(time_t now);
  void finishWrite() { output.close(); }
  bool read(uint16_t record, Task& task) const;
  const IndexEntry* entry(Section section, size_t offset) const;
  size_t count(Section section) const;
  time_t syncedAt() const { return header.syncedAt; }
  bool hasCache() const { return activeSlot >= 0; }

 private:
  static const char* path(int slot);
  bool verify(int slot, CacheHeader& result);
  // One fixed index arena, allocated per dashboard activity instead of reserving
  // permanent BSS or putting >40KB on the ESP32 loop task's stack.
  std::unique_ptr<IndexEntry[]> index;
  std::unique_ptr<Task> scratch;
  size_t visibleCount = 0;
  CacheHeader header;
  CacheHeader writing;
  int activeSlot = -1;
  int writeSlot = 0;
  HalFile output;
};
struct Config {
  char token[129]{};
  uint8_t fontSize = 1;
  bool load();
  bool save(const char* newToken, uint8_t font);
  bool configured() const { return token[0] != '\0'; }
};
struct Pending {
  char id[64]{};
  char uuid[37]{};
  bool acknowledged = false;
  bool load();
  bool save() const;
  bool clear();
  bool exists() const { return uuid[0] != '\0'; }
};
}  // namespace dashboard
