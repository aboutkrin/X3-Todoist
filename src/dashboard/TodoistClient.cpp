#include "TodoistClient.h"

#include <HalClock.h>
#include <HalPowerManager.h>
#include <Logging.h>
#include <Memory.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_random.h>
#include <sys/time.h>

#include <algorithm>
#include <cstddef>
#include <cstdlib>

#include "WifiCredentialStore.h"
#include "util/TaskWatchdog.h"

namespace dashboard {
namespace {
constexpr const char* RESPONSE = "/.crosspoint/dashboard-response.json";
constexpr size_t MAX_RESPONSE = 256 * 1024;
struct Sink {
  HalFile* file;
  size_t size = 0;
  bool failed = false;
  uint32_t retryAfter = REFRESH_SECONDS;
};

// JSON pages have variable-length strings. Cap all parser allocations together
// at 64KB instead of allowing an API response to consume the remaining heap.
class JsonAllocator final : public ArduinoJson::Allocator {
  struct alignas(std::max_align_t) Block {
    size_t size;
  };
  size_t used = 0;

 public:
  void* allocate(size_t size) override {
    if (size > 65536 - used) return nullptr;
    auto* block = static_cast<Block*>(malloc(sizeof(Block) + size));
    if (!block) return nullptr;
    block->size = size;
    used += size;
    return block + 1;
  }
  void deallocate(void* pointer) override {
    if (!pointer) return;
    auto* block = static_cast<Block*>(pointer) - 1;
    used -= block->size;
    free(block);
  }
  void* reallocate(void* pointer, size_t size) override {
    if (!pointer) return allocate(size);
    auto* old = static_cast<Block*>(pointer) - 1;
    if (size > 65536 - used + old->size) return nullptr;
    const size_t oldSize = old->size;
    auto* block = static_cast<Block*>(realloc(old, sizeof(Block) + size));
    if (!block) return nullptr;
    block->size = size;
    used = used - oldSize + size;
    return block + 1;
  }
};

esp_err_t httpEvent(esp_http_client_event_t* event) {
  resetTaskWatchdogIfSubscribed();
  auto* sink = static_cast<Sink*>(event->user_data);
  if (event->event_id == HTTP_EVENT_ON_HEADER && event->header_key && event->header_value &&
      strcasecmp(event->header_key, "Retry-After") == 0) {
    char* end = nullptr;
    const unsigned long seconds = strtoul(event->header_value, &end, 10);
    if (end != event->header_value && !*end) sink->retryAfter = std::min<unsigned long>(seconds, 86400);
  }
  if (event->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
  if (event->data_len < 0 || sink->size + event->data_len > MAX_RESPONSE ||
      sink->file->write(static_cast<const uint8_t*>(event->data), event->data_len) !=
          static_cast<size_t>(event->data_len)) {
    sink->failed = true;
    return ESP_FAIL;
  }
  sink->size += event->data_len;
  return ESP_OK;
}
std::string encode(const char* input) {
  static constexpr char hex[] = "0123456789ABCDEF";
  std::string result;
  result.reserve(strlen(input) * 3);
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(input); *p; ++p) {
    if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '-' || *p == '_')
      result += *p;
    else {
      result += '%';
      result += hex[*p >> 4];
      result += hex[*p & 15];
    }
  }
  return result;
}
bool readJson(JsonDocument& doc, JsonDocument& filter) {
  HalFile file;
  if (!Storage.openFileForRead("DASH", RESPONSE, file)) return false;
  struct Reader {
    HalFile& file;
    int read() { return file.read(); }
    size_t readBytes(char* buffer, size_t length) {
      const int count = file.read(buffer, length);
      return count > 0 ? static_cast<size_t>(count) : 0;
    }
  } reader{file};
  return !deserializeJson(doc, reader, DeserializationOption::Filter(filter), DeserializationOption::NestingLimit(12));
}
void uuid(char* out, size_t size) {
  uint8_t bytes[16];
  esp_fill_random(bytes, sizeof(bytes));
  bytes[6] = (bytes[6] & 0x0f) | 0x40;
  bytes[8] = (bytes[8] & 0x3f) | 0x80;
  snprintf(out, size, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", bytes[0], bytes[1],
           bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7], bytes[8], bytes[9], bytes[10], bytes[11],
           bytes[12], bytes[13], bytes[14], bytes[15]);
}
}  // namespace

