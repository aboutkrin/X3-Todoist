#pragma once
#include <optional>
#include <string>
struct WifiCredential {
  std::string ssid, password;
};
struct CredentialsStub {
  void loadFromFile() {}
  std::string getLastConnectedSsid() { return "test"; }
  std::optional<WifiCredential> findCredential(const std::string&) { return WifiCredential{"test", "test"}; }
};
inline CredentialsStub WIFI_STORE;
