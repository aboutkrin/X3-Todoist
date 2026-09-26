#include <I18n.h>
#include <WiFi.h>
#include <esp_http_client.h>

#include <cassert>
#include <iostream>

#include "activities/dashboard/DashboardSetupActivity.h"
using namespace dashboard;
const char* stubText(StrId) { return "Translated label"; }
static void projectsResponse(bool failure = false) {
  responses.push_back({"projects?limit=20", failure ? 503 : 200,
                       "{\"results\":[{\"id\":\"p\",\"name\":\"<Personal & "
                       "Family>\"},{\"id\":\"w\",\"name\":\"Work\"}],\"next_cursor\":null}"});
}
int main(int argc, char** argv) {
  assert(argc == 2);
  Storage.root = argv[1];
  Config config;
  assert(config.save("test-private-token", 1));
  ProjectSelection selection;
  GfxRenderer renderer;
  MappedInputManager input;
  {
    projectsResponse();
    DashboardSetupActivity setup(renderer, input, selection);
    setup.onEnter();
    auto& web = *WebServer::active;
    web.request("/", HTTP_GET);
    assert(web.status == 200 && web.body.find("test-private-token") == std::string::npos);
    assert(web.body.find("name=\"token\"") == std::string::npos);
    assert(web.headers.at("Cache-Control") == "no-store");
    web.request("/projects", HTTP_POST, {{"code", "bad"}});
    assert(web.status == 403 && web.body.find("Personal") == std::string::npos);
    web.request("/projects", HTTP_POST, {{"code", "123456"}});
    assert(web.status == 200 && web.body.find("&lt;Personal &amp; Family&gt;") != std::string::npos);
    assert(web.body.find("<Personal") == std::string::npos);
    web.request("/save-projects", HTTP_POST, {{"code", "123456"}});
    assert(web.status == 400 && selection.count == 0);
    web.request("/save-projects", HTTP_POST, {{"code", "123456"}, {"project", "p"}, {"project", "p"}});
    assert(web.status == 400 && selection.count == 0);
    web.request("/save-projects", HTTP_POST, {{"code", "123456"}, {"project", "unknown"}});
    assert(web.status == 400 && selection.count == 0);
    web.request("/save-projects", HTTP_POST, {{"code", "123456"}, {"project", "p"}, {"project", "w"}});
    assert(web.status == 200 && selection.count == 2);
    setup.loop();
    assert(Activity::finished);
    setup.onExit();
  }
  {
    projectsResponse(true);
    DashboardSetupActivity setup(renderer, input, selection);
    setup.onEnter();
    auto& web = *WebServer::active;
    web.request("/projects", HTTP_POST, {{"code", "123456"}});
    assert(web.status == 200 && web.body.find("checked") != std::string::npos);
    assert(web.body.find("class=\"notice\"") != std::string::npos);
    Pending pending;
    copyText(pending.id, sizeof(pending.id), "task");
    copyText(pending.uuid, sizeof(pending.uuid), "pending-command");
    assert(pending.save());
    web.request("/save-projects", HTTP_POST, {{"code", "123456"}, {"project", "p"}});
    assert(web.status == 409 && selection.count == 2);
    assert(pending.clear());
    for (int i = 0; i < 5; ++i) web.request("/projects", HTTP_POST, {{"code", "wrong"}});
    web.request("/projects", HTTP_POST, {{"code", "123456"}});
    assert(web.status == 403);
    fakeMillis += 300001;
    setup.loop();
    assert(Activity::finished);
    setup.onExit();
  }
  assert(responses.empty());
  {
    projectsResponse();
    DashboardSetupActivity setup(renderer, input, selection);
    setup.onEnter();
    auto& web = *WebServer::active;
    fakeMillis += 300001;
    web.request("/save-projects", HTTP_POST, {{"code", "123456"}, {"project", "p"}});
    assert(web.status == 403 && selection.count == 2);
    setup.loop();
    assert(Activity::finished);
    setup.onExit();
  }
  std::cout
      << "Web setup: pairing, escaped names, token privacy, selection validation, stale catalogue and timeout passed\n";
}