SyncResult TodoistClient::connect() {
  if (!config.configured()) return SyncResult::NotConfigured;
  if (WiFi.status() != WL_CONNECTED) {
    WIFI_STORE.loadFromFile();
    const auto saved = WIFI_STORE.findCredential(WIFI_STORE.getLastConnectedSsid());
    if (!saved) return SyncResult::Offline;
    WiFi.mode(WIFI_STA);
    WiFi.begin(saved->ssid.c_str(), saved->password.c_str());
    const auto started = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - started < 12000) {
      resetTaskWatchdogIfSubscribed();
      delay(50);
    }
    if (WiFi.status() != WL_CONNECTED) return SyncResult::Offline;
  }
  // TLS certificate validity uses the system clock, whereas the X3 RTC is
  // independent. Restore UTC from the HAL's local calendar on every wake.
  tm local{};
  if (halClock.localTime(local) && local.tm_year >= 124) {
    local.tm_isdst = -1;
    timeval value{mktime(&local), 0};
    settimeofday(&value, nullptr);
  }
  if (time(nullptr) < 1704067200 && !halClock.syncFromNTP()) return SyncResult::Clock;
  return SyncResult::Ok;
}
SyncResult TodoistClient::request(const char* path, const char* body) {
  Preferences cooldown;
  if (cooldown.begin("dash-http", false) && cooldown.getLong64("retry-at", 0) > time(nullptr))
    return SyncResult::RateLimited;
  HalFile file;
  if (!Storage.openFileForWrite("DASH", RESPONSE, file)) return SyncResult::StorageError;
  Sink sink{&file};
  // The SDK configuration exceeds the small local-variable budget.
  auto options = makeUniqueNoThrow<esp_http_client_config_t>();
  if (!options) {
    LOG_ERR("DASH", "HTTP config allocation failed");
    return SyncResult::Transport;
  }
  std::string url = "https://api.todoist.com/api/v1/";
  url += path;
  options->url = url.c_str();
  options->crt_bundle_attach = esp_crt_bundle_attach;
  options->timeout_ms = 10000;
  options->disable_auto_redirect = true;
  options->event_handler = httpEvent;
  options->user_data = &sink;
  options->buffer_size = 1024;
  options->buffer_size_tx = 1024;
  auto http = esp_http_client_init(options.get());
  if (!http) {
    LOG_ERR("DASH", "HTTP client initialization failed");
    return SyncResult::Transport;
  }
  const std::string authorization = std::string("Bearer ") + config.token;
  esp_http_client_set_header(http, "Authorization", authorization.c_str());
  esp_http_client_set_header(http, "Accept", "application/json");
  esp_http_client_set_header(http, "User-Agent", "X3-Personal-Dashboard/1");
  if (body) {
    esp_http_client_set_method(http, HTTP_METHOD_POST);
    esp_http_client_set_header(http, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_post_field(http, body, strlen(body));
  }
  resetTaskWatchdogIfSubscribed();
  const auto result = esp_http_client_perform(http);
  const int code = esp_http_client_get_status_code(http);
  const bool complete = esp_http_client_is_complete_data_received(http);
  if (result != ESP_OK || !complete || code != 200 || sink.failed) {
    int tlsCode = 0, tlsFlags = 0;
    const auto tlsError = esp_http_client_get_and_clear_last_tls_error(http, &tlsCode, &tlsFlags);
    LOG_ERR("DASH", "HTTP %.*s: result=%d status=%d complete=%d bytes=%u storage=%d",
            static_cast<int>(strcspn(path, "?/")), path, static_cast<int>(result), code, complete,
            static_cast<unsigned>(sink.size), sink.failed);
    LOG_ERR("DASH", "Transport errno=%d TLS=%d code=%d flags=%x", esp_http_client_get_errno(http),
            static_cast<int>(tlsError), tlsCode, tlsFlags);
  }
  esp_http_client_cleanup(http);
  file.flush();
  if (sink.failed) return SyncResult::StorageError;
  if (code == 401 || code == 403) return SyncResult::Auth;
  if (code == 429) {
    cooldown.putLong64("retry-at", time(nullptr) + sink.retryAfter);
    return SyncResult::RateLimited;
  }
  if (result != ESP_OK || !complete || code >= 500) return SyncResult::Transport;
  if (code != 200) return SyncResult::InvalidData;
  return SyncResult::Ok;
}
SyncResult TodoistClient::projects() {
  ProjectCatalog catalog;
  if (!catalog.beginWrite()) return SyncResult::StorageError;
  std::string cursor;
  for (int page = 0; page < 100; ++page) {
    const std::string path =
        "projects?limit=20" + (cursor.empty() ? std::string() : "&cursor=" + encode(cursor.c_str()));
    auto result = request(path.c_str());
    if (result != SyncResult::Ok) return result;
    JsonAllocator allocator;
    JsonDocument doc(&allocator), filter(&allocator);
    filter["results"][0]["id"] = true;
    filter["results"][0]["name"] = true;
    filter["next_cursor"] = true;
    if (!readJson(doc, filter) || !doc["results"].is<JsonArray>()) return SyncResult::InvalidData;
    for (JsonObjectConst item : doc["results"].as<JsonArrayConst>()) {
      Project project;
      if (!copyText(project.id, sizeof(project.id), item["id"] | "") || !project.id[0] ||
          !copyText(project.name, sizeof(project.name), item["name"] | ""))
        return SyncResult::Limit;
      if (!catalog.append(project)) return SyncResult::StorageError;
    }
    const char* next = doc["next_cursor"] | "";
    if (!*next) return catalog.commit() ? SyncResult::Ok : SyncResult::StorageError;
    if (strlen(next) > 512 || cursor == next) return SyncResult::InvalidData;
    cursor = next;
  }
  return SyncResult::Limit;
}
bool TodoistClient::projectName(const char* id, char* out, size_t capacity) {
  ProjectCatalog catalog;
  if (!catalog.load()) return false;
  Project project;
  if (catalog.find(id, project)) return copyText(out, capacity, project.name);
  return copyText(out, capacity, id);
}
SyncResult TodoistClient::refreshProjects() {
  HalPowerManager::Lock power;
  const auto result = connect();
  return result == SyncResult::Ok ? projects() : result;
}
SyncResult TodoistClient::sync(Store& store) {
  if (!selection.count) return SyncResult::ChooseProjects;
  HalPowerManager::Lock power;
  auto result = connect();
  if (result != SyncResult::Ok) return result;
  char userId[64]{};
  {
    result = request("sync", "sync_token=*&resource_types=%5B%22user%22%5D");
    if (result != SyncResult::Ok) return result;
    JsonAllocator allocator;
    JsonDocument doc(&allocator), filter(&allocator);
    filter["user"]["id"] = true;
    if (!readJson(doc, filter) || !copyText(userId, sizeof(userId), doc["user"]["id"] | "") || !userId[0])
      return SyncResult::InvalidData;
  }
  result = projects();
  if (result != SyncResult::Ok) return result;
  ProjectCatalog catalog;
  if (!catalog.load()) return SyncResult::StorageError;
  if (!store.startWrite()) return SyncResult::StorageError;
  // One reusable record keeps long task text off the loop stack.
  auto task = makeUniqueNoThrow<Task>();
  if (!task) {
    LOG_ERR("DASH", "Task allocation failed");
    return SyncResult::Transport;
  }
  for (size_t projectIndex = 0; projectIndex < selection.count; ++projectIndex) {
    Project project;
    if (!catalog.find(selection.projects[projectIndex].id, project)) continue;
    std::string cursor;
    bool finished = false;
    for (int page = 0; page < 500; ++page) {
      const std::string path = "tasks?limit=20&project_id=" + encode(project.id) +
                               (cursor.empty() ? std::string() : "&cursor=" + encode(cursor.c_str()));
      result = request(path.c_str());
      if (result != SyncResult::Ok) return result;
      JsonAllocator allocator;
      JsonDocument doc(&allocator), filter(&allocator);
      for (const char* key : {"id", "content", "description", "project_id", "parent_id", "responsible_uid", "priority",
                              "checked", "is_deleted"})
        filter["results"][0][key] = true;
      filter["results"][0]["due"]["date"] = true;
      filter["results"][0]["due"]["datetime"] = true;
      filter["results"][0]["due"]["is_recurring"] = true;
      filter["next_cursor"] = true;
      if (!readJson(doc, filter) || !doc["results"].is<JsonArray>()) return SyncResult::InvalidData;
      for (JsonObjectConst item : doc["results"].as<JsonArrayConst>()) {
        const char* assignee = item["responsible_uid"] | "";
        if ((item["checked"] | false) || (item["is_deleted"] | false) || (*assignee && strcmp(assignee, userId)))
          continue;
        const char* due = item["due"]["datetime"] | (item["due"]["date"] | "");
        if (strcmp(item["project_id"] | "", project.id)) return SyncResult::InvalidData;
        memset(task.get(), 0, sizeof(Task));
        if (!copyText(task->id, sizeof(task->id), item["id"] | "") || !task->id[0] ||
            !copyText(task->projectId, sizeof(task->projectId), project.id) ||
            !copyText(task->parentId, sizeof(task->parentId), item["parent_id"] | "") ||
            !copyText(task->title, sizeof(task->title), item["content"] | "") ||
            !copyText(task->description, sizeof(task->description), item["description"] | "") ||
            !copyText(task->due, sizeof(task->due), due) ||
            !copyText(task->project, sizeof(task->project), project.name))
          return SyncResult::Limit;
        task->priority = item["priority"] | 1;
        task->recurring = item["due"]["is_recurring"] | false;
        if (!store.append(*task)) return SyncResult::Limit;
      }
      const char* next = doc["next_cursor"] | "";
      if (!*next) {
        finished = true;
        break;
      }
      if (strlen(next) > 512 || cursor == next) return SyncResult::InvalidData;
      cursor = next;
    }
    if (!finished) return SyncResult::Limit;
  }
  return store.commit(time(nullptr)) ? SyncResult::Ok : SyncResult::StorageError;
}
SyncResult TodoistClient::sendPending(Pending& pending) {
  auto result = connect();
  if (result != SyncResult::Ok) return result;
  JsonAllocator allocator;
  JsonDocument doc(&allocator), filter(&allocator);
  auto command = doc.to<JsonArray>().add<JsonObject>();
  command["type"] = "item_close";
  command["uuid"] = pending.uuid;
  command["args"]["id"] = pending.id;
  std::string commands;
  serializeJson(doc, commands);
  const std::string body = "commands=" + encode(commands.c_str());
  result = request("sync", body.c_str());
  if (result != SyncResult::Ok) return result;
  doc.clear();
  filter["sync_status"] = true;
  if (!readJson(doc, filter)) return SyncResult::Pending;
  const char* status = doc["sync_status"][pending.uuid].as<const char*>();
  if (!status || strcmp(status, "ok")) {
    if (doc["sync_status"][pending.uuid]["error_code"].is<int>()) {
      return pending.clear() ? SyncResult::InvalidData : SyncResult::StorageError;
    }
    return SyncResult::Pending;
  }
  pending.acknowledged = true;
  return pending.save() ? SyncResult::Ok : SyncResult::StorageError;
}
SyncResult TodoistClient::complete(const Task& task, Pending& pending, Store& store) {
  if (pending.exists()) return SyncResult::Pending;
  {
    HalPowerManager::Lock power;
    const auto result = connect();
    if (result != SyncResult::Ok) return result;
    // A recurring task may have been completed on the phone since the cached
    // screen was shown. Never complete its new occurrence using an old screen.
    if (task.recurring) {
      const std::string path = "tasks/" + encode(task.id);
      const auto check = request(path.c_str());
      if (check != SyncResult::Ok) return check;
      JsonAllocator allocator;
      JsonDocument doc(&allocator), filter(&allocator);
      filter["due"]["date"] = true;
      filter["due"]["datetime"] = true;
      if (!readJson(doc, filter)) return SyncResult::InvalidData;
      const char* currentDue = doc["due"]["datetime"] | (doc["due"]["date"] | "");
      if (strcmp(task.due, currentDue)) return SyncResult::InvalidData;
    }
    copyText(pending.id, sizeof(pending.id), task.id);
    uuid(pending.uuid, sizeof(pending.uuid));
    pending.acknowledged = false;
    if (!pending.save()) {
      pending = Pending{};
      return SyncResult::StorageError;
    }
  }
  return reconcile(pending, store);
}
SyncResult TodoistClient::reconcile(Pending& pending, Store& store) {
  if (!pending.exists()) return sync(store);
  if (!pending.acknowledged) {
    HalPowerManager::Lock power;
    const auto result = sendPending(pending);
    if (result != SyncResult::Ok) return result;
  }
  const auto result = sync(store);
  if (result != SyncResult::Ok) return result;
  return pending.clear() ? SyncResult::Ok : SyncResult::StorageError;
}
}  // namespace dashboard
