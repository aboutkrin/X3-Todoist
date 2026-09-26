#pragma once
#include "activities/Activity.h"
#include "dashboard/DashboardStore.h"
#include "dashboard/DashboardView.h"
#include "dashboard/TodoistClient.h"

class DashboardActivity final : public Activity {
 public:
  DashboardActivity(GfxRenderer& renderer, MappedInputManager& input) : Activity("Dashboard", renderer, input) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isHomeActivity() const override { return true; }
  bool preventAutoSleep() override { return true; }
  uint32_t scheduledWakeSeconds() const override;

 private:
  dashboard::Store store;
  dashboard::Config config;
  dashboard::ProjectSelection projects;
  dashboard::ProjectCatalog catalog;
  dashboard::Pending pending;
  dashboard::View view;
  // Allocate once with the activity: four rendered records + one detail record.
  // Never put these ~12KB of text on a FreeRTOS stack or retain the full feed.
  dashboard::Task page[4];
  dashboard::Task detail;
  size_t detailOffsets[64]{};
  int detailPage = 0;
  int overviewSelection = 0;
  int listSelection = 0;
  GfxRenderer::Orientation previousOrientation = GfxRenderer::Portrait;
  unsigned long lastInput = 0;
  unsigned long lastAttempt = 0;
  bool ready = false;
  bool initialSync = true;
  bool setupAfterWifi = false;
  void refresh(bool complete = false);
  void updateView();
  void updateProjects();
  void connectWifi(bool setup);
  void status(dashboard::SyncResult result);
  void menuAction();
};
