// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2025 Mathieu Carbou
 */
#include "MycilaESPConnect.h"
#include "MycilaESPConnect_Includes.h"
#include "MycilaESPConnect_Logging.h"

void Mycila::ESPConnect::_startSTA() {
  LOGI(TAG, "Starting WiFi...");
  _setState(Mycila::ESPConnect::State::NETWORK_CONNECTING);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_MODE_NULL);

#ifndef ESP8266
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
#endif

  WiFi.setHostname(_config.hostname.c_str());
  WiFi.setSleep(false);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);

  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.mode(WIFI_MODE_STA);
#ifndef ESP8266
  WiFi.enableIPv6();
#endif

#ifndef ESPCONNECT_ETH_SUPPORT
  if (_config.ipConfig.ip) {
    LOGI(TAG, "Set WiFi Static IP Configuration:");
    LOGI(TAG, " - IP: %s", _config.ipConfig.ip.toString().c_str());
    LOGI(TAG, " - Gateway: %s", _config.ipConfig.gateway.toString().c_str());
    LOGI(TAG, " - Subnet: %s", _config.ipConfig.subnet.toString().c_str());
    LOGI(TAG, " - DNS: %s", _config.ipConfig.dns.toString().c_str());

    WiFi.config(_config.ipConfig.ip, _config.ipConfig.gateway, _config.ipConfig.subnet, _config.ipConfig.dns);
  }
#endif

  if (_config.wifiBSSID.length()) {
    LOGI(TAG, "Connecting to SSID: %s with BSSID: %s", _config.wifiSSID.c_str(), _config.wifiBSSID.c_str());

    MacAddress bssid(MACType::MAC6);
    bssid.fromString(_config.wifiBSSID.c_str());

    WiFi.begin(_config.wifiSSID.c_str(), _config.wifiPassword.c_str(), 0, bssid);
  } else {
    LOGI(TAG, "Connecting to SSID: %s", _config.wifiSSID.c_str());
    WiFi.begin(_config.wifiSSID.c_str(), _config.wifiPassword.c_str());
  }

  _lastTime = millis();

  LOGD(TAG, "WiFi started.");
}
