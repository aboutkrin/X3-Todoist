#pragma once
#include <WebServer.h>

#include "activities/Activity.h"
#include "dashboard/DashboardStore.h"

class DashboardSetupActivity final : public Activity {
 public:
  DashboardSetupActivity(GfxRenderer& renderer, MappedInputManager& input)
      : Activity("DashboardSetup", renderer, input) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

 private:
  std::unique_ptr<WebServer> server;
  dashboard::Config config;
  char code[12]{};
  char address[48]{};
  unsigned long started = 0;
  bool saved = false;
  int failedAttempts = 0;
};
