#pragma once
#include <WebServer.h>

#include "activities/Activity.h"
#include "dashboard/DashboardStore.h"

class DashboardSetupActivity final : public Activity {
 public:
  DashboardSetupActivity(GfxRenderer& renderer, MappedInputManager& input, dashboard::ProjectSelection& selection)
      : Activity("DashboardSetup", renderer, input), selection(selection) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

 private:
  std::unique_ptr<WebServer> server;
  dashboard::Config config;
  dashboard::ProjectSelection& selection;
  dashboard::ProjectCatalog catalog;
  bool stale = false;
  char code[12]{};
  char address[48]{};
  unsigned long started = 0;
  bool saved = false;
  int failedAttempts = 0;
  bool authorize();
  bool canSave();
  void pageStart();
  void pageEnd();
  void raw(const char* html);
  void text(const char* value);
  void hiddenCode();
  void projectRow(const dashboard::Project& project, bool unavailable = false);
  void showProjects();
};
