#include "DashboardStore.h"

#include <Logging.h>
#include <Memory.h>
#include <Preferences.h>

#include <algorithm>
#include <cstring>

namespace dashboard {
const char* Store::path(int slot) {
  return slot == 0 ? "/.crosspoint/dashboard-a.bin" : "/.crosspoint/dashboard-b.bin";
}
bool Store::begin(const ProjectSelection& selected) {
  selection = &selected;
  scratch = makeUniqueNoThrow<Task>();
  if (!scratch) {
    LOG_ERR("DASH", "Cache arena allocation failed");
    return false;
  }
  return Storage.ensureDirectoryExists("/.crosspoint");
}
bool Store::verify(int slot, CacheHeader& result) {
  HalFile file;
  if (!Storage.openFileForRead("DASH", path(slot), file)) return false;
  if (!selection || !selection->count || file.read(&result, sizeof(result)) != sizeof(result) ||
      result.magic != 0x58445431 || result.version != 2 || result.selectionGeneration != selection->generation ||
      result.selectionFingerprint != selection->fingerprint() || result.count > MAX_TASKS ||
      file.size() != sizeof(result) + result.count * sizeof(Task))
    return false;
  uint32_t crc = 0xffffffffu;
  for (uint32_t i = 0; i < result.count; ++i) {
    if (file.read(scratch.get(), sizeof(Task)) != sizeof(Task)) return false;
    crc = checksum(crc, scratch.get(), sizeof(Task));
    // Guard all strings before any rendering or date parsing after a power loss.
    if (!memchr(scratch->id, 0, sizeof(scratch->id)) || !memchr(scratch->title, 0, sizeof(scratch->title)) ||
        !memchr(scratch->project, 0, sizeof(scratch->project)) || !memchr(scratch->due, 0, sizeof(scratch->due)) ||
        !memchr(scratch->description, 0, sizeof(scratch->description)) ||
        !memchr(scratch->parentId, 0, sizeof(scratch->parentId)) ||
        !memchr(scratch->projectId, 0, sizeof(scratch->projectId)) || selection->find(scratch->projectId) < 0)
      return false;
  }
  return (crc ^ 0xffffffffu) == result.crc;
}
bool Store::load(time_t now) {
  (void)now;
  if (!scratch) return false;
  CacheHeader a, b;
  const bool hasA = verify(0, a), hasB = verify(1, b);
  activeSlot = hasA && (!hasB || a.generation >= b.generation) ? 0 : hasB ? 1 : -1;
  visibleCount = 0;
  if (activeSlot < 0) {
    releaseIndex();
    header = CacheHeader{};
    return false;
  }
  header = activeSlot == 0 ? a : b;
  if (header.count == 0) {
    releaseIndex();
    return true;
  }
  if (indexCapacity < header.count) {
    releaseIndex();
    index = makeUniqueNoThrow<IndexEntry[]>(header.count);
    if (!index) {
      LOG_ERR("DASH", "Cache index allocation failed: %u records", static_cast<unsigned>(header.count));
      return false;
    }
    indexCapacity = header.count;
  }
  for (uint16_t i = 0; i < header.count; ++i) {
    if (!read(i, *scratch)) return false;
    const int project = selection->find(scratch->projectId);
    if (project < 0) continue;
    auto& item = index[visibleCount++];
    item.record = i;
    item.section = Section::Today;
    item.projectIndex = project;
    item.priority = scratch->priority;
    copyText(item.id, sizeof(item.id), scratch->id);
    int day = 0;
    if (!parseDue(scratch->due, item.rank, day)) item.rank = INT64_MAX;
  }
  std::sort(index.get(), index.get() + visibleCount, before);
  return true;
}
bool Store::startWrite() {
  if (!selection || !selection->count || !selection->generation) return false;
  finishWrite();
  writeSlot = activeSlot == 0 ? 1 : 0;
  writing = CacheHeader{};
  writing.selectionGeneration = selection->generation;
  writing.selectionFingerprint = selection->fingerprint();
  writing.magic = 0;  // An interrupted generation must never become active.
  writing.generation = header.generation + 1;
  writing.crc = 0xffffffffu;
  LOG_INF("DASH", "Task cache write begin: slot=%d generation=%u", writeSlot, writing.generation);
  return Storage.openFileForWrite("DASH", path(writeSlot), output) &&
         output.write(&writing, sizeof(writing)) == sizeof(writing);
}
bool Store::append(const Task& task) {
  if (!output || writing.count >= MAX_TASKS) return false;
  if (output.write(&task, sizeof(task)) != sizeof(task)) return false;
  writing.crc = checksum(writing.crc, &task, sizeof(task));
  ++writing.count;
  return true;
}
bool Store::commit(time_t now) {
  if (!output) return false;
  writing.magic = 0x58445431;
  writing.crc ^= 0xffffffffu;
  writing.syncedAt = now;
  if (!output.seek(0) || output.write(&writing, sizeof(writing)) != sizeof(writing)) return false;
  if (!output.sync()) {
    LOG_ERR("DASH", "Task cache sync failed: slot=%d records=%u", writeSlot, writing.count);
    return false;
  }
  if (!output.close()) {
    LOG_ERR("DASH", "Task cache close failed: slot=%d", writeSlot);
    return false;
  }
  CacheHeader checked;
  if (!verify(writeSlot, checked)) {
    LOG_ERR("DASH", "Task cache verification failed: slot=%d", writeSlot);
    return false;
  }
  LOG_INF("DASH", "Task cache verified: slot=%d generation=%u records=%u", writeSlot, checked.generation,
          checked.count);
  return load(now);
}
bool Store::read(uint16_t record, Task& task) const {
  HalFile file;
  return activeSlot >= 0 && record < header.count && Storage.openFileForRead("DASH", path(activeSlot), file) &&
         file.seek(sizeof(CacheHeader) + record * sizeof(Task)) && file.read(&task, sizeof(task)) == sizeof(task);
}
const IndexEntry* Store::entry(size_t project, size_t offset) const {
  for (size_t i = 0; i < visibleCount; ++i)
    if (index[i].projectIndex == project && offset-- == 0) return &index[i];
  return nullptr;
}
size_t Store::count(size_t project) const {
  size_t result = 0;
  for (size_t i = 0; i < visibleCount; ++i) result += index[i].projectIndex == project;
  return result;
}
bool Config::load() {
  Preferences prefs;
  if (!prefs.begin("dashboard", true)) return false;
  prefs.getString("token", token, sizeof(token));
  fontSize = std::min<uint8_t>(2, prefs.getUChar("font", 1));
  return configured();
}
bool Config::save(const char* newToken, uint8_t font) {
  if (!newToken || !*newToken || strlen(newToken) >= sizeof(token) || font > 2) return false;
  for (const char* p = newToken; *p; ++p)
    if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '-' || *p == '_'))
      return false;
  if (strcmp(token, newToken)) {
    for (const char* path : {"/.crosspoint/dashboard-a.bin", "/.crosspoint/dashboard-b.bin",
                             "/.crosspoint/dashboard-response.json", "/.crosspoint/dashboard-projects.bin",
                             "/.crosspoint/dashboard-projects-a.bin", "/.crosspoint/dashboard-projects-b.bin",
                             "/.crosspoint/dashboard-selection-a.bin", "/.crosspoint/dashboard-selection-b.bin"}) {
      if (Storage.exists(path) && !Storage.remove(path)) {
        LOG_ERR("DASH", "Failed to clear previous account cache");
        return false;
      }
    }
  }
  Preferences prefs;
  if (!prefs.begin("dashboard", false)) return false;
  if (strcmp(token, newToken) && prefs.putString("token", newToken) != strlen(newToken)) return false;
  if (prefs.getUChar("font", 255) != font && prefs.putUChar("font", font) != 1) return false;
  copyText(token, sizeof(token), newToken);
  fontSize = font;
  return true;
}
bool Pending::load() {
  Preferences prefs;
  *this = Pending{};
  if (!prefs.begin("dash-pending", true)) return false;
  if (prefs.getBytesLength("command") != sizeof(Pending)) return false;
  if (prefs.getBytes("command", this, sizeof(Pending)) != sizeof(Pending) || !memchr(id, 0, sizeof(id)) ||
      !memchr(uuid, 0, sizeof(uuid))) {
    *this = Pending{};
    return false;
  }
  return exists();
}
bool Pending::save() const {
  Preferences prefs;
  return prefs.begin("dash-pending", false) && prefs.putBytes("command", this, sizeof(Pending)) == sizeof(Pending);
}
bool Pending::clear() {
  Preferences prefs;
  if (!prefs.begin("dash-pending", false)) return false;
  if (prefs.isKey("command") && !prefs.remove("command")) return false;
  *this = Pending{};
  return true;
}
}  // namespace dashboard
