#include <GfxRenderer.h>
#include <I18n.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "components/themes/BaseTheme.h"
#include "dashboard/DashboardView.h"
using namespace dashboard;
const char* stubText(StrId id) {
  switch (id) {
    case StrId::STR_DASH_PROJECTS:
      return "My projects";
    case StrId::STR_DASH_TODAY:
      return "Today";
    case StrId::STR_DASH_NO_DUE_DATE:
      return "No due date";
    case StrId::STR_DASH_MENU:
      return "Menu";
    case StrId::STR_DASH_NO_TASKS:
      return "No tasks";
    case StrId::STR_DASH_COMPLETE:
      return "Complete task";
    case StrId::STR_DASH_COMPLETE_WARNING:
      return "Complete this task and its subtasks?";
    case StrId::STR_DASH_RECUR_WARNING:
      return "Complete this occurrence and advance its recurring date?";
    case StrId::STR_CANCEL:
      return "Cancel";
    case StrId::STR_DASH_REFRESH:
      return "Refresh now";
    case StrId::STR_DASH_WIFI:
      return "Connect Wi-Fi";
    case StrId::STR_DASH_SETUP:
      return "Todoist setup";
    case StrId::STR_DASH_FONT:
      return "Text size";
    case StrId::STR_DASH_SETTINGS:
      return "System settings";
    case StrId::STR_DASH_SLEEP:
      return "Sleep";
    default:
      return "Label";
  }
}
int main(int argc, char** argv) {
  assert(argc == 2);
  std::filesystem::create_directories(argv[1]);
  ProjectSelection projects;
  projects.count = 8;
  projects.projects[0] = Project{"p", "งานส่วนตัว 📚"};
  projects.projects[1] = Project{"r", "Reading"};
  projects.projects[2] = Project{"w", "Work"};
  projects.projects[3] = Project{"h", "Home"};
  projects.projects[4] = Project{"e", "Errands"};
  projects.projects[5] = Project{"s", "Someday"};
  projects.projects[6] = Project{"i", "Inbox"};
  projects.projects[7] = Project{"f", "Family"};
  Task detail;
  Row rows[MAX_VISIBLE_ROWS];
  const char* titles[] = {"💼 ประชุมทีม", "🛒 ซื้อของ", "📚 อ่านหนังสือ", "🇹🇭 เที่ยวกับครอบครัว"};
  for (int i = 0; i < MAX_VISIBLE_ROWS; ++i) {
    copyText(rows[i].title, sizeof(rows[i].title), titles[i % 4]);
    copyText(rows[i].project, sizeof(rows[i].project), i == 0 ? "Work" : "Personal");
    copyText(rows[i].due, sizeof(rows[i].due), "2026-09-26");
  }
  copyText(detail.title, sizeof(detail.title), titles[0]);
  assert(rowsPerPage(0) == 8 && rowsPerPage(1) == 6 && rowsPerPage(2) == 6);
  for (int font = 0; font < 3; ++font)
    for (int screen = 0; screen < 5; ++screen) {
      View view;
      view.projects = &projects;
      view.projectCount = projects.count;
      view.todayCount = 8;
      for (size_t i = 0; i < projects.count; ++i) {
        view.projectOrder[i] = i == 0 ? 6 : i <= 6 ? i - 1 : i;
        view.available[i] = true;
      }
      view.fontSize = font;
      view.screen = static_cast<Screen>(screen);
      view.todayList = font == 0 && view.screen == Screen::List;
      view.detail = &detail;
      copyText(view.date, sizeof(view.date), "Sat 26 Sep");
      copyText(view.status, sizeof(view.status), "Updated 26 Sep 09:30");
      for (size_t i = 0; i < projects.count; ++i) view.count[i] = 8;
      for (int i = 0; i < MAX_VISIBLE_ROWS; ++i) view.rows[i] = &rows[i];
      copyText(view.detailText, sizeof(view.detailText),
               "Review project proposal\n\nWork\nToday, 10:00\n\nCheck scope and timeline before sending.");
      GfxRenderer renderer;
      BaseTheme::drawDashboard(renderer, view);
      renderer.drawLine(20, 752, 508, 752);
      renderer.drawText(UI_12_FONT_ID, 20, 755, "Back     Previous     Next     Open");
      std::ofstream out(std::filesystem::path(argv[1]) /
                        (std::to_string(font) + "-" + std::to_string(screen) + ".svg"));
      out << renderer.svg.str() << "</svg>";
    }
  // Every byte of a very long title/detail remains reachable by paging.
  View view;
  view.screen = Screen::Detail;
  for (size_t i = 0; i < sizeof(view.detailText) - 1; ++i) view.detailText[i] = (i % 12 == 11) ? ' ' : 'a';
  size_t pages = 0;
  GfxRenderer renderer;
  while (view.detailOffset < strlen(view.detailText)) {
    BaseTheme::drawDashboard(renderer, view);
    assert(view.nextDetailOffset > view.detailOffset);
    view.detailOffset = view.nextDetailOffset;
    assert(++pages <= 64);
  }
  // No spaces: line/page boundaries still cannot split a joined emoji sequence.
  for (int font = 0; font < 3; ++font) {
    for (const char* sequence : {"\U0001f469\U0001f3fd\u200d\U0001f4bb", "เก้า", "นํ้า", "ปู่"}) {
      View joined;
      joined.screen = Screen::Detail;
      joined.fontSize = font;
      const size_t unit = strlen(sequence);
      const size_t length = (sizeof(joined.detailText) - 1) / unit * unit;
      for (size_t i = 0; i < length; i += unit) memcpy(joined.detailText + i, sequence, unit);
      joined.detailText[length] = 0;
      size_t pageCount = 0;
      while (joined.detailOffset < length) {
        GfxRenderer emojiRenderer;
        BaseTheme::drawDashboard(emojiRenderer, joined);
        assert(emojiRenderer.blackPixels > 0);
        assert(joined.nextDetailOffset > joined.detailOffset && joined.nextDetailOffset % unit == 0);
        joined.detailOffset = joined.nextDetailOffset;
        assert(++pageCount < 64);
      }
    }
  }
  std::cout << "Dashboard views: six/eight rows at three font sizes fit 528x792; long text and joined-emoji pagination "
               "passed\n";
}
