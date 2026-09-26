#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>
constexpr int HTTP_GET = 0, HTTP_POST = 1;
constexpr size_t CONTENT_LENGTH_UNKNOWN = static_cast<size_t>(-1);
class WebServer {
 public:
  inline static WebServer* active = nullptr;
  std::map<std::pair<std::string, int>, std::function<void()>> handlers;
  std::vector<std::pair<std::string, std::string>> arguments;
  std::map<std::string, std::string> headers;
  std::string body;
  int status = 0;
  explicit WebServer(int) { active = this; }
  ~WebServer() { active = nullptr; }
  void on(const char* path, int method, std::function<void()> handler) { handlers[{path, method}] = handler; }
  void request(const char* path, int method, std::vector<std::pair<std::string, std::string>> args = {}) {
    arguments = std::move(args);
    body.clear();
    headers.clear();
    status = 0;
    handlers.at({path, method})();
  }
  std::string arg(const char* key) const {
    for (const auto& item : arguments)
      if (item.first == key) return item.second;
    return {};
  }
  std::string arg(int i) const { return arguments.at(i).second; }
  std::string argName(int i) const { return arguments.at(i).first; }
  int args() const { return static_cast<int>(arguments.size()); }
  void send(int code, const char* = "", const std::string& content = "") {
    status = code;
    body = content;
  }
  void sendHeader(const char* key, const char* value) { headers[key] = value; }
  void setContentLength(size_t) {}
  void sendContent(const char* value, size_t length) { body.append(value, length); }
  void sendContent(const char* value) { body += value; }
  void begin() {}
  void stop() {}
  void handleClient() {}
};
