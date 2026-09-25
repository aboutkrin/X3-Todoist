#pragma once
#include <cstdint>
#include <ctime>
inline unsigned long fakeMillis = 0;
inline unsigned long millis() { return fakeMillis; }
inline void delay(unsigned long ms) { fakeMillis += ms; }
constexpr int WL_CONNECTED = 3, WIFI_STA = 1;
class WifiStub {
 public:
  int connection = WL_CONNECTED;
  int status() { return connection; }
  void mode(int) {}
  void begin(const char*, const char*) {}
};
inline WifiStub WiFi;
