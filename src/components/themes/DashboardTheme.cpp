#include <GfxRenderer.h>
#include <HalPowerManager.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "BaseTheme.h"
#include "components/UITheme.h"
#include "dashboard/DashboardEmoji.h"
#include "dashboard/DashboardView.h"
#include "fontIds.h"

namespace {
constexpr int BODY_FONTS[] = {NOTOSANS_14_FONT_ID, NOTOSANS_16_FONT_ID, NOTOSANS_18_FONT_ID};
// Bounded, allocation-free word wrapping. Returns the first undisplayed byte,
// allowing detail pages to show the entire title instead of silently truncating.
size_t lines(const GfxRenderer& r, int font, Rect area, const char* text, size_t offset, int maxLines,
             bool black = true, bool bold = false) {
  const size_t length = strlen(text);
  const int height = r.getLineHeight(font);
  const auto style = bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
  for (int row = 0; row < maxLines && offset < length && height <= area.height; ++row) {
    char line[192]{};
    size_t count = 0, lastSpace = 0;
    while (offset + count < length && count < sizeof(line) - 5) {
      const unsigned char c = text[offset + count];
      if (c == '\n') break;
      const size_t bytes = dashboard::emoji::next(text + offset + count, length - offset - count).bytes;
      if (offset + count + bytes > length || count + bytes >= sizeof(line)) break;
      memcpy(line + count, text + offset + count, bytes);
      line[count + bytes] = 0;
      if (dashboard::emoji::width(r, font, line, style) > area.width) {
        line[count] = 0;
        break;
      }
      if (c == ' ') lastSpace = count;
      count += bytes;
    }
    if (!count && text[offset] != '\n') return length;
    if (offset + count < length && text[offset + count] != '\n' && lastSpace) count = lastSpace;
    line[count] = 0;
    dashboard::emoji::draw(r, font, area.x, area.y, line, black, style);
    offset += count;
    while (offset < length && text[offset] == ' ') ++offset;
    if (offset < length && text[offset] == '\n') ++offset;
    area.y += height;
    area.height -= height;
  }
  return offset;
}
}  // namespace

