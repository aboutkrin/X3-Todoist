#pragma once
#include <cassert>
#include <deque>
#include <string>
#include <vector>
using esp_err_t = int;
constexpr int ESP_OK = 0, ESP_FAIL = -1, HTTP_EVENT_ON_DATA = 1, HTTP_EVENT_ON_HEADER = 2, HTTP_METHOD_POST = 1;
struct esp_http_client_event_t {
  int event_id;
  void* user_data;
  int data_len;
  void* data;
  const char* header_key = nullptr;
  const char* header_value = nullptr;
};
struct esp_http_client_config_t {
  const char* url = nullptr;
  int (*crt_bundle_attach)(void*) = nullptr;
  int timeout_ms = 0;
  bool disable_auto_redirect = false;
  int (*event_handler)(esp_http_client_event_t*) = nullptr;
  void* user_data = nullptr;
  int buffer_size = 0, buffer_size_tx = 0;
};
struct HttpResponse {
  std::string path;
  int status = 200;
  std::string body;
  int result = ESP_OK;
  bool complete = true;
};
inline std::deque<HttpResponse> responses;
inline std::vector<std::string> posted;
struct HttpHandle {
  esp_http_client_config_t config;
  std::string url, body;
  int status = 0;
  bool complete = false;
};
inline HttpHandle* esp_http_client_init(esp_http_client_config_t* config) {
  assert(config->crt_bundle_attach);
  assert(config->disable_auto_redirect);
  return new HttpHandle{*config, config->url, {}};
}
inline void esp_http_client_set_header(HttpHandle*, const char*, const char*) {}
inline void esp_http_client_set_method(HttpHandle*, int) {}
inline void esp_http_client_set_post_field(HttpHandle* h, const char* p, size_t size) { h->body.assign(p, size); }
inline int esp_http_client_perform(HttpHandle* h) {
  assert(!responses.empty());
  auto response = responses.front();
  responses.pop_front();
  assert(h->url == "https://api.todoist.com/api/v1/" + response.path);
  if (!h->body.empty()) posted.push_back(h->body);
  if (response.body == "ACK") {
    // Echo the command UUID without depending on its random value.
    const auto begin = h->body.find("uuid%22%3A%22");
    assert(begin != std::string::npos);
    const auto start = begin + 13;
    const auto end = h->body.find("%22", start);
    const auto uuid = h->body.substr(start, end - start);
    response.body = "{\"sync_status\":{\"" + uuid + "\":\"ok\"}}";
  }
  h->status = response.status;
  h->complete = response.complete;
  esp_http_client_event_t event{HTTP_EVENT_ON_DATA, h->config.user_data, static_cast<int>(response.body.size()),
                                response.body.data()};
  if (h->config.event_handler(&event) != ESP_OK) return ESP_FAIL;
  return response.result;
}
inline int esp_http_client_get_status_code(HttpHandle* h) { return h->status; }
inline bool esp_http_client_is_complete_data_received(HttpHandle* h) { return h->complete; }
inline void esp_http_client_cleanup(HttpHandle* h) { delete h; }
