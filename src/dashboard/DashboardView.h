#pragma once
#include <cstdint>

#include "DashboardModel.h"
#include "DashboardProjects.h"
namespace dashboard {
enum class Screen { Overview, List, Detail, Confirm, Menu };
constexpr int MAX_VISIBLE_ROWS = 8;
constexpr int rowsPerPage(uint8_t fontSize) { return fontSize == 0 ? 8 : 6; }
struct Row {
  char title[1024]{};
  char project[128]{};
  char due[48]{};
};
static_assert(sizeof(Row) * MAX_VISIBLE_ROWS <= sizeof(Task) * 4);
struct View {
  Screen screen = Screen::Overview;
  size_t project = 0;
  size_t projectCount = 0;
  size_t todayCount = 0;
  bool todayList = false;
  const ProjectSelection* projects = nullptr;
  bool available[MAX_PROJECTS]{};
  uint8_t projectOrder[MAX_PROJECTS]{};
  int selection = 0;
  int count[MAX_PROJECTS]{};
  const Row* rows[MAX_VISIBLE_ROWS]{};
  const Task* detail = nullptr;
  size_t pageStart = 0;
  size_t detailOffset = 0;
  size_t nextDetailOffset = 0;
  uint8_t fontSize = 1;
  bool sleeping = false;
  char date[32]{};
  char status[160]{};
  char detailText[2300]{};
};
}  // namespace dashboard
