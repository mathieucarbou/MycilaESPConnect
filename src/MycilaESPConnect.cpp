// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2023 Mathieu Carbou and others
 */
#include "MycilaESPConnect.h"

#include <espconnect_webpage.h>

#include <AsyncJson.h>
#include <ESPmDNS.h>
#include <ETH.h>
#include <Preferences.h>
#include <functional>

#define TAG "ESPCONNECT"

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

const char* ESPConnectClass::getStateName() const {
  return NetworkStateNames[static_cast<int>(_state)];
}

const char* ESPConnectClass::getStateName(ESPConnectState state) const {
  return NetworkStateNames[static_cast<int>(state)];
}

ESPConnectMode ESPConnectClass::getMode() const {
  switch (_state) {
    case ESPConnectState::AP_STARTED:
    case ESPConnectState::PORTAL_STARTED:
      return ESPConnectMode::AP;
      break;
    case ESPConnectState::NETWORK_CONNECTED:
    case ESPConnectState::NETWORK_DISCONNECTED:
    case ESPConnectState::NETWORK_RECONNECTING:
      return ETH.localIP()[0] != 0 ? ESPConnectMode::ETH : (WiFi.localIP()[0] != 0 ? ESPConnectMode::STA : ESPConnectMode::NONE);
    default:
      return ESPConnectMode::NONE;
  }
}

bool ESPConnectClass::isConnected() const {
  return getIPAddress()[0] != 0;
}

const String ESPConnectClass::getMACAddress() const {
  switch (getMode()) {
    case ESPConnectMode::AP:
    case ESPConnectMode::STA:
      return WiFi.macAddress();
    case ESPConnectMode::ETH:
      return ETH.macAddress();
    default:
      return emptyString;
  }
}

const IPAddress ESPConnectClass::getIPAddress() const {
  switch (getMode()) {
    case ESPConnectMode::AP:
      return WiFi.softAPIP();
    case ESPConnectMode::STA:
      return WiFi.localIP();
    case ESPConnectMode::ETH:
      return ETH.localIP();
    default:
      return IPAddress();
  }
}

const String ESPConnectClass::getWiFiSSID() const {
  switch (getMode()) {
    case ESPConnectMode::AP:
      return _apSSID;
    case ESPConnectMode::STA:
      return _config.wifiSSID;
    default:
      return emptyString;
  }
}

const String ESPConnectClass::getWiFiBSSID() const {
  switch (getMode()) {
    case ESPConnectMode::AP:
      return WiFi.softAPmacAddress();
    case ESPConnectMode::STA:
      return WiFi.BSSIDstr();
    default:
      return emptyString;
  }
}

int8_t ESPConnectClass::getWiFiRSSI() const {
  return _state == ESPConnectState::NETWORK_CONNECTED && WiFi.getMode() == WIFI_MODE_STA ? WiFi.RSSI() : 0;
}

int8_t ESPConnectClass::getWiFiSignalQuality() const {
  return _state == ESPConnectState::NETWORK_CONNECTED && WiFi.getMode() == WIFI_MODE_STA ? _wifiSignalQuality(WiFi.RSSI()) : 0;
}

int8_t ESPConnectClass::_wifiSignalQuality(int32_t rssi) {
  int32_t s = map(rssi, -90, -30, 0, 100);
  return s > 100 ? 100 : (s < 0 ? 0 : s);
}

void ESPConnectClass::begin(AsyncWebServer* httpd, const String& hostname, const String& apSSID, const String& apPassword) {
  if (_state != ESPConnectState::NETWORK_DISABLED)
    return;

  _autoSave = true;

  Preferences preferences;
  preferences.begin("espconnect", true);
  String ssid = preferences.isKey("ssid") ? preferences.getString("ssid", emptyString) : emptyString;
  String password = preferences.isKey("password") ? preferences.getString("password", emptyString) : emptyString;
  bool ap = preferences.isKey("ap") ? preferences.getBool("ap", false) : false;
  preferences.end();

  begin(httpd, hostname, apSSID, apPassword, {ssid, password, ap});
}

