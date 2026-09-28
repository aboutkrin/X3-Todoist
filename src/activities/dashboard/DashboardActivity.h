#pragma once
#include "activities/Activity.h"
#include "dashboard/DashboardStore.h"
#include "dashboard/DashboardView.h"
#include "dashboard/TodoistClient.h"

class DashboardActivity final : public Activity {
 public:
  DashboardActivity(GfxRenderer& renderer, MappedInputManager& input, bool resumeFromSleep = false,
                    bool cleanInitialRefresh = false)
      : Activity("Dashboard", renderer, input),
        resumeFromSleep(resumeFromSleep),
        cleanInitialRefresh(cleanInitialRefresh) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isHomeActivity() const override { return true; }
  bool preventAutoSleep() override { return true; }
  bool preservesSleepFrame() const override { return true; }
  void prepareForSleep() override;

 private:
  dashboard::Store store;
  dashboard::Config config;
  dashboard::ProjectSelection projects;
  dashboard::ProjectCatalog catalog;
  dashboard::Pending pending;
  dashboard::View view;
  // Row summaries omit descriptions and IDs; the detail record is reused for SD reads.
  dashboard::Row page[dashboard::MAX_VISIBLE_ROWS];
  dashboard::Task detail;
  size_t detailOffsets[64]{};
  int detailPage = 0;
  int overviewSelection = 0;
  int listSelection = 0;
  GfxRenderer::Orientation previousOrientation = GfxRenderer::Portrait;
  unsigned long lastInput = 0;
  bool ready = false;
  bool syncRequested = false;
  bool resumeFromSleep = false;
  bool cleanInitialRefresh = false;
  bool powerReleasedSinceWake = false;
  bool setupAfterWifi = false;
  void restorePosition();
  void sleep(bool fromTimeout);
  void refresh(bool complete = false);
  void updateView();
  void updateProjects();
  void connectWifi(bool setup);
  void status(dashboard::SyncResult result);
  void menuAction();
};
