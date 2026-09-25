#include "DashboardSetupActivity.h"

#include <I18n.h>
#include <Memory.h>
#include <WiFi.h>
#include <esp_random.h>

#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr char SETUP_PAGE[] =
    R"HTML(<!doctype html><html lang="en"><meta name="viewport" content="width=device-width,initial-scale=1"><title>%s</title>
<style>body{font:18px system-ui;max-width:32rem;margin:3rem auto;padding:1rem;color:#111;background:white}label{display:block;margin:1.5rem 0}input,button{font:inherit;padding:.7rem;box-sizing:border-box;width:100%%}button{background:#111;color:white;border:0}p{line-height:1.5}</style>
<h1>%s</h1><p>%s</p>
<form method="post" action="/save"><label>%s<input name="code" inputmode="numeric" required maxlength="6" autocomplete="off"></label><label>%s<input name="token" type="password" required maxlength="128" autocomplete="off"></label><button>%s</button></form></html>)HTML";
}
void DashboardSetupActivity::onEnter() {
  Activity::onEnter();
  config.load();
  if (WiFi.status() != WL_CONNECTED) {
    finish();
    return;
  }
  // WebServer owns network buffers and must not be on the small loop stack.
  server = makeUniqueNoThrow<WebServer>(80);
  if (!server) {
    LOG_ERR("DASH", "Setup server allocation failed");
    finish();
    return;
  }
  snprintf(code, sizeof(code), "%06lu", static_cast<unsigned long>(esp_random() % 1000000));
  snprintf(address, sizeof(address), "http://%s", WiFi.localIP().toString().c_str());
  server->on("/", HTTP_GET, [this] {
    server->sendHeader("Cache-Control", "no-store");
    server->sendHeader("Content-Security-Policy",
                       "default-src 'none'; style-src 'unsafe-inline'; form-action 'self'; frame-ancestors 'none'");
    const char* labels[] = {tr(STR_DASH_SETUP),     tr(STR_DASH_CONNECT),   tr(STR_DASH_PAIR_HELP),
                            tr(STR_DASH_PAIR_CODE), tr(STR_DASH_API_TOKEN), tr(STR_DASH_SAVE_DEVICE)};
    size_t size = sizeof(SETUP_PAGE);
    for (const char* label : labels) size += strlen(label);
    // The translated HTML is too large for the loop stack; release after send.
    auto page = makeUniqueNoThrow<char[]>(size);
    if (!page) {
      LOG_ERR("DASH", "Setup page allocation failed");
      server->send(503);
      return;
    }
    snprintf(page.get(), size, SETUP_PAGE, labels[0], labels[1], labels[2], labels[3], labels[4], labels[5]);
    server->send(200, "text/html", page.get());
  });
  server->on("/save", HTTP_POST, [this] {
    if (failedAttempts >= 5 || server->arg("code") != code) {
      ++failedAttempts;
      server->send(403, "text/plain", tr(STR_DASH_BAD_CODE));
      return;
    }
    dashboard::Pending pending;
    pending.load();
    if (pending.exists()) {
      server->send(409, "text/plain", tr(STR_DASH_PENDING));
      return;
    }
    if (!config.save(server->arg("token").c_str(), config.fontSize)) {
      server->send(400, "text/plain", tr(STR_DASH_BAD_TOKEN));
      return;
    }
    server->sendHeader("Cache-Control", "no-store");
    server->send(200, "text/plain", tr(STR_DASH_SAVED));
    saved = true;
  });
  server->begin();
  started = millis();
  requestUpdate();
}
void DashboardSetupActivity::onExit() {
  if (server) server->stop();
  server.reset();
  memset(config.token, 0, sizeof(config.token));
  Activity::onExit();
}
void DashboardSetupActivity::loop() {
  if (server) server->handleClient();
  if (saved || millis() - started >= 300000 || mappedInput.wasPressed(MappedInputManager::Button::Back)) finish();
}
void DashboardSetupActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto area = UITheme::getInstance().getScreenSafeArea(renderer, true);
  GUI.drawHeader(renderer, Rect{area.x, area.y, area.width, 90}, tr(STR_DASH_SETUP));
  UITheme::drawCenteredWrappedText(renderer, Rect{area.x + 20, area.y + 110, area.width - 40, 120}, NOTOSANS_16_FONT_ID,
                                   tr(STR_DASH_SETUP_HINT), 3);
  UITheme::drawCenteredText(renderer, area, NOTOSANS_16_FONT_ID, area.y + 260, address);
  UITheme::drawCenteredText(renderer, area, NOTOSANS_18_FONT_ID, area.y + 340, code);
  const auto hints = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
  renderer.displayBuffer();
}
