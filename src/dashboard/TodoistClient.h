#pragma once
#include <ArduinoJson.h>

#include "DashboardStore.h"

namespace dashboard {
enum class SyncResult {
  Ok,
  NotConfigured,
  Offline,
  Clock,
  Auth,
  RateLimited,
  Transport,
  InvalidData,
  StorageError,
  Limit,
  Pending,
  ChooseProjects
};
struct SectionEntry {
  char id[64]{};
  char name[48]{};
  uint16_t order = 0;
};
// Sections of one project; heap-allocated for the duration of a sync.
struct SectionTable {
  static constexpr size_t CAPACITY = 24;
  SectionEntry items[CAPACITY];
  size_t count = 0;
  const SectionEntry* find(const char* id) const {
    for (size_t i = 0; i < count; ++i)
      if (!strcmp(items[i].id, id)) return &items[i];
    return nullptr;
  }
};
class TodoistClient {
 public:
  explicit TodoistClient(Config& config, const ProjectSelection& selection) : config(config), selection(selection) {}
  SyncResult refreshProjects();
  SyncResult sync(Store& store);
  SyncResult complete(const Task& task, Pending& pending, Store& store);
  SyncResult reconcile(Pending& pending, Store& store);

 private:
  Config& config;
  const ProjectSelection& selection;
  const char* responsePath = nullptr;
  SyncResult connect();
  SyncResult request(const char* path, const char* body = nullptr);
  SyncResult sendPending(Pending& pending);
  SyncResult projects();
  SyncResult sections(const char* projectId, SectionTable& table);
  bool projectName(const char* id, char* out, size_t capacity);
};
}  // namespace dashboard
