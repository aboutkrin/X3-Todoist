#include <cassert>
#include <filesystem>
#include <iostream>

#include "DashboardStore.h"
using namespace dashboard;
int main(int argc, char** argv) {
  assert(argc == 2);
  Storage.root = argv[1];
  setenv("TZ", "ICT-7", 1);
  tzset();
  tm t{};
  t.tm_year = 126;
  t.tm_mon = 8;
  t.tm_mday = 26;
  t.tm_hour = 12;
  t.tm_isdst = -1;
  const auto now = mktime(&t);
  ProjectSelection selection;
  assert(selection.add(Project{"p", "Personal"}));
  assert(selection.save());
  Task task{};
  copyText(task.projectId, sizeof(task.projectId), "p");
  copyText(task.id, sizeof(task.id), "first");
  copyText(task.title, sizeof(task.title), "First task");
  copyText(task.due, sizeof(task.due), "2026-09-26");
  {
    Store store;
    assert(store.begin(selection));
    assert(!store.load(now));
    assert(store.startWrite());
    assert(store.append(task));
    assert(store.commit(now));
    assert(store.count(0) == 1);
    assert(store.syncedAt() == now);
    store.releaseIndex();
    assert(store.hasCache());
    assert(store.count(0) == 0);
    assert(store.load(now));
    assert(store.count(0) == 1);
    // Power loss in a later sync must retain the previous committed generation.
    assert(store.startWrite());
    copyText(task.id, sizeof(task.id), "interrupted");
    assert(store.append(task));
  }
  {
    Store store;
    assert(store.begin(selection));
    assert(store.load(now));
    assert(store.count(0) == 1);
    assert(!strcmp(store.entry(0, 0)->id, "first"));
    assert(store.startWrite());
    assert(store.commit(now + 1));
    assert(store.count(0) == 0);
  }
  // Corrupt the newest generation: fall back to the last intact generation.
  {
    std::ofstream broken(Storage.root / ".crosspoint/dashboard-b.bin", std::ios::binary | std::ios::trunc);
    broken << "broken";
  }
  {
    Store store;
    assert(store.begin(selection));
    assert(store.load(now));
    assert(store.count(0) == 1);
    assert(store.load(now + 86400));
    assert(store.count(0) == 1);
    assert(store.startWrite());
    for (size_t i = 0; i < MAX_TASKS; ++i) assert(store.append(task));
    assert(!store.append(task));
  }
  {
    Store store;
    assert(store.begin(selection) && store.load(now));
    const auto previousSync = store.syncedAt();
    const auto previousCount = store.count(0);
    assert(store.startWrite() && store.append(task));
    HalFile::failSync = true;
    assert(!store.commit(now + 1));
    HalFile::failSync = false;
    assert(store.syncedAt() == previousSync && store.count(0) == previousCount);
    store.finishWrite();
  }
  {
    Store store;
    assert(store.begin(selection) && store.load(now));
    assert(store.startWrite() && store.append(task));
    copyText(task.projectId, sizeof(task.projectId), "outside");
    copyText(task.id, sizeof(task.id), "outside-today");
    assert(store.append(task) && store.commit(now));
    assert(store.count(0) == 1 && store.todayCount() == 2);
    assert(store.load(now + 86400));
    assert(store.todayCount() == 2);
    assert(store.todayEntry(0) && store.todayEntry(1));
    copyText(task.projectId, sizeof(task.projectId), "p");
  }
  Config config;
  // A reused generation with a different selected set must not accept an old snapshot.
  assert(selection.add(Project{"q", "Work"}));
  {
    Store store;
    assert(store.begin(selection));
    assert(!store.load(now));
    assert(!store.hasCache());
  }
  --selection.count;
  ++selection.generation;
  {
    Store store;
    assert(store.begin(selection));
    assert(!store.load(now));
  }
  assert(!config.load());
  assert(!config.save("token\r\nheader", 1));
  assert(!config.save("abc", 3));
  assert(config.save("example-token", 2));
  Config loaded;
  assert(loaded.load());
  assert(loaded.fontSize == 2);
  {
    Store store;
    assert(store.begin(selection));
    assert(!store.load(now));  // The initial token cleared the old cache.
    assert(store.startWrite() && store.append(task) && store.commit(now));
    assert(config.save(config.token, 1));
    assert(store.load(now));  // Text size changes retain the cache.
    assert(config.save("another-account-token", 1));
    assert(!store.load(now));
    assert(!store.hasCache() && store.count(0) == 0);
  }
  Pending command;
  assert(!command.load());
  copyText(command.id, sizeof(command.id), "task");
  copyText(command.uuid, sizeof(command.uuid), "stable-command-id");
  assert(command.save());
  Pending rebooted;
  assert(rebooted.load());
  assert(!strcmp(command.uuid, rebooted.uuid));
  rebooted.acknowledged = true;
  assert(rebooted.save());
  Pending acknowledged;
  assert(acknowledged.load());
  assert(acknowledged.acknowledged);
  assert(acknowledged.clear());
  assert(!command.load());
  std::cout << "Dashboard storage: atomic generations, corruption fallback, midnight regrouping, capacity and "
               "persisted completion passed\n";
}
