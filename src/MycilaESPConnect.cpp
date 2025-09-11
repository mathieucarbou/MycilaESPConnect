// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2025 Mathieu Carbou
 */
#include "MycilaESPConnect.h"
#include "MycilaESPConnect_Includes.h"
#include "MycilaESPConnect_Logging.h"

#include <cstdio>

static const char* NetworkStateNames[] = {
  "NETWORK_DISABLED",
  "NETWORK_ENABLED",
  "NETWORK_CONNECTING",
  "NETWORK_TIMEOUT",
  "NETWORK_CONNECTED",
  "NETWORK_DISCONNECTED",
  "NETWORK_RECONNECTING",
  "AP_STARTING",
  "AP_STARTED",
  "PORTAL_STARTING",
  "PORTAL_STARTED",
  "PORTAL_COMPLETE",
  "PORTAL_TIMEOUT",
};

const char* Mycila::ESPConnect::getStateName() const {
  return NetworkStateNames[static_cast<int>(_state)];
}

const char* Mycila::ESPConnect::getStateName(Mycila::ESPConnect::State state) const {
  return NetworkStateNames[static_cast<int>(state)];
}

Mycila::ESPConnect::Mode Mycila::ESPConnect::getMode() const {
  switch (_state) {
    case Mycila::ESPConnect::State::AP_STARTED:
      return Mycila::ESPConnect::Mode::AP;
#ifndef ESPCONNECT_NO_CAPTIVE_PORTAL
    case Mycila::ESPConnect::State::PORTAL_STARTED:
      return Mycila::ESPConnect::Mode::AP;
#endif
    case Mycila::ESPConnect::State::NETWORK_CONNECTED:
    case Mycila::ESPConnect::State::NETWORK_DISCONNECTED:
    case Mycila::ESPConnect::State::NETWORK_RECONNECTING:
#ifdef ESPCONNECT_ETH_SUPPORT
      if (ETH.linkUp() && ETH.localIP()[0] != 0)
        return Mycila::ESPConnect::Mode::ETH;
#endif
      if (WiFi.localIP()[0] != 0)
        return Mycila::ESPConnect::Mode::STA;
      return Mycila::ESPConnect::Mode::NONE;
    default:
      return Mycila::ESPConnect::Mode::NONE;
  }
}

ESPCONNECT_STRING Mycila::ESPConnect::getMACAddress(Mycila::ESPConnect::Mode mode) const {
  ESPCONNECT_STRING mac;

  switch (mode) {
    case Mycila::ESPConnect::Mode::AP:
      mac = WiFi.softAPmacAddress().c_str();
      break;
    case Mycila::ESPConnect::Mode::STA:
      mac = WiFi.macAddress().c_str();
      break;
#ifdef ESPCONNECT_ETH_SUPPORT
    case Mycila::ESPConnect::Mode::ETH:
      if (ETH.linkUp())
        mac = ETH.macAddress().c_str();
      break;
#endif
    default:
      break;
  }

  if (mac.length() && mac != "00:00:00:00:00:00")
    return mac;

#ifdef ESP8266
  switch (mode) {
    case Mycila::ESPConnect::Mode::AP:
      return WiFi.softAPmacAddress().c_str();
    case Mycila::ESPConnect::Mode::STA:
      return WiFi.macAddress().c_str();
    default:
      // ETH not supported with ESP8266
      return {};
  }
#else
  // ESP_MAC_IEEE802154 is used to mean "no MAC address" in this context
  esp_mac_type_t type = esp_mac_type_t::ESP_MAC_IEEE802154;

  switch (mode) {
    case Mycila::ESPConnect::Mode::AP:
      type = ESP_MAC_WIFI_SOFTAP;
      break;
    case Mycila::ESPConnect::Mode::STA:
      type = ESP_MAC_WIFI_STA;
      break;
  #ifdef ESPCONNECT_ETH_SUPPORT
    case Mycila::ESPConnect::Mode::ETH:
      type = ESP_MAC_ETH;
      break;
  #endif
    default:
      break;
  }

  if (type == esp_mac_type_t::ESP_MAC_IEEE802154)
    return "";

  uint8_t bytes[6] = {0, 0, 0, 0, 0, 0};
  if (esp_read_mac(bytes, type) != ESP_OK)
    return "";

  char buffer[18] = {0};
  snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X", bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]);
  return buffer;
#endif
}