void ESPConnectClass::begin(AsyncWebServer* httpd, const String& hostname, const String& apSSID, const String& apPassword, const ESPConnectConfig& config) {
  if (_state != ESPConnectState::NETWORK_DISABLED)
    return;

  _httpd = httpd;
  _hostname = hostname;
  _apSSID = apSSID;
  _apPassword = apPassword;
  _config = config; // copy values

  _wifiEventListenerId = WiFi.onEvent(std::bind(&ESPConnectClass::_onWiFiEvent, this, std::placeholders::_1));

  httpd->on("/espconnect/scan", HTTP_GET, [&](AsyncWebServerRequest* request) {
    AsyncJsonResponse* response = new AsyncJsonResponse(true);
    JsonArray json = response->getRoot();
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED)
    {
      WiFi.scanNetworks(true);
      return request->send(202);
    }
    else if (n == WIFI_SCAN_RUNNING)
      return request->send(202);
    else
    {
      for (int i = 0; i < n; ++i)
      {
#if ARDUINOJSON_VERSION_MAJOR == 6
        JsonObject entry = json.createNestedObject();
#else
        JsonObject entry = json.add<JsonObject>();
#endif
        entry["name"] = WiFi.SSID(i);
        entry["rssi"] = WiFi.RSSI(i);
        entry["signal"] = _wifiSignalQuality(WiFi.RSSI(i));
        entry["open"] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
      }
      WiFi.scanDelete();
      if (WiFi.scanComplete() == WIFI_SCAN_FAILED)
        WiFi.scanNetworks(true);
    }
    response->setLength();
    request->send(response); });

  httpd->on("/espconnect/connect", HTTP_POST, [&](AsyncWebServerRequest* request) {
    _config.apMode = (request->hasParam("ap_mode", true) ? request->getParam("ap_mode", true)->value() : emptyString) == "true";
    if (_config.apMode) {
      request->send(200, "application/json", "{\"message\":\"Configuration Saved.\"}");
      _setState(ESPConnectState::PORTAL_COMPLETE);
    } else {
      String ssid = request->hasParam("ssid", true) ? request->getParam("ssid", true)->value() : emptyString;
      String password = request->hasParam("password", true) ? request->getParam("password", true)->value() : emptyString;
      if (ssid.isEmpty())
        return request->send(403, "application/json", "{\"message\":\"Invalid SSID\"}");
      if (ssid.length() > 32 || password.length() > 64 || (!password.isEmpty() && password.length() < 8))
        return request->send(403, "application/json", "{\"message\":\"Credentials exceed character limit of 32 & 64 respectively, or password lower than 8 characters.\"}");
      _config.wifiSSID = ssid;
      _config.wifiPassword = password;
      request->send(200, "application/json", "{\"message\":\"Configuration Saved.\"}");
      _setState(ESPConnectState::PORTAL_COMPLETE);
    } });

  httpd->on("/espconnect", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", ESPCONNECT_HTML, sizeof(ESPCONNECT_HTML));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response); });

  // this filter makes sure that the root path is only rewritten when captive portal is started
  httpd->rewrite("/", "/espconnect").setFilter([&](AsyncWebServerRequest* request) {
    return _state == ESPConnectState::PORTAL_STARTED;
  });

  _state = ESPConnectState::NETWORK_ENABLED;

  // blocks like the old behaviour
  if (_blocking) {
    ESP_LOGD(TAG, "begin(blocking=true)");
    while (_state != ESPConnectState::AP_STARTED && _state != ESPConnectState::NETWORK_CONNECTED) {
      loop();
      delay(100);
    }
  } else {
    ESP_LOGD(TAG, "begin(blocking=false)");
  }
}

void ESPConnectClass::end() {
  if (_state == ESPConnectState::NETWORK_DISABLED)
    return;
  ESP_LOGD(TAG, "end()");
  _lastTime = -1;
  _autoSave = false;
  _setState(ESPConnectState::NETWORK_DISABLED);
  WiFi.removeEvent(_wifiEventListenerId);
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_MODE_NULL);
  _stopAP();
  _httpd = nullptr;
}

