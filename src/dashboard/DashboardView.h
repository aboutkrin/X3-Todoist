#pragma once
#include <cstdint>

#include "DashboardModel.h"
#include "DashboardProjects.h"
namespace dashboard {
enum class Screen { Overview, List, Detail, Confirm, Menu };
struct View {
  Screen screen = Screen::Overview;
  size_t project = 0;
  size_t projectCount = 0;
  const ProjectSelection* projects = nullptr;
  bool available[MAX_PROJECTS]{};
  uint8_t projectOrder[MAX_PROJECTS]{};
  int selection = 0;
  int count[MAX_PROJECTS]{};
  const Task* rows[4]{};
  const Task* detail = nullptr;
  size_t pageStart = 0;
  size_t detailOffset = 0;
  size_t nextDetailOffset = 0;
  uint8_t fontSize = 1;
  char date[32]{};
  char status[160]{};
  char detailText[2300]{};
};
}  // namespace dashboard
