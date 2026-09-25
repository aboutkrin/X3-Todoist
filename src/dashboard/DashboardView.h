#pragma once
#include <cstdint>

#include "DashboardModel.h"
namespace dashboard {
enum class Screen { Overview, List, Detail, Confirm, Menu };
struct View {
  Screen screen = Screen::Overview;
  Section section = Section::Today;
  int selection = 0;
  int count[3]{};
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
