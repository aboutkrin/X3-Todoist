#pragma once
#include <HalStorage.h>

#include "DashboardModel.h"

namespace dashboard {
constexpr size_t MAX_PROJECTS = 16;
struct Project {
  char id[64]{};
  char name[128]{};
};

// Lives with the activity, never on the ESP32 task stack.
struct ProjectSelection {
  uint32_t generation = 0;
  uint32_t count = 0;
  Project projects[MAX_PROJECTS]{};
  bool load();
  bool save();
  uint32_t fingerprint() const;
  int find(const char* id) const;
  bool add(const Project& project);
};

class ProjectCatalog {
 public:
  bool load();
  bool read(size_t position, Project& project) const;
  bool find(const char* id, Project& project) const;
  size_t count() const { return header.count; }
  bool beginWrite();
  bool append(const Project& project);
  bool commit();

 private:
  struct Header {
    uint32_t magic = 0;
    uint32_t generation = 0;
    uint32_t count = 0;
    uint32_t crc = 0;
  } header, writing;
  int slot = -1;
  int writeSlot = 0;
  HalFile output;
  static const char* path(int slot);
  bool verify(int slot, Header& result) const;
};
}  // namespace dashboard
