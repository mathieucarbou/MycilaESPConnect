// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2025 Mathieu Carbou
 */
#include "MycilaESPConnect.h"
#include "MycilaESPConnect_Includes.h"
#include "MycilaESPConnect_Logging.h"

void Mycila::ESPConnect::_startAP() {
  LOGI(TAG, "Starting Access Point...");
  _setState(Mycila::ESPConnect::State::AP_STARTING);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_MODE_NULL);

#ifndef ESP8266
  WiFi.softAPsetHostname(_config.hostname.c_str());
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
#endif

  WiFi.setHostname(_config.hostname.c_str());
  WiFi.setSleep(false);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);

  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.mode(WIFI_MODE_AP);

  if (!_apPassword.length() || _apPassword.length() < 8) {
    // Disabling invalid Access Point password which must be at least 8 characters long when set
    WiFi.softAP(_apSSID.c_str(), "");
  } else
    WiFi.softAP(_apSSID.c_str(), _apPassword.c_str());

  if (_dnsServer == nullptr) {
    _dnsServer = new DNSServer();
    _dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    _dnsServer->start(53, "*", WiFi.softAPIP());
  }

  LOGI(TAG, "Access Point started.");

#ifdef ESP8266
  _onWiFiEvent(ARDUINO_EVENT_WIFI_AP_START);
#endif
}

void Mycila::ESPConnect::_stopAP() {
  LOGI(TAG, "Stopping Access Point...");
  WiFi.softAPdisconnect(true);
  if (_dnsServer != nullptr) {
    _dnsServer->stop();
    delete _dnsServer;
    _dnsServer = nullptr;
  }
  LOGD(TAG, "Access Point stopped.");
}
