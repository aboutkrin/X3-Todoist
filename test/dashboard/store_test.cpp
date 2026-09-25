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
  Task task{};
  copyText(task.id, sizeof(task.id), "first");
  copyText(task.title, sizeof(task.title), "First task");
  copyText(task.due, sizeof(task.due), "2026-09-26");
  {
    Store store;
    assert(store.begin());
    assert(!store.load(now));
    assert(store.startWrite());
    assert(store.append(task));
    assert(store.commit(now));
    assert(store.count(Section::Today) == 1);
    assert(store.syncedAt() == now);
    // Power loss in a later sync must retain the previous committed generation.
    assert(store.startWrite());
    copyText(task.id, sizeof(task.id), "interrupted");
    assert(store.append(task));
  }
  {
    Store store;
    assert(store.begin());
    assert(store.load(now));
    assert(store.count(Section::Today) == 1);
    assert(!strcmp(store.entry(Section::Today, 0)->id, "first"));
    assert(store.startWrite());
    assert(store.commit(now + 1));
    assert(store.count(Section::Today) == 0);
  }
  // Corrupt the newest generation: fall back to the last intact generation.
  {
    std::ofstream broken(Storage.root / ".crosspoint/dashboard-b.bin", std::ios::binary | std::ios::trunc);
    broken << "broken";
  }
  {
    Store store;
    assert(store.begin());
    assert(store.load(now));
    assert(store.count(Section::Today) == 1);
    assert(store.load(now + 86400));
    assert(store.count(Section::Overdue) == 1);
    assert(store.startWrite());
    for (size_t i = 0; i < MAX_TASKS; ++i) assert(store.append(task));
    assert(!store.append(task));
  }
  Config config;
  assert(!config.load());
  assert(!config.save("token\r\nheader", 1));
  assert(!config.save("abc", 3));
  assert(config.save("example-token", 2));
  Config loaded;
  assert(loaded.load());
  assert(loaded.fontSize == 2);
  {
    Store store;
    assert(store.begin());
    assert(!store.load(now));  // The initial token cleared the old cache.
    assert(store.startWrite() && store.append(task) && store.commit(now));
    assert(config.save(config.token, 1));
    assert(store.load(now));  // Text size changes retain the cache.
    assert(config.save("another-account-token", 1));
    assert(!store.load(now));
    assert(!store.hasCache() && store.count(Section::Today) == 0);
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
