#include <Preferences.h>
#include <WiFi.h>
#include <esp_http_client.h>

#include <cassert>
#include <iostream>

#include "TodoistClient.h"
using namespace dashboard;
static std::string today() {
  time_t now = time(nullptr);
  tm local{};
  localtime_r(&now, &local);
  char out[24];
  strftime(out, sizeof(out), "%Y-%m-%d", &local);
  return out;
}
static std::string task(const char* id, const char* assignee = "", bool recurring = false, const char* project = "p") {
  return std::string("{\"id\":\"") + id +
         "\",\"project_id\":\"" + project + "\",\"content\":\"Example "
         "task\",\"description\":\"Details\",\"priority\":4,\"responsible_uid\":\"" +
         assignee + "\",\"due\":{\"date\":\"" + today() + "\",\"is_recurring\":" + (recurring ? "true" : "false") +
         "}}";
}
static void syncResponses(bool empty = false) {
  responses.push_back({"sync", 200, "{\"user\":{\"id\":\"me\",\"inbox_project_id\":\"p\"}}"});
  responses.push_back(
      {"projects?limit=20", 200, "{\"results\":[{\"id\":\"p\",\"name\":\"Work\"},{\"id\":\"q\",\"name\":\"Outside\"}],\"next_cursor\":\"next\"}"});
  responses.push_back({"projects?limit=20&cursor=next", 200, "{\"results\":[],\"next_cursor\":null}"});
  responses.push_back({"tasks?limit=20&project_id=p", 200,
                       "{\"results\":[" + (empty ? std::string() : task("one") + "," + task("other", "someone-else")) +
                           "],\"next_cursor\":\"next\"}"});
  responses.push_back({"tasks?limit=20&project_id=p&cursor=next", 200,
                       "{\"results\":[" + (empty ? std::string() : task("two", "me")) + "],\"next_cursor\":null}"});
  responses.push_back({"tasks/filter?limit=20&query=today%20%7C%20overdue", 200,
                       "{\"results\":[" +
                           (empty ? std::string() : task("outside", "", false, "q") + "," + task("one") + "," +
                                                        task("assigned-away", "someone-else", false, "q")) +
                           "],\"next_cursor\":null}"});
}
int main(int argc, char** argv) {
  assert(argc == 2);
  Storage.root = argv[1];
  setenv("TZ", "ICT-7", 1);
  tzset();
  Config config;
  assert(config.save("test-token", 1));
  ProjectSelection selection;
  assert(selection.add(Project{"p", "Work"}));
  assert(selection.save());
  Store store;
  assert(store.begin(selection));
  TodoistClient client(config, selection);
  syncResponses();
  assert(client.sync(store) == SyncResult::Ok);
  assert(responses.empty());
  assert(store.count(0) == 2);
  assert(store.todayCount() == 3);
  assert(config.inboxId[0] == 'p');
  bool foundOutside = false;
  for (size_t i = 0; i < store.todayCount(); ++i)
    foundOutside |= !strcmp(store.todayEntry(i)->id, "outside");
  assert(foundOutside);
  // A completed HTTP response is not usable when persisting it fails.
  responses.push_back({"sync", 200, "{\"user\":{\"id\":\"me\"}}"});
  HalFile::failSync = true;
  assert(client.sync(store) == SyncResult::StorageError);
  HalFile::failSync = false;
  assert(responses.empty());
  assert(store.count(0) == 2);
  Task selected;
  assert(store.read(store.entry(0, 0)->record, selected));
  assert(!strcmp(selected.project, "Work"));
  responses.push_back({"sync", 401, "{}"});
  assert(client.sync(store) == SyncResult::Auth);
  assert(store.count(0) == 2);
  responses.push_back({"sync", 429, "{}"});
  assert(client.sync(store) == SyncResult::RateLimited);
  assert(client.sync(store) == SyncResult::RateLimited);
  assert(responses.empty());
  {
    Preferences p;
    p.begin("dash-http", false);
    p.putLong64("retry-at", 0);
  }
  // Server may apply the command while the acknowledgment is lost.
  Pending pending;
  responses.push_back({"sync", 200, "", ESP_FAIL, false});
  assert(client.complete(selected, pending, store) == SyncResult::Transport);
  assert(pending.exists());
  const std::string firstCommand = posted.back();
  Pending rebooted;
  assert(rebooted.load());
  responses.push_back({"sync", 200, "ACK"});
  syncResponses(true);
  assert(client.reconcile(rebooted, store) == SyncResult::Ok);
  assert(!rebooted.exists());
  assert(store.count(0) == 0);
  assert(posted[posted.size() - 2] == firstCommand);
  assert(responses.empty());
  // Offline completion must not create a deferred user action.
  WiFi.connection = 0;
  assert(client.complete(selected, pending, store) == SyncResult::Pending);  // old in-memory pending remains blocked
  Pending fresh;
  assert(!fresh.load());
  assert(client.complete(selected, fresh, store) == SyncResult::Offline);
  assert(!fresh.exists());
  WiFi.connection = WL_CONNECTED;
  // A phone-completed recurring task must not advance a second occurrence.
  selected.recurring = 1;
  responses.push_back({std::string("tasks/") + selected.id, 200, "{\"due\":{\"date\":\"2099-01-01\"}}"});
  assert(client.complete(selected, fresh, store) == SyncResult::InvalidData);
  assert(!fresh.exists());
  // Once acknowledged, a failed refresh must never send the close again.
  selected.recurring = 0;
  responses.push_back({"sync", 200, "ACK"});
  responses.push_back({"sync", 503, "{}"});
  assert(client.complete(selected, fresh, store) == SyncResult::Transport);
  assert(fresh.exists() && fresh.acknowledged);
  Pending ackReboot;
  assert(ackReboot.load() && ackReboot.acknowledged);
  const size_t postCount = posted.size();
  syncResponses(true);
  assert(client.reconcile(ackReboot, store) == SyncResult::Ok);
  assert(posted.size() == postCount + 1);  // Only the user resource query is posted.
  assert(responses.empty());
  assert(selection.add(Project{"w", "Work two"}));
  assert(selection.save());
  assert(!store.load(time(nullptr)));
  responses.push_back({"sync", 200, "{\"user\":{\"id\":\"me\"}}"});
  responses.push_back(
      {"projects?limit=20", 200, R"({"results":[{"id":"p","name":"Work renamed"},{"id":"w","name":"Work two"}]})"});
  responses.push_back(
      {"tasks?limit=20&project_id=p", 200,
       R"({"results":[{"id":"undated","project_id":"p","content":"No date","priority":4},{"id":"future","project_id":"p","content":"Future","due":{"date":"2099-01-01"}}]})"});
  responses.push_back({"tasks?limit=20&project_id=w", 200,
                       R"({"results":[{"id":"second","project_id":"w","content":"Second project"}]})"});
  responses.push_back({"tasks/filter?limit=20&query=today%20%7C%20overdue", 200, R"({"results":[]})"});
  assert(client.sync(store) == SyncResult::Ok);
  assert(store.count(0) == 2 && store.count(1) == 1);
  assert(!strcmp(store.entry(0, 0)->id, "future"));
  assert(!strcmp(store.entry(0, 1)->id, "undated"));
  assert(store.read(store.entry(0, 0)->record, selected));
  assert(!strcmp(selected.project, "Work renamed"));
  responses.push_back({"sync", 200, "{\"user\":{\"id\":\"me\"}}"});
  responses.push_back({"projects?limit=20", 200, R"({"results":[{"id":"p","name":"Work"}]})"});
  responses.push_back({"tasks?limit=20&project_id=p", 200,
                       R"({"results":[{"id":"wrong","project_id":"unselected","content":"Wrong project"}]})"});
  assert(client.sync(store) == SyncResult::InvalidData);
  store.finishWrite();
  assert(store.load(time(nullptr)) && store.count(0) == 2 && store.count(1) == 1);
  assert(responses.empty());
  std::cout << "Todoist client: selected projects, undated/future tasks, snapshot isolation, pagination, assignee "
               "filtering, auth, rate limits, lost ACK retry identity, offline "
               "completion and recurring conflict passed\n";
}
