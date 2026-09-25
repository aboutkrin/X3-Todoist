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
  Pending
};
class TodoistClient {
 public:
  explicit TodoistClient(Config& config) : config(config) {}
  SyncResult sync(Store& store);
  SyncResult complete(const Task& task, Pending& pending, Store& store);
  SyncResult reconcile(Pending& pending, Store& store);

 private:
  Config& config;
  SyncResult connect();
  SyncResult request(const char* path, const char* body = nullptr);
  SyncResult sendPending(Pending& pending);
  SyncResult projects();
  bool projectName(const char* id, char* out, size_t capacity);
};
}  // namespace dashboard