void ESPConnectClass::loop() {
  if (_dnsServer != nullptr)
    _dnsServer->processNextRequest();

  // first check if we have to enter AP mode
  if (_state == ESPConnectState::NETWORK_ENABLED && _config.apMode)
    _startAP(false);

  // otherwise, tries to connect to WiFi or ethernet
  if (_state == ESPConnectState::NETWORK_ENABLED) {
    if (!_config.wifiSSID.isEmpty())
      _startSTA();
    if (_allowEthernet)
      _startEthernet();
  }

  // connection to WiFi or Ethernet times out ?
  if (_state == ESPConnectState::NETWORK_CONNECTING && _durationPassed(_connectTimeout)) {
    WiFi.disconnect(true, true);
    _setState(ESPConnectState::NETWORK_TIMEOUT);
  }

  // check if we have to enter captive portal mode
  if (_state == ESPConnectState::NETWORK_TIMEOUT ||
      (_state == ESPConnectState::NETWORK_ENABLED && !_allowEthernet && _config.wifiSSID.isEmpty()))
    _startAP(true);

  // portal duration ends ?
  if (_state == ESPConnectState::PORTAL_STARTED && _durationPassed(_portalTimeout)) {
    _setState(ESPConnectState::PORTAL_TIMEOUT);
  }

  // disconnect from network ? reconnect!
  if (_state == ESPConnectState::NETWORK_DISCONNECTED) {
    _setState(ESPConnectState::NETWORK_RECONNECTING);
  }
}

void ESPConnectClass::clearConfiguration() {
  Preferences preferences;
  preferences.begin("espconnect", false);
  preferences.clear();
  preferences.end();
}

void ESPConnectClass::toJson(const JsonObject& root) const {
  root["ip_address"] = getIPAddress().toString();
  root["mac_address"] = getMACAddress();
  root["mode"] = getMode() == ESPConnectMode::AP ? "AP" : (getMode() == ESPConnectMode::STA ? "STA" : (getMode() == ESPConnectMode::ETH ? "ETH" : "NONE"));
  root["state"] = getStateName();
  root["wifi_bssid"] = getWiFiBSSID();
  root["wifi_rssi"] = getWiFiRSSI();
  root["wifi_signal"] = getWiFiSignalQuality();
  root["wifi_ssid"] = getWiFiSSID();
}

void ESPConnectClass::_setState(ESPConnectState state) {
  if (_state == state)
    return;

  ESP_LOGD(TAG, "State: %s => %s", getStateName(_state), getStateName(state));

  // be sure to save anything before auto restart and callback
  if (_autoSave && state == ESPConnectState::PORTAL_COMPLETE) {
    Preferences preferences;
    preferences.begin("espconnect", false);
    preferences.putBool("ap", _config.apMode);
    if (!_config.apMode) {
      preferences.putString("ssid", _config.wifiSSID);
      preferences.putString("password", _config.wifiPassword);
    }
    preferences.end();
  }

  const ESPConnectState previous = _state;
  _state = state;

  // make sure callback is called before auto restart
  if (_callback != nullptr)
    _callback(previous, _state);

  if (_autoRestart && (_state == ESPConnectState::PORTAL_COMPLETE || _state == ESPConnectState::PORTAL_TIMEOUT)) {
    ESP.restart();
  }
}

void ESPConnectClass::_startMDNS() {
  MDNS.begin(_hostname.c_str());
}

void ESPConnectClass::_startEthernet() {
  _setState(ESPConnectState::NETWORK_CONNECTING);

  pinMode(ETH_PHY_POWER, OUTPUT);

#ifdef ESPCONNECT_ETH_RESET_ON_START
  digitalWrite(ETH_PHY_POWER, LOW);
  delay(350);
#endif

  digitalWrite(ETH_PHY_POWER, HIGH);
  ETH.setHostname(_hostname.c_str());
  ETH.begin();

  _lastTime = esp_timer_get_time();
}

void ESPConnectClass::_startSTA() {
  _setState(ESPConnectState::NETWORK_CONNECTING);

  WiFi.setHostname(_hostname.c_str());
  WiFi.setSleep(false);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(_config.wifiSSID, _config.wifiPassword);

  _lastTime = esp_timer_get_time();
}

