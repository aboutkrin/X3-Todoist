#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "DashboardProjects.h"
using namespace dashboard;
int main(int argc, char** argv) {
  assert(argc == 2);
  Storage.root = argv[1];
  ProjectSelection selection;
  assert(!selection.load());
  assert(!selection.save());
  assert(!selection.add(Project{"bad<id", "Bad"}));
  assert(selection.add(Project{"w", "Work"}));
  assert(selection.add(Project{"p", "Personal"}));
  assert(!selection.add(Project{"w", "Duplicate"}));
  assert(selection.save());
  assert(selection.generation == 1);
  assert(!strcmp(selection.projects[0].id, "p"));
  assert(selection.save() && selection.generation == 1);
  ProjectSelection loaded;
  assert(loaded.load() && loaded.count == 2 && loaded.find("w") == 1);
  assert(loaded.add(Project{"r", "Reading"}));
  assert(loaded.save() && loaded.generation == 2);
  {
    std::ofstream broken(Storage.root / ".crosspoint/dashboard-selection-b.bin", std::ios::binary | std::ios::trunc);
    broken << "interrupted";
  }
  assert(loaded.load() && loaded.generation == 1 && loaded.count == 2);
  for (size_t i = loaded.count; i < MAX_PROJECTS; ++i) {
    Project project;
    snprintf(project.id, sizeof(project.id), "project%zu", i);
    assert(loaded.add(project));
  }
  assert(!loaded.add(Project{"overflow", "Overflow"}));
  assert(loaded.save());
  assert(selection.load() && selection.count == MAX_PROJECTS);

  ProjectCatalog catalog;
  assert(!catalog.load());
  assert(catalog.beginWrite());
  assert(catalog.append(Project{"p", "<Personal & Family>"}));
  assert(catalog.append(Project{"w", "Work"}));
  assert(catalog.commit() && catalog.count() == 2);
  Project project;
  assert(catalog.find("p", project) && !strcmp(project.name, "<Personal & Family>"));
  {
    ProjectCatalog interrupted;
    assert(interrupted.beginWrite());
    assert(interrupted.append(Project{"r", "Reading"}));
  }
  assert(catalog.load() && catalog.count() == 2);
  assert(catalog.beginWrite());
  assert(catalog.append(Project{"p", "Renamed"}));
  assert(catalog.commit());
  assert(catalog.find("p", project) && !strcmp(project.name, "Renamed"));
  assert(!catalog.find("w", project));
  assert(catalog.beginWrite());
  assert(catalog.commit() && catalog.count() == 0);
  std::cout << "Project settings and catalogue: limits, stable saves, recovery, rename and empty generations passed\n";
}
