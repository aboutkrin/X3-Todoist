#pragma once
#include <HalStorage.h>

#include <memory>

#include "DashboardModel.h"
#include "DashboardProjects.h"

namespace dashboard {
struct CacheHeader {
  uint32_t magic = 0x58445431;
  uint32_t version = 2;
  uint32_t selectionGeneration = 0;
  uint32_t selectionFingerprint = 0;
  uint32_t generation = 0;
  uint32_t count = 0;
  int64_t syncedAt = 0;
  uint32_t crc = 0;
};
class Store {
 public:
  bool begin(const ProjectSelection& selected);
  bool load(time_t now);
  void releaseIndex() {
    index.reset();
    indexCapacity = 0;
    visibleCount = 0;
  }
  bool startWrite();
  bool append(const Task& task);
  bool commit(time_t now);
  void finishWrite() {
    if (output) output.close();
  }
  bool read(uint16_t record, Task& task) const;
  const IndexEntry* entry(size_t project, size_t offset) const;
  size_t count(size_t project) const;
  time_t syncedAt() const { return header.syncedAt; }
  bool hasCache() const { return activeSlot >= 0; }

 private:
  const ProjectSelection* selection = nullptr;
  static const char* path(int slot);
  bool verify(int slot, CacheHeader& result);
  // Sized to the cache and released during networking so TLS can use the heap.
  std::unique_ptr<IndexEntry[]> index;
  size_t indexCapacity = 0;
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
