// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2023 Mathieu Carbou and others
 */
#include "MycilaESPConnect.h"

#include <AsyncJson.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <functional>

#if defined(ESPCONNECT_ETH_SUPPORT)

#if !defined(ESPCONNECT_ETH_ALT_IMPL) && (defined(ESPCONNECT_ETH_CS) || defined(ESPCONNECT_ETH_INT) || defined(ESPCONNECT_ETH_MISO) || defined(ESPCONNECT_ETH_MOSI) || defined(ESPCONNECT_ETH_RST) || defined(ESPCONNECT_ETH_SCLK))
#define ESPCONNECT_ETH_ALT_IMPL 1
#endif

#ifdef ESPCONNECT_ETH_ALT_IMPL
#include <ETHClass.h>
#else
#include <ETH.h>
#endif

#endif // ESPCONNECT_ETH_SUPPORT

#include <espconnect_webpage.h>

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
#ifdef ESPCONNECT_ETH_SUPPORT
      if (ETH.linkUp() && ETH.localIP()[0] != 0)
        return ESPConnectMode::ETH;
#endif
      if (WiFi.localIP()[0] != 0)
        return ESPConnectMode::STA;
      return ESPConnectMode::NONE;
    default:
      return ESPConnectMode::NONE;
  }
}

const String ESPConnectClass::getMACAddress(ESPConnectMode mode) const {
  switch (mode) {
    case ESPConnectMode::AP:
    case ESPConnectMode::STA:
      return WiFi.macAddress();
#ifdef ESPCONNECT_ETH_SUPPORT
    case ESPConnectMode::ETH:
      return ETH.linkUp() ? ETH.macAddress() : emptyString;
#endif
    default:
      return emptyString;
  }
}

const IPAddress ESPConnectClass::getIPAddress(ESPConnectMode mode) const {
  const wifi_mode_t wifiMode = WiFi.getMode();
  switch (mode) {
    case ESPConnectMode::AP:
      return wifiMode == WIFI_MODE_AP || wifiMode == WIFI_MODE_APSTA ? WiFi.softAPIP() : IPAddress();
    case ESPConnectMode::STA:
      return wifiMode == WIFI_MODE_STA ? WiFi.localIP() : IPAddress();
#ifdef ESPCONNECT_ETH_SUPPORT
    case ESPConnectMode::ETH:
      return ETH.linkUp() ? ETH.localIP() : IPAddress();
#endif
    default:
      return IPAddress();
  }
}

const String ESPConnectClass::getWiFiSSID() const {
  switch (WiFi.getMode()) {
    case WIFI_MODE_AP:
    case WIFI_MODE_APSTA:
      return WiFi.softAPSSID();
    case WIFI_MODE_STA:
      return WiFi.SSID();
    default:
      return emptyString;
  }
}

const String ESPConnectClass::getWiFiBSSID() const {
  switch (WiFi.getMode()) {
    case WIFI_MODE_AP:
    case WIFI_MODE_APSTA:
      return WiFi.softAPmacAddress();
    case WIFI_MODE_STA:
      return WiFi.BSSIDstr();
    default:
      return emptyString;
  }
}

int8_t ESPConnectClass::getWiFiRSSI() const {
  return WiFi.getMode() == WIFI_MODE_STA ? WiFi.RSSI() : 0;
}

int8_t ESPConnectClass::getWiFiSignalQuality() const {
  return WiFi.getMode() == WIFI_MODE_STA ? _wifiSignalQuality(WiFi.RSSI()) : 0;
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

  _state = ESPConnectState::NETWORK_ENABLED;

  // blocks like the old behaviour
  if (_blocking) {
    ESP_LOGI(TAG, "Starting ESPConnect in blocking mode...");
    while (_state != ESPConnectState::AP_STARTED && _state != ESPConnectState::NETWORK_CONNECTED) {
      loop();
      delay(100);
    }
  } else {
    ESP_LOGI(TAG, "Starting ESPConnect in non-blocking mode...");
  }
}

