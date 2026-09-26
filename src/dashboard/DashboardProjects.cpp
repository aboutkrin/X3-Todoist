#include "DashboardProjects.h"

#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstring>

namespace dashboard {
namespace {
constexpr uint32_t SELECTION_MAGIC = 0x58505331;
constexpr uint32_t CATALOG_MAGIC = 0x58504331;
constexpr size_t CATALOG_LIMIT = 2000;
const char* selectionPath(int slot) {
  return slot ? "/.crosspoint/dashboard-selection-b.bin" : "/.crosspoint/dashboard-selection-a.bin";
}
bool valid(const Project& project) {
  if (!project.id[0] || !memchr(project.id, 0, sizeof(project.id)) || !memchr(project.name, 0, sizeof(project.name)))
    return false;
  for (const char* p = project.id; *p; ++p)
    if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '-'))
      return false;
  return true;
}
bool readSelection(int slot, ProjectSelection& selection) {
  HalFile file;
  uint32_t magic = 0, crc = 0;
  if (!Storage.openFileForRead("DASH", selectionPath(slot), file) ||
      file.size() != sizeof(selection) + sizeof(magic) + sizeof(crc) ||
      file.read(&magic, sizeof(magic)) != sizeof(magic) || magic != SELECTION_MAGIC ||
      file.read(&selection, sizeof(selection)) != sizeof(selection) || file.read(&crc, sizeof(crc)) != sizeof(crc) ||
      checksum(0xffffffffu, &selection, sizeof(selection)) != crc || selection.count == 0 ||
      selection.count > MAX_PROJECTS || selection.generation == 0)
    return false;
  for (size_t i = 0; i < selection.count; ++i) {
    if (!valid(selection.projects[i])) return false;
    for (size_t j = 0; j < i; ++j)
      if (!strcmp(selection.projects[i].id, selection.projects[j].id)) return false;
  }
  return true;
}
}  // namespace

int ProjectSelection::find(const char* id) const {
  for (size_t i = 0; i < count; ++i)
    if (!strcmp(projects[i].id, id)) return static_cast<int>(i);
  return -1;
}
uint32_t ProjectSelection::fingerprint() const {
  uint32_t crc = checksum(0xffffffffu, &count, sizeof(count));
  for (size_t i = 0; i < count; ++i) crc = checksum(crc, projects[i].id, strlen(projects[i].id) + 1);
  return crc;
}
bool ProjectSelection::add(const Project& project) {
  if (!valid(project) || count >= MAX_PROJECTS || find(project.id) >= 0) return false;
  projects[count++] = project;
  return true;
}
bool ProjectSelection::load() {
  generation = count = 0;
  // A candidate selection exceeds the stack budget; reuse one checked heap block.
  auto candidate = makeUniqueNoThrow<ProjectSelection>();
  if (!candidate) {
    LOG_ERR("DASH", "Selection allocation failed");
    return false;
  }
  for (int slot = 0; slot < 2; ++slot)
    if (readSelection(slot, *candidate) && candidate->generation > generation) *this = *candidate;
  return generation != 0;
}
bool ProjectSelection::save() {
  if (count == 0 || count > MAX_PROJECTS) return false;
  for (size_t i = 0; i < count; ++i) {
    if (!valid(projects[i])) return false;
    for (size_t j = 0; j < i; ++j)
      if (!strcmp(projects[i].id, projects[j].id)) return false;
  }
  std::sort(projects, projects + count, [](const Project& a, const Project& b) {
    const int name = strcmp(a.name, b.name);
    return name ? name < 0 : strcmp(a.id, b.id) < 0;
  });
  auto previous = makeUniqueNoThrow<ProjectSelection>();
  if (!previous) {
    LOG_ERR("DASH", "Selection save allocation failed");
    return false;
  }
  uint32_t latest = 0;
  int active = -1;
  for (int slot = 0; slot < 2; ++slot) {
    if (readSelection(slot, *previous) && previous->generation > latest) {
      latest = previous->generation;
      active = slot;
    }
  }
  if (active >= 0 && readSelection(active, *previous) && previous->count == count &&
      !memcmp(previous->projects, projects, count * sizeof(Project))) {
    generation = latest;
    return true;
  }
  if (latest == UINT32_MAX || !Storage.ensureDirectoryExists("/.crosspoint")) return false;
  generation = latest + 1;
  const uint32_t crc = checksum(0xffffffffu, this, sizeof(*this));
  const int target = active == 0 ? 1 : 0;
  {
    HalFile file;
    if (!Storage.openFileForWrite("DASH", selectionPath(target), file) ||
        file.write(&SELECTION_MAGIC, sizeof(SELECTION_MAGIC)) != sizeof(SELECTION_MAGIC) ||
        file.write(this, sizeof(*this)) != sizeof(*this) || file.write(&crc, sizeof(crc)) != sizeof(crc))
      return false;
    file.flush();
  }
  return readSelection(target, *previous) && previous->generation == generation;
}

