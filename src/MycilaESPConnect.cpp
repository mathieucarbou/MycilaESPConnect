// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2023 Mathieu Carbou and others
 */
#include "MycilaESPConnect.h"

#ifdef ESP8266
  #ifndef ESPCONNECT_NO_MDNS
    #include <ESP8266mDNS.h>
  #endif
  #define wifi_mode_t                         WiFiMode_t
  #define WIFI_MODE_STA                       WIFI_STA
  #define WIFI_MODE_AP                        WIFI_AP
  #define WIFI_MODE_APSTA                     WIFI_AP_STA
  #define WIFI_MODE_NULL                      WIFI_OFF
  #define WIFI_AUTH_OPEN                      ENC_TYPE_NONE
  #define ARDUINO_EVENT_WIFI_STA_GOT_IP       WIFI_EVENT_STAMODE_GOT_IP
  #define ARDUINO_EVENT_WIFI_STA_LOST_IP      WIFI_EVENT_STAMODE_DHCP_TIMEOUT
  #define ARDUINO_EVENT_WIFI_STA_DISCONNECTED WIFI_EVENT_STAMODE_DISCONNECTED
  #define ARDUINO_EVENT_WIFI_AP_START         WIFI_EVENT_SOFTAPMODE_STACONNECTED
#else
  #ifndef ESPCONNECT_NO_MDNS
    #include <ESPmDNS.h>
  #endif
  #include <esp_mac.h>
#endif

#include <AsyncJson.h>
#include <Preferences.h>
#include <functional>

#if defined(ESPCONNECT_ETH_SUPPORT)
  #if defined(ETH_PHY_SPI_SCK) && defined(ETH_PHY_SPI_MISO) && defined(ETH_PHY_SPI_MOSI) && defined(ETH_PHY_CS) && defined(ETH_PHY_IRQ) && defined(ETH_PHY_RST)
    #define ESPCONNECT_ETH_SPI_SUPPORT 1
    #if ESP_IDF_VERSION_MAJOR >= 5
      #include <ETH.h>
      #include <SPI.h>
    #else
      #include "backport/ETHClass.h"
    #endif
  #else
    #include <ETH.h>
  #endif
#endif

#include "./espconnect_webpage.h"

#ifdef MYCILA_LOGGER_SUPPORT
  #include <MycilaLogger.h>
extern Mycila::Logger logger;
  #define LOGD(tag, format, ...) logger.debug(tag, format, ##__VA_ARGS__)
  #define LOGI(tag, format, ...) logger.info(tag, format, ##__VA_ARGS__)
  #define LOGW(tag, format, ...) logger.warn(tag, format, ##__VA_ARGS__)
  #define LOGE(tag, format, ...) logger.error(tag, format, ##__VA_ARGS__)
