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
  SyncResult connect();
  SyncResult request(const char* path, const char* body = nullptr);
  SyncResult sendPending(Pending& pending);
  SyncResult projects();
  bool projectName(const char* id, char* out, size_t capacity);
};
}  // namespace dashboard