void BaseTheme::drawDashboard(GfxRenderer& r, dashboard::View& view) {
  using namespace dashboard;
  auto safe = UITheme::getInstance().getScreenSafeArea(r, true);
  int topInset, rightInset, bottomInset, leftInset;
  r.getOrientedViewableTRBL(&topInset, &rightInset, &bottomInset, &leftInset);
  const int right = std::min(safe.x + safe.width, r.getScreenWidth() - rightInset);
  const int bottom = std::min(safe.y + safe.height, r.getScreenHeight() - bottomInset);
  safe.x = std::max(safe.x, leftInset);
  safe.y = std::max(safe.y, topInset);
  safe.width = right - safe.x;
  safe.height = bottom - safe.y;
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pad = metrics.contentSidePadding;
  const int x = safe.x + pad, width = safe.width - pad * 2;
  const int bodyFont = BODY_FONTS[std::min<uint8_t>(view.fontSize, 2)];
  const int bodyHeight = r.getLineHeight(bodyFont);
  const int smallHeight = r.getLineHeight(UI_12_FONT_ID);
  const int rowCount = rowsPerPage(view.fontSize);
  const int top = safe.y + metrics.topPadding;
  const int footer = safe.y + safe.height - smallHeight * 2;
  r.clearScreen();
  r.drawText(UI_12_FONT_ID, x, top, view.date);
  char battery[12];
  snprintf(battery, sizeof(battery), "%u%%", powerManager.getBatteryPercentage());
  r.drawText(UI_12_FONT_ID, x + width - r.getTextWidth(UI_12_FONT_ID, battery), top, battery);
  if (view.sleeping) {
    const char* sleeping = tr(STR_SLEEPING);
    r.drawText(UI_12_FONT_ID, x + width - r.getTextWidth(UI_12_FONT_ID, battery) -
                                  r.getTextWidth(UI_12_FONT_ID, sleeping) - pad,
               top, sleeping);
  }
  const int titleY = top + smallHeight + metrics.verticalSpacing;
  const char* title = view.screen == Screen::Overview                     ? tr(STR_DASH_PROJECTS)
                      : view.screen == Screen::Menu                       ? tr(STR_DASH_MENU)
                      : view.todayList                                     ? tr(STR_DASH_TODAY)
                      : view.projects && view.project < view.projectCount ? view.projects->projects[view.project].name
                                                                          : tr(STR_DASH_PROJECTS);
  const int titleFont = view.screen == Screen::Overview || view.screen == Screen::List ? NOTOSANS_16_FONT_ID
                                                                                       : NOTOSANS_18_FONT_ID;
  const int titleHeight = r.getLineHeight(titleFont);
  lines(r, titleFont, Rect{x, titleY, width, titleHeight}, title, 0, 1, true, true);
  const int contentY = titleY + titleHeight + metrics.verticalSpacing * 2;
  const int contentHeight = footer - contentY - metrics.verticalSpacing;
  const size_t project = view.project;
  if (view.screen == Screen::Overview) {
    const int step = contentHeight / rowCount;
    for (int i = 0; i < rowCount && view.pageStart + i <= view.projectCount; ++i) {
      const size_t position = view.pageStart + i;
      const bool today = position == 0;
      const size_t index = today ? 0 : view.projectOrder[position - 1];
      const int y = contentY + i * step;
      r.drawLine(x, y, x + width, y);
      if (view.selection == static_cast<int>(view.pageStart) + i) r.fillRect(x, y + 4, 4, step - 8);
      lines(r, bodyFont, Rect{x + 16, y + 2, width - 70, bodyHeight},
            today ? tr(STR_DASH_TODAY) : view.projects->projects[index].name, 0, 1, true, true);
      char count[12];
      snprintf(count, sizeof(count), "%d", today ? static_cast<int>(view.todayCount) : view.count[index]);
      r.drawText(bodyFont, x + width - r.getTextWidth(bodyFont, count), y + 2, count);
      lines(r, UI_12_FONT_ID, Rect{x + 16, y + step - smallHeight - 2, width - 16, smallHeight},
            !today && !view.available[index] ? tr(STR_DASH_PROJECT_UNAVAILABLE)
            : view.rows[i]         ? view.rows[i]->title
                                   : tr(STR_DASH_NO_TASKS),
            0, 1);
    }
  } else if (view.screen == Screen::List) {
    const int step = contentHeight / rowCount;
    if (!(view.todayList ? view.todayCount : view.count[project]))
      lines(r, bodyFont, Rect{x, contentY, width, contentHeight},
            view.todayList || view.available[project] ? tr(STR_DASH_NO_TASKS) : tr(STR_DASH_PROJECT_UNAVAILABLE), 0, 2);
    for (int i = 0; i < rowCount && view.rows[i]; ++i) {
      const int y = contentY + i * step;
      const bool selected = view.selection == static_cast<int>(view.pageStart) + i;
      if (selected)
        r.fillRect(x, y, width, step);
      else
        r.drawLine(x, y, x + width, y);
      r.drawRect(x + 8, y + 9, 18, 18, !selected);
      lines(r, bodyFont, Rect{x + 38, y + 2, width - 46, bodyHeight}, view.rows[i]->title, 0, 1, !selected, true);
      char metadata[192];
      if (view.todayList)
        snprintf(metadata, sizeof(metadata), "%s - %s", view.rows[i]->project, view.rows[i]->due);
      else
        snprintf(metadata, sizeof(metadata), "%s", view.rows[i]->due[0] ? view.rows[i]->due : tr(STR_DASH_NO_DUE_DATE));
      lines(r, UI_12_FONT_ID, Rect{x + 38, y + step - smallHeight - 2, width - 46, smallHeight}, metadata, 0, 1,
            !selected);
    }
  } else if (view.screen == Screen::Detail) {
    view.nextDetailOffset = lines(r, bodyFont, Rect{x, contentY, width, contentHeight}, view.detailText,
                                  view.detailOffset, contentHeight / bodyHeight);
  } else if (view.screen == Screen::Confirm) {
    const int actionHeight = bodyHeight + metrics.verticalSpacing * 2;
    lines(r, bodyFont, Rect{x, contentY, width, contentHeight / 2}, view.detail ? view.detail->title : "", 0, 4, true,
          true);
    const int warningY = footer - actionHeight * 2 - smallHeight * 3 - 20;
    lines(r, UI_12_FONT_ID, Rect{x, warningY, width, smallHeight * 3},
          view.detail && view.detail->recurring ? tr(STR_DASH_RECUR_WARNING) : tr(STR_DASH_COMPLETE_WARNING), 0, 3);
    for (int i = 0; i < 2; ++i) {
      const int y = footer - actionHeight * (2 - i) - 8;
      const bool selected = view.selection == i;
      if (selected)
        r.fillRect(x, y, width, actionHeight - 4);
      else
        r.drawRect(x, y, width, actionHeight - 4);
      r.drawText(bodyFont, x + 12, y + 8, i == 0 ? tr(STR_CANCEL) : tr(STR_DASH_COMPLETE), !selected);
    }
  } else {
    const StrId options[] = {StrId::STR_DASH_REFRESH, StrId::STR_DASH_WIFI,     StrId::STR_DASH_SETUP,
                             StrId::STR_DASH_FONT,    StrId::STR_DASH_SETTINGS, StrId::STR_DASH_SLEEP};
    const int step = contentHeight / 6;
    for (int i = 0; i < 6; ++i) {
      const int y = contentY + step * i;
      if (view.selection == i) r.fillRect(x, y, width, step);
      r.drawText(bodyFont, x + 12, y + 8, I18N.get(options[i]), view.selection != i);
      if (i == 3) {
        const char* sizes[] = {"28", "32", "36"};
        r.drawText(bodyFont, x + width - 60, y + 8, sizes[view.fontSize], view.selection != i);
      }
    }
  }
  r.drawLine(x, footer, x + width, footer);
  lines(r, UI_12_FONT_ID, Rect{x, footer + 6, width, smallHeight * 2}, view.status, 0, 2);
}
