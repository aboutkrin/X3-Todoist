#include "DashboardActivity.h"

#include <HalClock.h>
#include <I18n.h>
#include <Memory.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <sys/time.h>

#include <algorithm>

#include "CrossPointSettings.h"
#include "DashboardSetupActivity.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "util/Timezones.h"

extern void enterDeepSleep(bool fromTimeout);
namespace {
constexpr unsigned long IDLE_MS = 60000;
}
void DashboardActivity::onEnter() {
  Activity::onEnter();
  previousOrientation = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Portrait);
  const bool configured = config.load();
  pending.load();
  if (!configured && SETTINGS.clockTimezone == 255) {
    for (size_t i = 0; i < timezones::count(); ++i)
      if (strcmp(timezones::table()[i].posixTz, "ICT-7") == 0) {
        SETTINGS.clockTimezone = i;
        SETTINGS.saveToFile();
        timezones::applyToClock();
        break;
      }
  }
  tm local{};
  if (halClock.localTime(local) && local.tm_year >= 124) {
    local.tm_isdst = -1;
    timeval value{mktime(&local), 0};
    settimeofday(&value, nullptr);
  }
  ready = store.begin();
  if (ready) store.load(time(nullptr));
  view.fontSize = config.fontSize;
  lastInput = millis();
  status(ready ? configured ? dashboard::SyncResult::Ok : dashboard::SyncResult::NotConfigured
               : dashboard::SyncResult::StorageError);
  updateView();
  requestUpdate();
}
void DashboardActivity::onExit() {
  memset(config.token, 0, sizeof(config.token));
  renderer.setOrientation(previousOrientation);
  Activity::onExit();
}
uint32_t DashboardActivity::scheduledWakeSeconds() const {
  if (!config.configured() || !ready) return 0;
  const unsigned long elapsed = millis() - lastAttempt;
  return std::max<uint32_t>(
      1, dashboard::REFRESH_SECONDS - std::min<unsigned long>(dashboard::REFRESH_SECONDS - 1, elapsed / 1000));
}
void DashboardActivity::status(dashboard::SyncResult result) {
  using dashboard::SyncResult;
  const char* message = "";
  switch (result) {
    case SyncResult::Ok: {
      if (!store.hasCache()) {
        message = tr(STR_DASH_NOT_SYNCED);
        break;
      }
      tm local{};
      const time_t sync = store.syncedAt();
      localtime_r(&sync, &local);
      char date[24];
      strftime(date, sizeof(date), "%d %b %H:%M", &local);
      snprintf(view.status, sizeof(view.status), "%s %s", tr(STR_DASH_UPDATED), date);
      return;
    }
    case SyncResult::NotConfigured:
      message = tr(STR_DASH_NEEDS_SETUP);
      break;
    case SyncResult::Offline:
      message = tr(STR_DASH_OFFLINE);
      break;
    case SyncResult::Clock:
      message = tr(STR_DASH_CLOCK_ERROR);
      break;
    case SyncResult::Auth:
      message = tr(STR_DASH_AUTH_ERROR);
      break;
    case SyncResult::RateLimited:
      message = tr(STR_DASH_RATE_LIMIT);
      break;
    case SyncResult::StorageError:
      message = tr(STR_DASH_STORAGE_ERROR);
      break;
    case SyncResult::Limit:
      message = tr(STR_DASH_LIMIT);
      break;
    case SyncResult::Pending:
      message = tr(STR_DASH_PENDING);
      break;
    default:
      message = tr(STR_DASH_SYNC_ERROR);
      break;
  }
  snprintf(view.status, sizeof(view.status), "%s", message);
}
void DashboardActivity::updateView() {
  tm local{};
  const time_t now = time(nullptr);
  localtime_r(&now, &local);
  if (now >= 1704067200)
    strftime(view.date, sizeof(view.date), "%a %d %b", &local);
  else
    snprintf(view.date, sizeof(view.date), "%s", tr(STR_DASH_CLOCK_UNSET));
  for (int i = 0; i < 3; ++i) view.count[i] = store.count(static_cast<dashboard::Section>(i));
  for (auto& row : view.rows) row = nullptr;
  if (view.screen == dashboard::Screen::Overview) {
    for (int i = 0; i < 3; ++i) {
      const auto item = store.entry(static_cast<dashboard::Section>(i), 0);
      if (item && store.read(item->record, page[i])) view.rows[i] = &page[i];
    }
  } else if (view.screen == dashboard::Screen::List) {
    view.selection = std::clamp(view.selection, 0, std::max(0, view.count[static_cast<int>(view.section)] - 1));
    view.pageStart = view.selection / 4 * 4;
    for (int i = 0; i < 4; ++i) {
      const auto item = store.entry(view.section, view.pageStart + i);
      if (item && store.read(item->record, page[i])) view.rows[i] = &page[i];
    }
  }
  view.detail = &detail;
}
void DashboardActivity::refresh(bool complete) {
  if (!ready) return;
  {
    RenderLock lock;
    snprintf(view.status, sizeof(view.status), "%s", tr(STR_DASH_SYNCING));
  }
  requestUpdateAndWait();
  {
    RenderLock lock;
    dashboard::TodoistClient client(config);
    LOG_INF("DASH", "Sync start heap: %u", ESP.getFreeHeap());
    const auto result = complete ? client.complete(detail, pending, store) : client.reconcile(pending, store);
    store.finishWrite();
    LOG_INF("DASH", "Sync result %u, heap: %u, minimum since boot: %u", static_cast<unsigned>(result),
            ESP.getFreeHeap(), ESP.getMinFreeHeap());
    lastAttempt = millis();
    status(result);
    if (pending.exists()) status(dashboard::SyncResult::Pending);
    if (complete && result == dashboard::SyncResult::Ok) {
      view.screen = dashboard::Screen::List;
      view.selection = 0;
    }
    updateView();
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  requestUpdate();
}
void DashboardActivity::connectWifi(bool setup) {
  setupAfterWifi = setup;
  auto wifi = makeUniqueNoThrow<WifiSelectionActivity>(renderer, mappedInput);
  if (!wifi) {
    LOG_ERR("DASH", "WiFi activity allocation failed");
    return;
  }
  startActivityForResult(std::move(wifi), [this](const ActivityResult& result) {
    lastInput = millis();
    if (result.isCancelled) return;
    if (setupAfterWifi) {
      auto setup = makeUniqueNoThrow<DashboardSetupActivity>(renderer, mappedInput);
      if (!setup) {
        LOG_ERR("DASH", "Setup activity allocation failed");
        return;
      }
      startActivityForResult(std::move(setup), [this](const ActivityResult&) {
        RenderLock lock;
        config.load();
        store.load(time(nullptr));
        view.screen = dashboard::Screen::Overview;
        view.selection = 0;
        updateView();
        initialSync = true;
        lastInput = millis();
      });
    } else
      initialSync = true;
  });
}
void DashboardActivity::menuAction() {
  switch (view.selection) {
    case 0: {
      RenderLock lock;
      initialSync = true;
      view.screen = dashboard::Screen::Overview;
      view.selection = 0;
      updateView();
      break;
    }
    case 1:
      connectWifi(false);
      break;
    case 2:
      connectWifi(true);
      break;
    case 3: {
      RenderLock lock;
      config.fontSize = (config.fontSize + 1) % 3;
      view.fontSize = config.fontSize;
      if (config.configured()) config.save(config.token, config.fontSize);
      break;
    }
    case 4:
      activityManager.goToSettings();
      break;
    case 5:
      enterDeepSleep(false);
      break;
  }
}
void DashboardActivity::loop() {
  using namespace dashboard;
  if (initialSync && ready && config.configured()) {
    initialSync = false;
    refresh();
    // A scheduled wake only fetches and repaints; no minute of awake idle time.
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER && millis() - lastInput > 1000) {
      requestUpdateAndWait();
      enterDeepSleep(true);
    }
    return;
  }
  if (ready && config.configured() && millis() - lastAttempt >= REFRESH_SECONDS * 1000UL) {
    refresh();
    return;
  }
  if (mappedInput.wasAnyPressed()) lastInput = millis();
  if (millis() - lastInput >= IDLE_MS && config.configured()) {
    {
      RenderLock lock;
      view.screen = Screen::Overview;
      view.selection = 0;
      updateView();
    }
    requestUpdateAndWait();
    enterDeepSleep(true);
    return;
  }
  bool changed = false;
  bool doComplete = false;
  bool doMenu = false;
  {
    RenderLock lock;
    const bool back = mappedInput.wasPressed(MappedInputManager::Button::Back);
    const bool confirm = mappedInput.wasPressed(MappedInputManager::Button::Confirm);
    const bool prev = mappedInput.wasPressed(MappedInputManager::Button::Left) ||
                      mappedInput.wasPressed(MappedInputManager::Button::Up);
    const bool next = mappedInput.wasPressed(MappedInputManager::Button::Right) ||
                      mappedInput.wasPressed(MappedInputManager::Button::Down);
    if (back) {
      if (view.screen == Screen::Overview) {
        view.screen = Screen::Menu;
        view.selection = 0;
      } else if (view.screen == Screen::Confirm) {
        view.screen = Screen::Detail;
        view.selection = 0;
      } else if (view.screen == Screen::Detail) {
        view.screen = Screen::List;
        view.selection = 0;
      } else {
        view.screen = Screen::Overview;
        view.selection = 0;
      }
      changed = true;
    } else if (prev || next) {
      if (view.screen == Screen::Detail) {
        if (next && view.nextDetailOffset < strlen(view.detailText) && detailPage < 63) {
          detailOffsets[++detailPage] = view.nextDetailOffset;
        } else if (prev && detailPage > 0)
          --detailPage;
        view.detailOffset = detailOffsets[detailPage];
      } else {
        const int count = view.screen == Screen::Overview  ? 3
                          : view.screen == Screen::Menu    ? 6
                          : view.screen == Screen::Confirm ? 2
                                                           : view.count[static_cast<int>(view.section)];
        if (count) view.selection = (view.selection + (next ? 1 : count - 1)) % count;
      }
      changed = true;
    } else if (confirm) {
      if (view.screen == Screen::Overview) {
        view.section = static_cast<Section>(view.selection);
        view.screen = Screen::List;
        view.selection = 0;
      } else if (view.screen == Screen::List) {
        const auto entry = store.entry(view.section, view.selection);
        if (entry && store.read(entry->record, detail)) {
          snprintf(view.detailText, sizeof(view.detailText), "%s\n\n%s\n%s\n\n%s", detail.title, detail.project,
                   detail.due, detail.description);
          detailPage = 0;
          view.detailOffset = detailOffsets[0] = 0;
          view.screen = Screen::Detail;
        }
      } else if (view.screen == Screen::Detail) {
        if (pending.exists())
          status(SyncResult::Pending);
        else {
          view.screen = Screen::Confirm;
          view.selection = 0;
        }
      } else if (view.screen == Screen::Confirm) {
        if (view.selection == 1)
          doComplete = true;
        else
          view.screen = Screen::Detail;
      } else
        doMenu = true;
      changed = true;
    }
    if (changed) updateView();
  }
  if (doComplete) refresh(true);
  if (doMenu) menuAction();
  if (changed) requestUpdate();
}
void DashboardActivity::render(RenderLock&&) {
  GUI.drawDashboard(renderer, view);
  const auto hints =
      mappedInput.mapLabels(view.screen == dashboard::Screen::Overview ? tr(STR_DASH_MENU) : tr(STR_BACK),
                            view.screen == dashboard::Screen::Detail ? tr(STR_DASH_COMPLETE) : tr(STR_SELECT),
                            tr(STR_DASH_PREVIOUS), tr(STR_DASH_NEXT));
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
  renderer.displayBuffer(HalDisplay::RefreshMode::FAST_REFRESH);
}