IPAddress Mycila::ESPConnect::getIPAddress(Mycila::ESPConnect::Mode mode) const {
  const wifi_mode_t wifiMode = WiFi.getMode();
  switch (mode) {
    case Mycila::ESPConnect::Mode::AP:
      return wifiMode == WIFI_MODE_AP || wifiMode == WIFI_MODE_APSTA ? WiFi.softAPIP() : IPAddress();
    case Mycila::ESPConnect::Mode::STA:
      return wifiMode == WIFI_MODE_STA ? WiFi.localIP() : IPAddress();
#ifdef ESPCONNECT_ETH_SUPPORT
    case Mycila::ESPConnect::Mode::ETH:
      return ETH.linkUp() ? ETH.localIP() : IPAddress();
#endif
    default:
      return IPAddress();
  }
}

IPAddress Mycila::ESPConnect::getLinkLocalIPv6Address(Mycila::ESPConnect::Mode mode) const {
#ifndef ESP8266
  const wifi_mode_t wifiMode = WiFi.getMode();
  switch (mode) {
    case Mycila::ESPConnect::Mode::AP:
      return IN6ADDR_ANY;
    case Mycila::ESPConnect::Mode::STA:
      return wifiMode == WIFI_MODE_STA ? WiFi.linkLocalIPv6() : IN6ADDR_ANY;
  #ifdef ESPCONNECT_ETH_SUPPORT
    case Mycila::ESPConnect::Mode::ETH:
      return ETH.linkUp() ? ETH.linkLocalIPv6() : IN6ADDR_ANY;
  #endif
    default:
      return IN6ADDR_ANY;
  }
#else
  return IPAddress();
#endif
}

IPAddress Mycila::ESPConnect::getGlobalIPv6Address(Mycila::ESPConnect::Mode mode) const {
#ifndef ESP8266
  const wifi_mode_t wifiMode = WiFi.getMode();
  switch (mode) {
    case Mycila::ESPConnect::Mode::AP:
      return IN6ADDR_ANY;
    case Mycila::ESPConnect::Mode::STA:
      return wifiMode == WIFI_MODE_STA ? WiFi.globalIPv6() : IN6ADDR_ANY;
  #ifdef ESPCONNECT_ETH_SUPPORT
    case Mycila::ESPConnect::Mode::ETH:
      return ETH.linkUp() ? ETH.globalIPv6() : IN6ADDR_ANY;
  #endif
    default:
      return IN6ADDR_ANY;
  }
#else
  return IPAddress();
#endif
}

ESPCONNECT_STRING Mycila::ESPConnect::getWiFiSSID() const {
  switch (WiFi.getMode()) {
    case WIFI_MODE_AP:
    case WIFI_MODE_APSTA:
      return _apSSID;
    case WIFI_MODE_STA:
      return _config.wifiSSID;
    default:
      return {};
  }
}

ESPCONNECT_STRING Mycila::ESPConnect::getWiFiBSSID() const {
  switch (WiFi.getMode()) {
    case WIFI_MODE_AP:
    case WIFI_MODE_APSTA:
      return WiFi.softAPmacAddress().c_str();
    case WIFI_MODE_STA:
      return WiFi.BSSIDstr().c_str();
    default:
      return {};
  }
}

int8_t Mycila::ESPConnect::getWiFiRSSI() const {
  return WiFi.getMode() == WIFI_MODE_STA ? WiFi.RSSI() : 0;
}

int8_t Mycila::ESPConnect::getWiFiSignalQuality() const {
  return WiFi.getMode() == WIFI_MODE_STA ? _wifiSignalQuality(WiFi.RSSI()) : 0;
}