void ESPConnectClass::end() {
  if (_state == ESPConnectState::NETWORK_DISABLED)
    return;
  ESP_LOGI(TAG, "Stopping ESPConnect...");
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
  if (_state == ESPConnectState::NETWORK_ENABLED && _config.apMode) {
    _startAP();
  }

  // start captive portal when network enabled but not in ap mode and no wifi info and no ethernet
  // portal wil be interrupted when network connected
#ifndef ESPCONNECT_ETH_SUPPORT
  if (_state == ESPConnectState::NETWORK_ENABLED && _config.wifiSSID.isEmpty()) {
    _startAP();
  }
#endif

  // otherwise, tries to connect to WiFi or ethernet
  if (_state == ESPConnectState::NETWORK_ENABLED) {
#ifdef ESPCONNECT_ETH_SUPPORT
    _startEthernet();
#endif
    if (!_config.wifiSSID.isEmpty())
      _startSTA();
  }

  // connection to WiFi or Ethernet times out ?
  if (_state == ESPConnectState::NETWORK_CONNECTING && _durationPassed(_connectTimeout)) {
    WiFi.disconnect(true, true);
    _setState(ESPConnectState::NETWORK_TIMEOUT);
  }

  // start captive portal on connect timeout
  if (_state == ESPConnectState::NETWORK_TIMEOUT) {
    _startAP();
  }

  // portal duration ends ?
  if (_state == ESPConnectState::PORTAL_STARTED && _durationPassed(_portalTimeout)) {
    _setState(ESPConnectState::PORTAL_TIMEOUT);
  }

  // disconnect from network ? reconnect!
  if (_state == ESPConnectState::NETWORK_DISCONNECTED) {
    _setState(ESPConnectState::NETWORK_RECONNECTING);
  }

  if (_state == ESPConnectState::AP_STARTED || _state == ESPConnectState::NETWORK_CONNECTED) {
    _disableCaptivePortal();
  }

  if (_state == ESPConnectState::PORTAL_COMPLETE || _state == ESPConnectState::PORTAL_TIMEOUT) {
    _stopAP();
    if (_autoRestart)
      ESP.restart();
    else
      _setState(ESPConnectState::NETWORK_ENABLED);
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

  const ESPConnectState previous = _state;
  _state = state;
  ESP_LOGD(TAG, "State: %s => %s", getStateName(previous), getStateName(state));

  // be sure to save anything before auto restart and callback
  if (_autoSave && _state == ESPConnectState::PORTAL_COMPLETE) {
    Preferences preferences;
    preferences.begin("espconnect", false);
    preferences.putBool("ap", _config.apMode);
    if (!_config.apMode) {
      preferences.putString("ssid", _config.wifiSSID);
      preferences.putString("password", _config.wifiPassword);
    }
    preferences.end();
  }

  // make sure callback is called before auto restart
  if (_callback != nullptr)
    _callback(previous, state);
}

#ifdef ESPCONNECT_ETH_SUPPORT
void ESPConnectClass::_startEthernet() {
  _setState(ESPConnectState::NETWORK_CONNECTING);

#if defined(ETH_PHY_POWER) && ETH_PHY_POWER > -1
  pinMode(ETH_PHY_POWER, OUTPUT);
#ifdef ESPCONNECT_ETH_RESET_ON_START
  ESP_LOGD(TAG, "Resetting ETH_PHY_POWER Pin %d", ETH_PHY_POWER);
  digitalWrite(ETH_PHY_POWER, LOW);
  delay(350);
#endif
  ESP_LOGD(TAG, "Activating ETH_PHY_POWER Pin %d", ETH_PHY_POWER);
  digitalWrite(ETH_PHY_POWER, HIGH);
#endif

  ESP_LOGI(TAG, "Starting Ethernet...");

#ifdef ESPCONNECT_ETH_ALT_IMPL
  if (!ETH.beginSPI(ESPCONNECT_ETH_MISO, ESPCONNECT_ETH_MOSI, ESPCONNECT_ETH_SCLK, ESPCONNECT_ETH_CS, ESPCONNECT_ETH_RST, ESPCONNECT_ETH_INT)) {
    ESP_LOGE(TAG, "ETH failed to start!");
  }
#else
  if (!ETH.begin()) {
    ESP_LOGE(TAG, "ETH failed to start!");
    _setState(ESPConnectState::NETWORK_ENABLED);
  }
#endif

  _lastTime = esp_timer_get_time();

  ESP_LOGD(TAG, "ETH started.");
}
#endif

void ESPConnectClass::_startSTA() {
  _setState(ESPConnectState::NETWORK_CONNECTING);

  ESP_LOGI(TAG, "Starting WiFi...");

  WiFi.setSleep(false);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(_config.wifiSSID, _config.wifiPassword);

  _lastTime = esp_timer_get_time();

  ESP_LOGD(TAG, "WiFi started.");
}

void ESPConnectClass::_startAP() {
  _setState(_config.apMode ? ESPConnectState::AP_STARTING : ESPConnectState::PORTAL_STARTING);

  ESP_LOGI(TAG, "Starting Access Point...");

  WiFi.setSleep(false);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.mode(_config.apMode ? WIFI_AP : WIFI_AP_STA);

  if (_apPassword.isEmpty() || _apPassword.length() < 8) {
    // Disabling invalid Access Point password which must be at least 8 characters long when set
    WiFi.softAP(_apSSID, emptyString);
  } else
    WiFi.softAP(_apSSID, _apPassword);

  if (_dnsServer == nullptr) {
    _dnsServer = new DNSServer();
    _dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    _dnsServer->start(53, "*", WiFi.softAPIP());
  }

  ESP_LOGD(TAG, "Access Point started.");

  if (!_config.apMode)
    _enableCaptivePortal();
}

void ESPConnectClass::_stopAP() {
  _disableCaptivePortal();
  ESP_LOGI(TAG, "Stopping Access Point...");
  _lastTime = -1;
  WiFi.softAPdisconnect(true);
  if (_dnsServer != nullptr) {
    _dnsServer->stop();
    delete _dnsServer;
    _dnsServer = nullptr;
  }
  ESP_LOGD(TAG, "Access Point stopped.");
}

void ESPConnectClass::_enableCaptivePortal() {
  ESP_LOGI(TAG, "Enable Captive Portal...");

  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  WiFi.scanNetworks(true);

  if (_scanHandler == nullptr) {
    _scanHandler = &_httpd->on("/espconnect/scan", HTTP_GET, [&](AsyncWebServerRequest* request) {
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
  }

  if (_connectHandler == nullptr) {
    _connectHandler = &_httpd->on("/espconnect/connect", HTTP_POST, [&](AsyncWebServerRequest* request) {
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
  }

  if (_homeHandler == nullptr) {
    _homeHandler = &_httpd->on("/espconnect", HTTP_GET, [](AsyncWebServerRequest* request) {
      AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", ESPCONNECT_HTML, sizeof(ESPCONNECT_HTML));
      response->addHeader("Content-Encoding", "gzip");
      request->send(response); });
  }

  if (_rewriteHandler == nullptr) {
    // this filter makes sure that the root path is only rewritten when captive portal is started
    _rewriteHandler = &_httpd->rewrite("/", "/espconnect").setFilter([&](AsyncWebServerRequest* request) {
      return _state == ESPConnectState::PORTAL_STARTED;
    });
  }

  _httpd->onNotFound([](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse_P(200, "text/html", ESPCONNECT_HTML, sizeof(ESPCONNECT_HTML));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  _httpd->begin();
  MDNS.addService("http", "tcp", 80);
  _lastTime = esp_timer_get_time();
}

void ESPConnectClass::_disableCaptivePortal() {
  if (_rewriteHandler == nullptr)
    return;
  ESP_LOGI(TAG, "Disable Captive Portal...");
  mdns_service_remove("_http", "_tcp");
  _httpd->end();
  _httpd->onNotFound(nullptr);
  if (_connectHandler != nullptr) {
    _httpd->removeHandler(_connectHandler);
    _connectHandler = nullptr;
  }
  if (_scanHandler != nullptr) {
    _httpd->removeHandler(_scanHandler);
    _scanHandler = nullptr;
  }
  if (_homeHandler != nullptr) {
    _httpd->removeHandler(_homeHandler);
    _homeHandler = nullptr;
  }
  if (_rewriteHandler != nullptr) {
    _httpd->removeRewrite(_rewriteHandler);
    _rewriteHandler = nullptr;
  }
}

void ESPConnectClass::_onWiFiEvent(WiFiEvent_t event) {
  if (_state == ESPConnectState::NETWORK_DISABLED)
    return;

  switch (event) {
#ifdef ESPCONNECT_ETH_SUPPORT
    case ARDUINO_EVENT_ETH_START:
      if (ETH.linkUp()) {
        ESP_LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_ETH_START", getStateName());
        ETH.setHostname(_hostname.c_str());
      }
      break;

    case ARDUINO_EVENT_ETH_GOT_IP:
      if (_state == ESPConnectState::NETWORK_CONNECTING || _state == ESPConnectState::NETWORK_RECONNECTING || _state == ESPConnectState::PORTAL_STARTING || _state == ESPConnectState::PORTAL_STARTED) {
        ESP_LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_ETH_GOT_IP", getStateName());
        if (_state == ESPConnectState::PORTAL_STARTING || _state == ESPConnectState::PORTAL_STARTED) {
          _stopAP();
        }
        _lastTime = -1;
        MDNS.begin(_hostname.c_str());
        _setState(ESPConnectState::NETWORK_CONNECTED);
      }
      break;
#endif

    case ARDUINO_EVENT_WIFI_STA_START:
      WiFi.setHostname(_hostname.c_str());
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
        MDNS.begin(_hostname.c_str());
        _setState(ESPConnectState::NETWORK_CONNECTED);
      }
      break;

    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      if (_state == ESPConnectState::NETWORK_CONNECTED) {
#ifdef ESPCONNECT_ETH_SUPPORT
        if (ETH.linkUp() && ETH.localIP()[0] != 0)
          return;
#endif
        ESP_LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_WIFI_STA_LOST_IP", getStateName());
        _setState(ESPConnectState::NETWORK_DISCONNECTED);
      }
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      if (_state == ESPConnectState::NETWORK_CONNECTED) {
#ifdef ESPCONNECT_ETH_SUPPORT
        if (ETH.linkUp() && ETH.localIP()[0] != 0)
          return;
#endif
        ESP_LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_WIFI_STA_DISCONNECTED", getStateName());
        _setState(ESPConnectState::NETWORK_DISCONNECTED);
      }
      break;

    case ARDUINO_EVENT_WIFI_AP_START:
      WiFi.softAPsetHostname(_hostname.c_str());
      MDNS.begin(_hostname.c_str());
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
