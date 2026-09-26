#include "DashboardSetupActivity.h"

#include <FontCacheManager.h>
#include <I18n.h>
#include <Memory.h>
#include <WiFi.h>
#include <esp_random.h>

#include "components/UITheme.h"
#include "dashboard/TodoistClient.h"
#include "fontIds.h"

namespace {
constexpr char PAGE_START[] =
    R"HTML(<!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1"><meta charset="utf-8"><style>
*{box-sizing:border-box}body{margin:0;background:#f5f4f1;color:#202020;font:17px system-ui,sans-serif;line-height:1.5}main{max-width:36rem;margin:3rem auto;padding:2rem;background:white;border:1px solid #dededb;border-radius:16px}h1{font-size:1.65rem;line-height:1.2;margin:0 0 1rem}p{color:#555}.badge{font-size:.85rem;color:#245b38;background:#edf6ef;padding:.35rem .7rem;border-radius:20px;display:inline-block}.project{display:flex;align-items:center;gap:1rem;padding:1rem;border:1px solid #ddd;border-radius:8px;margin:.6rem 0;overflow-wrap:anywhere}.project input{width:22px;height:22px;flex-shrink:0;accent-color:#d4483f}label.field{display:block;margin:1rem 0}.field input{display:block;width:100%;padding:.8rem;border:1px solid #bbb;border-radius:6px;font:inherit}button{width:100%;border:0;border-radius:7px;padding:1rem;background:#d4483f;color:white;font:inherit;font-weight:600;cursor:pointer}small{display:block;color:#666}.notice{padding:.75rem;background:#faf3e6;border:1px solid #ded4bf}details{margin-top:2rem;border-top:1px solid #ddd;padding-top:1rem}summary{cursor:pointer}@media(max-width:600px){main{margin:0;border:0;border-radius:0;padding:1.5rem}}fieldset{border:0;padding:0;margin:0}</style><title>)HTML";
}

void DashboardSetupActivity::raw(const char* html) { server->sendContent(html, strlen(html)); }
void DashboardSetupActivity::text(const char* value) {
  char buffer[192];
  size_t used = 0;
  for (const char* p = value; *p; ++p) {
    const char* escaped = nullptr;
    switch (*p) {
      case '&':
        escaped = "&amp;";
        break;
      case '<':
        escaped = "&lt;";
        break;
      case '>':
        escaped = "&gt;";
        break;
      case '"':
        escaped = "&quot;";
        break;
      case '\'':
        escaped = "&#39;";
        break;
    }
    const size_t size = escaped ? strlen(escaped) : 1;
    if (used + size > sizeof(buffer)) {
      server->sendContent(buffer, used);
      used = 0;
    }
    memcpy(buffer + used, escaped ? escaped : p, size);
    used += size;
  }
  if (used) server->sendContent(buffer, used);
}
void DashboardSetupActivity::pageStart() {
  server->sendHeader("Cache-Control", "no-store");
  server->sendHeader("Content-Security-Policy",
                     "default-src 'none'; style-src 'unsafe-inline'; form-action 'self'; frame-ancestors 'none'");
  server->sendHeader("Referrer-Policy", "no-referrer");
  server->setContentLength(CONTENT_LENGTH_UNKNOWN);
  server->send(200, "text/html; charset=utf-8", "");
  raw(PAGE_START);
  text(tr(STR_DASH_WEB_PROJECTS));
  raw("</title></head><body><main>");
}
void DashboardSetupActivity::pageEnd() {
  raw("</main></body></html>");
  server->sendContent("");
}
void DashboardSetupActivity::hiddenCode() {
  raw("<input type=\"hidden\" name=\"code\" value=\"");
  text(code);
  raw("\">");
}
bool DashboardSetupActivity::authorize() {
  if (saved || millis() - started >= 300000) {
    server->send(403);
    return false;
  }
  if (failedAttempts >= 5 || server->arg("code") != code) {
    if (failedAttempts < 5) ++failedAttempts;
    server->send(403, "text/plain; charset=utf-8", tr(STR_DASH_BAD_CODE));
    return false;
  }
  return true;
}
bool DashboardSetupActivity::canSave() {
  if (!authorize()) return false;
  dashboard::Pending pending;
  pending.load();
  if (pending.exists()) {
    server->send(409, "text/plain; charset=utf-8", tr(STR_DASH_PENDING));
    return false;
  }
  return true;
}
void DashboardSetupActivity::projectRow(const dashboard::Project& project, bool unavailable) {
  raw("<label class=\"project\"><input type=\"checkbox\" name=\"project\" value=\"");
  text(project.id);
  raw(selection.find(project.id) >= 0 ? "\" checked><span>" : "\"><span>");
  text(project.name);
  if (unavailable) {
    raw("<small>");
    text(tr(STR_DASH_PROJECT_UNAVAILABLE));
    raw("</small>");
  }
  raw("</span></label>");
}
void DashboardSetupActivity::showProjects() {
  pageStart();
  raw("<h1>");
  text(tr(STR_DASH_WEB_PROJECTS));
  raw("</h1><span class=\"badge\">");
  text(tr(STR_DASH_CONNECTED));
  raw("</span><p>");
  text(tr(STR_DASH_PROJECT_HELP));
  raw("</p>");
  if (stale) {
    raw("<p class=\"notice\">");
    text(tr(STR_DASH_CATALOG_STALE));
    raw("</p>");
  }
  if (!catalog.count() && !selection.count) {
    raw("<p>");
    text(tr(STR_DASH_CATALOG_EMPTY));
    raw("</p>");
  } else {
    raw("<form method=\"post\" action=\"/save-projects\">");
    hiddenCode();
    raw("<fieldset><legend>");
    text(tr(STR_DASH_PROJECTS));
    raw("</legend>");
    dashboard::Project project;
    for (size_t i = 0; i < catalog.count(); ++i)
      if (catalog.read(i, project)) projectRow(project);
    for (size_t i = 0; i < selection.count; ++i)
      if (!catalog.find(selection.projects[i].id, project)) projectRow(selection.projects[i], true);
    raw("</fieldset><p><small>");
    text(tr(STR_DASH_PROJECT_RULES));
    raw("</small></p><button>");
    text(tr(STR_DASH_SAVE_REFRESH));
    raw("</button></form>");
  }
  raw("<p><small>");
  text(tr(STR_DASH_KEEP_TOKEN));
  raw("</small></p><details><summary>");
  text(tr(STR_DASH_CHANGE_TOKEN));
  raw("</summary><form method=\"post\" action=\"/save\">");
  hiddenCode();
  raw("<label class=\"field\">");
  text(tr(STR_DASH_API_TOKEN));
  raw("<input name=\"token\" type=\"password\" required maxlength=\"128\" autocomplete=\"off\"></label><button>");
  text(tr(STR_DASH_SAVE_DEVICE));
  raw("</button></form></details>");
  pageEnd();
}
void DashboardSetupActivity::onEnter() {
  Activity::onEnter();
  config.load();
  if (WiFi.status() != WL_CONNECTED) {
    finish();
    return;
  }
  if (config.configured()) {
    // TLS runs before allocating the server and its request buffers.
    RenderLock lock;
    if (auto* fonts = renderer.getFontCacheManager()) fonts->releaseSdFontCaches();
    dashboard::TodoistClient client(config, selection);
    stale = client.refreshProjects() != dashboard::SyncResult::Ok;
    catalog.load();
  }
  server = makeUniqueNoThrow<WebServer>(80);
  if (!server) {
    LOG_ERR("DASH", "Setup server allocation failed");
    finish();
    return;
  }
  snprintf(code, sizeof(code), "%06lu", static_cast<unsigned long>(esp_random() % 1000000));
  snprintf(address, sizeof(address), "http://%s", WiFi.localIP().toString().c_str());
  server->on("/", HTTP_GET, [this] {
    pageStart();
    raw("<h1>");
    text(tr(STR_DASH_SETUP));
    raw("</h1><p>");
    text(tr(STR_DASH_SETUP_HINT));
    raw("</p><form method=\"post\" action=\"");
    raw(config.configured() ? "/projects" : "/save");
    raw("\"><label class=\"field\">");
    text(tr(STR_DASH_PAIR_CODE));
    raw("<input name=\"code\" inputmode=\"numeric\" required minlength=\"6\" maxlength=\"6\" "
        "autocomplete=\"off\"></label>");
    if (!config.configured()) {
      raw("<label class=\"field\">");
      text(tr(STR_DASH_API_TOKEN));
      raw("<input name=\"token\" type=\"password\" required maxlength=\"128\" autocomplete=\"off\"></label>");
    }
    raw("<button>");
    text(config.configured() ? tr(STR_DASH_UNLOCK) : tr(STR_DASH_SAVE_DEVICE));
    raw("</button></form>");
    pageEnd();
  });
  server->on("/projects", HTTP_POST, [this] {
    if (!authorize()) return;
    if (!config.configured()) {
      server->send(409, "text/plain", tr(STR_DASH_NEEDS_SETUP));
      return;
    }
    showProjects();
  });
  server->on("/save-projects", HTTP_POST, [this] {
    if (!canSave()) return;
    // One bounded draft lets validation fail without altering the active selection.
    auto draft = makeUniqueNoThrow<dashboard::ProjectSelection>();
    if (!draft) {
      LOG_ERR("DASH", "Project selection allocation failed");
      server->send(503);
      return;
    }
    dashboard::Project project;
    for (int i = 0; i < server->args(); ++i) {
      if (server->argName(i) != "project") continue;
      const auto id = server->arg(i);
      if (!catalog.find(id.c_str(), project)) {
        const int previous = selection.find(id.c_str());
        if (previous < 0) {
          server->send(400, "text/plain", tr(STR_DASH_BAD_PROJECTS));
          return;
        }
        project = selection.projects[previous];
      }
      if (!draft->add(project)) {
        server->send(400, "text/plain", tr(STR_DASH_BAD_PROJECTS));
        return;
      }
    }
    if (!draft->count) {
      server->send(400, "text/plain", tr(STR_DASH_BAD_PROJECTS));
      return;
    }
    if (!draft->save()) {
      server->send(503, "text/plain", tr(STR_DASH_STORAGE_ERROR));
      return;
    }
    selection = *draft;
    server->sendHeader("Cache-Control", "no-store");
    server->send(200, "text/plain; charset=utf-8", tr(STR_DASH_SAVED));
    saved = true;
  });
  server->on("/save", HTTP_POST, [this] {
    if (!canSave()) return;
    if (!config.save(server->arg("token").c_str(), config.fontSize)) {
      server->send(400, "text/plain", tr(STR_DASH_BAD_TOKEN));
      return;
    }
    server->sendHeader("Cache-Control", "no-store");
    server->send(200, "text/plain; charset=utf-8", tr(STR_DASH_TOKEN_SAVED));
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
  if (saved || millis() - started >= 300000 || mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (server) server->handleClient();
  if (saved) finish();
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