#elif defined(ESP8266)
  #ifdef ESPCONNECT_DEBUG
    #define LOGD(tag, format, ...)                         \
      {                                                    \
        Serial.printf("%6lu [%s] DEBUG: ", millis(), tag); \
        Serial.printf(format "\n", ##__VA_ARGS__);         \
      }
    #define LOGI(tag, format, ...)                        \
      {                                                   \
        Serial.printf("%6lu [%s] INFO: ", millis(), tag); \
        Serial.printf(format "\n", ##__VA_ARGS__);        \
      }
    #define LOGW(tag, format, ...)                        \
      {                                                   \
        Serial.printf("%6lu [%s] WARN: ", millis(), tag); \
        Serial.printf(format "\n", ##__VA_ARGS__);        \
      }
    #define LOGE(tag, format, ...)                         \
      {                                                    \
        Serial.printf("%6lu [%s] ERROR: ", millis(), tag); \
        Serial.printf(format "\n", ##__VA_ARGS__);         \
      }
  #else
    #define LOGD(tag, format, ...)
    #define LOGI(tag, format, ...)
    #define LOGW(tag, format, ...)
    #define LOGE(tag, format, ...)
  #endif
#else
  #define LOGD(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__)
  #define LOGI(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)
  #define LOGW(tag, format, ...) ESP_LOGW(tag, format, ##__VA_ARGS__)
  #define LOGE(tag, format, ...) ESP_LOGE(tag, format, ##__VA_ARGS__)
#endif

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
  String mac = emptyString;

  switch (mode) {
    case ESPConnectMode::AP:
      mac = WiFi.softAPmacAddress();
      break;
    case ESPConnectMode::STA:
      mac = WiFi.macAddress();
      break;
#ifdef ESPCONNECT_ETH_SUPPORT
    case ESPConnectMode::ETH:
      mac = ETH.linkUp() ? ETH.macAddress() : emptyString;
      break;
#endif
    default:
      mac = emptyString;
      break;
  }

  if (mac != emptyString && mac != "00:00:00:00:00:00")
    return mac;

#ifdef ESP8266
  switch (mode) {
    case ESPConnectMode::AP:
      return WiFi.softAPmacAddress();
    case ESPConnectMode::STA:
      return WiFi.macAddress();
    default:
      // ETH not supported with ESP8266
      return emptyString;
  }
#else
  // ESP_MAC_IEEE802154 is used to mean "no MAC address" in this context
  esp_mac_type_t type = esp_mac_type_t::ESP_MAC_IEEE802154;

  switch (mode) {
    case ESPConnectMode::AP:
      type = ESP_MAC_WIFI_SOFTAP;
      break;
    case ESPConnectMode::STA:
      type = ESP_MAC_WIFI_STA;
      break;
  #ifdef ESPCONNECT_ETH_SUPPORT
    case ESPConnectMode::ETH:
      type = ESP_MAC_ETH;
      break;
  #endif
    default:
      break;
  }

  if (type == esp_mac_type_t::ESP_MAC_IEEE802154)
    return emptyString;

  uint8_t bytes[6] = {0, 0, 0, 0, 0, 0};
  if (esp_read_mac(bytes, type) != ESP_OK)
    return emptyString;

  char buffer[18] = {0};
  snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X", bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]);
  return String(buffer);
#endif
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

void ESPConnectClass::begin(AsyncWebServer& httpd, const String& hostname, const String& apSSID, const String& apPassword) {
  if (_state != ESPConnectState::NETWORK_DISABLED)
    return;

  _autoSave = true;

  LOGD(TAG, "Loading config...");
  Preferences preferences;
  preferences.begin("espconnect", true);
  String ssid = preferences.isKey("ssid") ? preferences.getString("ssid", emptyString) : emptyString;
  String password = preferences.isKey("password") ? preferences.getString("password", emptyString) : emptyString;
  bool ap = preferences.isKey("ap") ? preferences.getBool("ap", false) : false;
  preferences.end();
  LOGD(TAG, " - AP: %d", ap);
  LOGD(TAG, " - SSID: %s", ssid.c_str());

  begin(httpd, hostname, apSSID, apPassword, {ssid, password, ap});
}

void ESPConnectClass::begin(AsyncWebServer& httpd, const String& hostname, const String& apSSID, const String& apPassword, const ESPConnectConfig& config) {
  if (_state != ESPConnectState::NETWORK_DISABLED)
    return;

  _httpd = &httpd;
  _hostname = hostname;
  _apSSID = apSSID;
  _apPassword = apPassword;
  _config = config; // copy values

#ifdef ESP8266
  onStationModeGotIP = WiFi.onStationModeGotIP([this](__unused const WiFiEventStationModeGotIP& event) {
    this->_onWiFiEvent(ARDUINO_EVENT_WIFI_STA_GOT_IP);
  });
  onStationModeDHCPTimeout = WiFi.onStationModeDHCPTimeout([this]() {
    this->_onWiFiEvent(ARDUINO_EVENT_WIFI_STA_LOST_IP);
  });
  onStationModeDisconnected = WiFi.onStationModeDisconnected([this](__unused const WiFiEventStationModeDisconnected& event) {
    this->_onWiFiEvent(ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  });
#else
  _wifiEventListenerId = WiFi.onEvent(std::bind(&ESPConnectClass::_onWiFiEvent, this, std::placeholders::_1));
#endif

  _state = ESPConnectState::NETWORK_ENABLED;

  // blocks like the old behaviour
  if (_blocking) {
    LOGI(TAG, "Starting ESPConnect in blocking mode...");
    while (_state != ESPConnectState::AP_STARTED && _state != ESPConnectState::NETWORK_CONNECTED) {
      loop();
      delay(100);
    }
  } else {
    LOGI(TAG, "Starting ESPConnect in non-blocking mode...");
  }
}

void ESPConnectClass::end() {
  if (_state == ESPConnectState::NETWORK_DISABLED)
    return;
  LOGI(TAG, "Stopping ESPConnect...");
  _lastTime = -1;
  _autoSave = false;
  _setState(ESPConnectState::NETWORK_DISABLED);
#ifndef ESP8266
  WiFi.removeEvent(_wifiEventListenerId);
#endif
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

  // timeout portal if we failed to connect to WiFi (we got a SSID) and portal duration is passed
  // in order to restart and try again to connect to the configured WiFi
  if (_state == ESPConnectState::PORTAL_STARTED && !_config.wifiSSID.isEmpty() && _durationPassed(_portalTimeout)) {
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
    if (_autoRestart) {
      LOGW(TAG, "Auto Restart of ESP...");
      ESP.restart();
    } else
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
  root["ip_address_ap"] = getIPAddress(ESPConnectMode::AP).toString();
  root["ip_address_eth"] = getIPAddress(ESPConnectMode::ETH).toString();
  root["ip_address_sta"] = getIPAddress(ESPConnectMode::STA).toString();
  root["mac_address"] = getMACAddress();
  root["mac_address_ap"] = getMACAddress(ESPConnectMode::AP);
  root["mac_address_eth"] = getMACAddress(ESPConnectMode::ETH);
  root["mac_address_sta"] = getMACAddress(ESPConnectMode::STA);
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
  LOGD(TAG, "State: %s => %s", getStateName(previous), getStateName(state));

  // be sure to save anything before auto restart and callback
  if (_autoSave && _state == ESPConnectState::PORTAL_COMPLETE) {
    LOGD(TAG, "Saving config...");
    LOGD(TAG, " - AP: %d", _config.apMode);
    LOGD(TAG, " - SSID: %s", _config.wifiSSID.c_str());
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
  LOGD(TAG, "Resetting ETH_PHY_POWER Pin %d", ETH_PHY_POWER);
  digitalWrite(ETH_PHY_POWER, LOW);
  delay(350);
    #endif
  LOGD(TAG, "Activating ETH_PHY_POWER Pin %d", ETH_PHY_POWER);
  digitalWrite(ETH_PHY_POWER, HIGH);
  #endif

  LOGI(TAG, "Starting Ethernet...");
  #if defined(ESPCONNECT_ETH_SPI_SUPPORT)
    #if ESP_IDF_VERSION_MAJOR >= 5
  // https://github.com/espressif/arduino-esp32/tree/master/libraries/Ethernet/examples
  SPI.begin(ETH_PHY_SPI_SCK, ETH_PHY_SPI_MISO, ETH_PHY_SPI_MOSI);
  if (!ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_CS, ETH_PHY_IRQ, ETH_PHY_RST, SPI)) {
    LOGE(TAG, "ETH failed to start!");
  }
    #else
  if (!ETH.beginSPI(ETH_PHY_SPI_MISO, ETH_PHY_SPI_MOSI, ETH_PHY_SPI_SCK, ETH_PHY_CS, ETH_PHY_RST, ETH_PHY_IRQ)) {
    LOGE(TAG, "ETH failed to start!");
  }
    #endif
  #else
  if (!ETH.begin()) {
    LOGE(TAG, "ETH failed to start!");
  }
  #endif

  _lastTime = millis();

  LOGD(TAG, "ETH started.");
}
#endif

void ESPConnectClass::_startSTA() {
  _setState(ESPConnectState::NETWORK_CONNECTING);

  LOGI(TAG, "Starting WiFi...");

  WiFi.setHostname(_hostname.c_str());
  WiFi.setSleep(false);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);

  LOGD(TAG, "Connecting to SSID: %s...", _config.wifiSSID.c_str());
  WiFi.begin(_config.wifiSSID, _config.wifiPassword);

  _lastTime = millis();

  LOGD(TAG, "WiFi started.");
}

void ESPConnectClass::_startAP() {
  _setState(_config.apMode ? ESPConnectState::AP_STARTING : ESPConnectState::PORTAL_STARTING);

  LOGI(TAG, "Starting Access Point...");

  WiFi.setHostname(_hostname.c_str());
#ifndef ESP8266
  WiFi.softAPsetHostname(_hostname.c_str());
#endif
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

  LOGD(TAG, "Access Point started.");

#ifdef ESP8266
  _onWiFiEvent(ARDUINO_EVENT_WIFI_AP_START);
#endif

  if (!_config.apMode)
    _enableCaptivePortal();
}

void ESPConnectClass::_stopAP() {
  _disableCaptivePortal();
  LOGI(TAG, "Stopping Access Point...");
  _lastTime = -1;
  WiFi.softAPdisconnect(true);
  if (_dnsServer != nullptr) {
    _dnsServer->stop();
    delete _dnsServer;
    _dnsServer = nullptr;
  }
  LOGD(TAG, "Access Point stopped.");
}

void ESPConnectClass::_enableCaptivePortal() {
  LOGI(TAG, "Enable Captive Portal...");

#ifndef ESP8266
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
#endif
  WiFi.scanNetworks(true);
  _scanStart = millis();

  if (_scanHandler == nullptr) {
    _scanHandler = &_httpd->on("/espconnect/scan", HTTP_GET, [&](AsyncWebServerRequest* request) {
      int n = WiFi.scanComplete();

      // scan still running ? wait...
      if (n == WIFI_SCAN_RUNNING) {
        return request->send(202);

        // scan error or finished with no result ?
      } else if (n == 0 || n == WIFI_SCAN_FAILED) {
        // timeout ?
        const bool timedOut = millis() - _scanStart > _scanTimeout;

        // re-scan
        WiFi.scanDelete();
        WiFi.scanNetworks(true);
        _scanStart = millis();

        // send empty json response, to let the user choose AP mode if timeout, or still ask client to wait
        if (timedOut) {
          AsyncJsonResponse* response = new AsyncJsonResponse(true);
          response->setLength();
          request->send(response);
        } else {
          return request->send(202);
        }

        // scan results ?
      } else {
        AsyncJsonResponse* response = new AsyncJsonResponse(true);
        JsonArray json = response->getRoot();
        // we have some results
        for (int i = 0; i < n; ++i) {
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
        // clean up and start scanning again in background
        WiFi.scanDelete();
        WiFi.scanNetworks(true);
        _scanStart = millis();
        response->setLength();
        request->send(response);
      }
    });
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
    _rewriteHandler = &_httpd->rewrite("/", "/espconnect").setFilter([&](__unused AsyncWebServerRequest* request) {
      return _state == ESPConnectState::PORTAL_STARTED;
    });
  }

  _httpd->onNotFound([](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse_P(200, "text/html", ESPCONNECT_HTML, sizeof(ESPCONNECT_HTML));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  _httpd->begin();
#ifndef ESPCONNECT_NO_MDNS
  MDNS.addService("http", "tcp", 80);
#endif
  _lastTime = millis();
}

void ESPConnectClass::_disableCaptivePortal() {
  if (_rewriteHandler == nullptr)
    return;
  LOGI(TAG, "Disable Captive Portal...");
#ifndef ESPCONNECT_NO_MDNS
  #ifndef ESP8266
  mdns_service_remove("_http", "_tcp");
  #endif
#endif
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
        LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_ETH_START", getStateName());
        ETH.setHostname(_hostname.c_str());
      }
      break;

    case ARDUINO_EVENT_ETH_GOT_IP:
      if (_state == ESPConnectState::NETWORK_CONNECTING || _state == ESPConnectState::NETWORK_RECONNECTING || _state == ESPConnectState::PORTAL_STARTING || _state == ESPConnectState::PORTAL_STARTED) {
        LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_ETH_GOT_IP", getStateName());
        if (_state == ESPConnectState::PORTAL_STARTING || _state == ESPConnectState::PORTAL_STARTED) {
          _stopAP();
        }
        _lastTime = -1;
  #ifndef ESPCONNECT_NO_MDNS
        MDNS.begin(_hostname.c_str());
  #endif
        _setState(ESPConnectState::NETWORK_CONNECTED);
      }
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      if (_state == ESPConnectState::NETWORK_CONNECTED && WiFi.localIP()[0] == 0) {
        LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_ETH_DISCONNECTED", getStateName());
        _setState(ESPConnectState::NETWORK_DISCONNECTED);
      }
      break;
#endif

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      if (_state == ESPConnectState::NETWORK_CONNECTING || _state == ESPConnectState::NETWORK_RECONNECTING) {
        LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_WIFI_STA_GOT_IP", getStateName());
        _lastTime = -1;
#ifndef ESPCONNECT_NO_MDNS
        MDNS.begin(_hostname.c_str());
#endif
        _setState(ESPConnectState::NETWORK_CONNECTED);
      }
      break;

    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_WIFI_STA_DISCONNECTED", getStateName());
        WiFi.reconnect();
      } else {
        LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_WIFI_STA_LOST_IP", getStateName());
      }
      if (_state == ESPConnectState::NETWORK_CONNECTED) {
        // we have to move to state disconnected only if we are not connected to ethernet
#ifdef ESPCONNECT_ETH_SUPPORT
        if (ETH.linkUp() && ETH.localIP()[0] != 0)
          return;
#endif
        _setState(ESPConnectState::NETWORK_DISCONNECTED);
      }
      break;

    case ARDUINO_EVENT_WIFI_AP_START:
#ifndef ESPCONNECT_NO_MDNS
      MDNS.begin(_hostname.c_str());
#endif
      if (_state == ESPConnectState::AP_STARTING) {
        LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_WIFI_AP_START", getStateName());
        _setState(ESPConnectState::AP_STARTED);
      } else if (_state == ESPConnectState::PORTAL_STARTING) {
        LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_WIFI_AP_START", getStateName());
        _setState(ESPConnectState::PORTAL_STARTED);
      }
      break;

    default:
      break;
  }
}

bool ESPConnectClass::_durationPassed(uint32_t intervalSec) {
  if (_lastTime >= 0 && millis() - (uint32_t)_lastTime >= intervalSec * 1000) {
    _lastTime = -1;
    return true;
  }
  return false;
}

ESPConnectClass ESPConnect;