int8_t Mycila::ESPConnect::_wifiSignalQuality(int32_t rssi) {
  int32_t s = map(rssi, -90, -30, 0, 100);
  return s > 100 ? 100 : (s < 0 ? 0 : s);
}

void Mycila::ESPConnect::loadConfiguration(Mycila::ESPConnect::Config& config) {
  LOGD(TAG, "Loading config...");
  Preferences preferences;
  preferences.begin("espconnect", true);
  // ap
  config.apMode = preferences.isKey("ap") ? preferences.getBool("ap", false) : false;
  // bssid
  if (preferences.isKey("bssid"))
    config.wifiBSSID = preferences.getString("bssid").c_str();
  // ssid
  if (preferences.isKey("ssid"))
    config.wifiSSID = preferences.getString("ssid").c_str();
  // password
  if (preferences.isKey("password"))
    config.wifiPassword = preferences.getString("password").c_str();
  // ip
  if (preferences.isKey("ip"))
    config.ipConfig.ip.fromString(preferences.getString("ip"));
  // subnet
  if (preferences.isKey("subnet"))
    config.ipConfig.subnet.fromString(preferences.getString("subnet"));
  // gateway
  if (preferences.isKey("gateway"))
    config.ipConfig.gateway.fromString(preferences.getString("gateway"));
  // dns
  if (preferences.isKey("dns"))
    config.ipConfig.dns.fromString(preferences.getString("dns"));
  // hostname
  if (preferences.isKey("hostname"))
    config.hostname = preferences.getString("hostname").c_str();
  preferences.end();
  LOGD(TAG, " - AP: %d", config.apMode);
  LOGD(TAG, " - BSSID: %s", config.wifiBSSID.c_str());
  LOGD(TAG, " - SSID: %s", config.wifiSSID.c_str());
  LOGD(TAG, " - IP: %s", config.ipConfig.ip.toString().c_str());
  LOGD(TAG, " - Subnet: %s", config.ipConfig.subnet.toString().c_str());
  LOGD(TAG, " - Gateway: %s", config.ipConfig.gateway.toString().c_str());
  LOGD(TAG, " - DNS: %s", config.ipConfig.dns.toString().c_str());
  LOGD(TAG, " - Hostname: %s", config.hostname.c_str());
}

void Mycila::ESPConnect::saveConfiguration(const Mycila::ESPConnect::Config& config) {
  LOGD(TAG, "Saving config...");
  LOGD(TAG, " - AP: %d", config.apMode);
  LOGD(TAG, " - BSSID: %s", config.wifiBSSID.c_str());
  LOGD(TAG, " - SSID: %s", config.wifiSSID.c_str());
  LOGD(TAG, " - IP: %s", config.ipConfig.ip.toString().c_str());
  LOGD(TAG, " - Subnet: %s", config.ipConfig.subnet.toString().c_str());
  LOGD(TAG, " - Gateway: %s", config.ipConfig.gateway.toString().c_str());
  LOGD(TAG, " - DNS: %s", config.ipConfig.dns.toString().c_str());
  LOGD(TAG, " - Hostname: %s", config.hostname.c_str());
  Preferences preferences;
  preferences.begin("espconnect", false);
  preferences.putBool("ap", config.apMode);
  preferences.putString("bssid", config.wifiBSSID.c_str());
  preferences.putString("ssid", config.wifiSSID.c_str());
  preferences.putString("password", config.wifiPassword.c_str());
  preferences.putString("ip", config.ipConfig.ip.toString().c_str());
  preferences.putString("subnet", config.ipConfig.subnet.toString().c_str());
  preferences.putString("gateway", config.ipConfig.gateway.toString().c_str());
  preferences.putString("dns", config.ipConfig.dns.toString().c_str());
  preferences.putString("hostname", config.hostname.c_str());
  preferences.end();
}

void Mycila::ESPConnect::clearConfiguration() {
  LOGD(TAG, "Clearing config...");
  Preferences preferences;
  preferences.begin("espconnect", false);
  preferences.clear();
  preferences.end();
}