void ESPConnectClass::_startAP(bool captivePortal) {
  _setState(captivePortal ? ESPConnectState::PORTAL_STARTING : ESPConnectState::AP_STARTING);

  WiFi.setHostname(_hostname.c_str());
  WiFi.setSleep(false);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.mode(captivePortal ? WIFI_AP_STA : WIFI_AP);

  if (_apPassword.isEmpty() || _apPassword.length() < 8) {
    // Disabling invalid Access Point password which must be at least 8 characters long when set
    WiFi.softAP(_apSSID, emptyString);
  } else
    WiFi.softAP(_apSSID, _apPassword);

  _startMDNS();

  if (_dnsServer == nullptr) {
    _dnsServer = new DNSServer();
    _dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    _dnsServer->start(53, "*", WiFi.softAPIP());
  }

  if (captivePortal) {
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
    WiFi.scanNetworks(true);

    _httpd->begin();

    _httpd->onNotFound([](AsyncWebServerRequest* request) {
      AsyncWebServerResponse* response = request->beginResponse_P(200, "text/html", ESPCONNECT_HTML, sizeof(ESPCONNECT_HTML));
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
    });

    MDNS.addService("http", "tcp", 80);

    _lastTime = esp_timer_get_time();
  }
}

void ESPConnectClass::_stopAP() {
  _lastTime = -1;
  WiFi.softAPdisconnect(true);
  mdns_service_remove("_http", "_tcp");
  if (_dnsServer != nullptr) {
    _dnsServer->stop();
    delete _dnsServer;
    _dnsServer = nullptr;
  }
}

void ESPConnectClass::_onWiFiEvent(WiFiEvent_t event) {
  if (_state == ESPConnectState::NETWORK_DISABLED)
    return;

  switch (event) {
    case ARDUINO_EVENT_ETH_GOT_IP:
      if (_state == ESPConnectState::NETWORK_CONNECTING || _state == ESPConnectState::NETWORK_RECONNECTING || _state == ESPConnectState::PORTAL_STARTING || _state == ESPConnectState::PORTAL_STARTED) {
        ESP_LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_ETH_GOT_IP", getStateName());
        if (_state == ESPConnectState::PORTAL_STARTING || _state == ESPConnectState::PORTAL_STARTED) {
          _stopAP();
        }
        _lastTime = -1;
        _startMDNS();
        _setState(ESPConnectState::NETWORK_CONNECTED);
      }
      break;

    case ARDUINO_EVENT_ETH_DISCONNECTED:
      if (_state == ESPConnectState::NETWORK_CONNECTED && WiFi.localIP()[0] == 0) {
        ESP_LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_ETH_DISCONNECTED", getStateName());
        _setState(ESPConnectState::NETWORK_DISCONNECTED);
      }
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      if (_state == ESPConnectState::NETWORK_CONNECTING || _state == ESPConnectState::NETWORK_RECONNECTING) {
        ESP_LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_WIFI_STA_GOT_IP", getStateName());
        _lastTime = -1;
        _startMDNS();
        _setState(ESPConnectState::NETWORK_CONNECTED);
      }
      break;

    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      if (_state == ESPConnectState::NETWORK_CONNECTED && ETH.localIP()[0] == 0) {
        ESP_LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_WIFI_STA_LOST_IP", getStateName());
        _setState(ESPConnectState::NETWORK_DISCONNECTED);
      }
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      if (_state == ESPConnectState::NETWORK_CONNECTED && ETH.localIP()[0] == 0) {
        ESP_LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_WIFI_STA_DISCONNECTED", getStateName());
        _setState(ESPConnectState::NETWORK_DISCONNECTED);
      }
      break;

    case ARDUINO_EVENT_WIFI_AP_START:
      if (_state == ESPConnectState::AP_STARTING) {
        ESP_LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_WIFI_AP_START", getStateName());
        _setState(ESPConnectState::AP_STARTED);
      } else if (_state == ESPConnectState::PORTAL_STARTING) {
        ESP_LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_WIFI_AP_START", getStateName());
        _setState(ESPConnectState::PORTAL_STARTED);
      }
      break;

    default:
      break;
  }
}

bool ESPConnectClass::_durationPassed(uint32_t intervalSec) {
  if (_lastTime >= 0) {
    const int64_t now = esp_timer_get_time();
    if (now - _lastTime >= (int64_t)intervalSec * (int64_t)1000000) {
      _lastTime = now;
      return true;
    }
  }
  return false;
}

ESPConnectClass ESPConnect;