const char* ProjectCatalog::path(int slot) {
  return slot ? "/.crosspoint/dashboard-projects-b.bin" : "/.crosspoint/dashboard-projects-a.bin";
}
bool ProjectCatalog::verify(int candidate, Header& result) const {
  HalFile file;
  if (!Storage.openFileForRead("DASH", path(candidate), file) || file.read(&result, sizeof(result)) != sizeof(result) ||
      result.magic != CATALOG_MAGIC || result.count > CATALOG_LIMIT ||
      file.size() != sizeof(result) + result.count * sizeof(Project))
    return false;
  uint32_t crc = 0xffffffffu;
  Project project;
  for (size_t i = 0; i < result.count; ++i) {
    if (file.read(&project, sizeof(project)) != sizeof(project) || !valid(project)) return false;
    crc = checksum(crc, &project, sizeof(project));
  }
  return crc == result.crc;
}
bool ProjectCatalog::load() {
  Header a, b;
  const bool hasA = verify(0, a), hasB = verify(1, b);
  slot = hasA && (!hasB || a.generation >= b.generation) ? 0 : hasB ? 1 : -1;
  header = slot == 0 ? a : slot == 1 ? b : Header{};
  return slot >= 0;
}
bool ProjectCatalog::read(size_t position, Project& project) const {
  HalFile file;
  return slot >= 0 && position < header.count && Storage.openFileForRead("DASH", path(slot), file) &&
         file.seek(sizeof(Header) + position * sizeof(Project)) &&
         file.read(&project, sizeof(project)) == sizeof(project);
}
bool ProjectCatalog::find(const char* id, Project& project) const {
  for (size_t i = 0; i < header.count; ++i)
    if (read(i, project) && !strcmp(project.id, id)) return true;
  return false;
}
bool ProjectCatalog::beginWrite() {
  if (output) output.close();
  load();
  writeSlot = slot == 0 ? 1 : 0;
  writing = Header{};
  writing.generation = header.generation + 1;
  writing.crc = 0xffffffffu;
  return Storage.ensureDirectoryExists("/.crosspoint") && Storage.openFileForWrite("DASH", path(writeSlot), output) &&
         output.write(&writing, sizeof(writing)) == sizeof(writing);
}
bool ProjectCatalog::append(const Project& project) {
  if (!output || writing.count >= CATALOG_LIMIT || !valid(project) ||
      output.write(&project, sizeof(project)) != sizeof(project))
    return false;
  writing.crc = checksum(writing.crc, &project, sizeof(project));
  ++writing.count;
  return true;
}
bool ProjectCatalog::commit() {
  if (!output) return false;
  writing.magic = CATALOG_MAGIC;
  if (!output.seek(0) || output.write(&writing, sizeof(writing)) != sizeof(writing)) return false;
  output.flush();
  output.close();
  Header checked;
  return verify(writeSlot, checked) && load();
}
}  // namespace dashboard
