// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2025 Mathieu Carbou
 */
#include "MycilaESPConnect.h"

#include <cstdio>

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

#include <Preferences.h>
#include <functional>

#if defined(ESPCONNECT_ETH_SUPPORT)
  #if defined(ETH_PHY_SPI_SCK) && defined(ETH_PHY_SPI_MISO) && defined(ETH_PHY_SPI_MOSI) && defined(ETH_PHY_CS) && defined(ETH_PHY_IRQ) && defined(ETH_PHY_RST)
    #define ESPCONNECT_ETH_SPI_SUPPORT 1
    #include <ETH.h>
    #include <SPI.h>
  #else
    #include <ETH.h>
  #endif
#endif

#ifndef ESPCONNECT_NO_CAPTIVE_PORTAL
  #include "./espconnect_webpage.h"
#endif

#ifdef ESPCONNECT_NO_LOGGING
  #define LOGD(tag, format, ...)
  #define LOGI(tag, format, ...)
  #define LOGW(tag, format, ...)
  #define LOGE(tag, format, ...)
#elif defined(MYCILA_LOGGER_SUPPORT)
  #include <MycilaLogger.h>
extern Mycila::Logger logger;
  #define LOGD(tag, format, ...) logger.debug(tag, format, ##__VA_ARGS__)
  #define LOGI(tag, format, ...) logger.info(tag, format, ##__VA_ARGS__)
  #define LOGW(tag, format, ...) logger.warn(tag, format, ##__VA_ARGS__)
  #define LOGE(tag, format, ...) logger.error(tag, format, ##__VA_ARGS__)
#elif defined(ESP8266)
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

const char* Mycila::ESPConnect::getStateName() const {
  return NetworkStateNames[static_cast<int>(_state)];
}

const char* Mycila::ESPConnect::getStateName(Mycila::ESPConnect::State state) const {
  return NetworkStateNames[static_cast<int>(state)];
}

Mycila::ESPConnect::Mode Mycila::ESPConnect::getMode() const {
  switch (_state) {
    case Mycila::ESPConnect::State::AP_STARTED:
    case Mycila::ESPConnect::State::PORTAL_STARTED:
      return Mycila::ESPConnect::Mode::AP;
      break;
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

void Mycila::ESPConnect::begin(const char* hostname, const char* apSSID, const char* apPassword) {
  if (_state != Mycila::ESPConnect::State::NETWORK_DISABLED)
    return;

  _autoSave = true;
  Config config;
  loadConfiguration(config);
  config.hostname = hostname == nullptr ? "" : hostname;
  begin(apSSID, apPassword, config);
}

void Mycila::ESPConnect::begin(const char* apSSID, const char* apPassword, const Mycila::ESPConnect::Config& config) {
  if (_state != Mycila::ESPConnect::State::NETWORK_DISABLED)
    return;

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
  _wifiEventListenerId = WiFi.onEvent(std::bind(&ESPConnect::_onWiFiEvent, this, std::placeholders::_1));
#endif

  _state = Mycila::ESPConnect::State::NETWORK_ENABLED;

  // blocks like the old behaviour
  if (_blocking) {
    LOGI(TAG, "Starting ESPConnect in blocking mode...");
    while (_state != Mycila::ESPConnect::State::AP_STARTED && _state != Mycila::ESPConnect::State::NETWORK_CONNECTED) {
      loop();
      delay(100);
    }
  } else {
    LOGI(TAG, "Starting ESPConnect in non-blocking mode...");
  }
}

void Mycila::ESPConnect::end() {
  if (_state == Mycila::ESPConnect::State::NETWORK_DISABLED)
    return;
  LOGI(TAG, "Stopping ESPConnect...");
  _lastTime = -1;
  _autoSave = false;
  _setState(Mycila::ESPConnect::State::NETWORK_DISABLED);
#ifndef ESP8266
  WiFi.removeEvent(_wifiEventListenerId);
#endif
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_MODE_NULL);
  _stopAP();
#ifndef ESPCONNECT_NO_CAPTIVE_PORTAL
  _httpd = nullptr;
#endif
}

void Mycila::ESPConnect::loop() {
  if (_dnsServer != nullptr)
    _dnsServer->processNextRequest();

  // first check if we have to enter AP mode
  if (_state == Mycila::ESPConnect::State::NETWORK_ENABLED && _config.apMode) {
    _startAP();
  }

  // start captive portal when network enabled but not in ap mode and no wifi info and no ethernet
  // portal wil be interrupted when network connected
#ifndef ESPCONNECT_ETH_SUPPORT
  if (_state == Mycila::ESPConnect::State::NETWORK_ENABLED && !_config.wifiSSID.length()) {
    _startAP();
  }
#endif

  // otherwise, tries to connect to WiFi or ethernet
  if (_state == Mycila::ESPConnect::State::NETWORK_ENABLED) {
#ifdef ESPCONNECT_ETH_SUPPORT
    _startEthernet();
#endif
    if (_config.wifiSSID.length())
      _startSTA();
  }

  // connection to WiFi or Ethernet times out ?
  if (_state == Mycila::ESPConnect::State::NETWORK_CONNECTING && _durationPassed(_connectTimeout)) {
    if (WiFi.getMode() != WIFI_MODE_NULL) {
      WiFi.config(static_cast<uint32_t>(0x00000000), static_cast<uint32_t>(0x00000000), static_cast<uint32_t>(0x00000000), static_cast<uint32_t>(0x00000000));
      WiFi.disconnect(true, true);
    }
    _setState(Mycila::ESPConnect::State::NETWORK_TIMEOUT);
  }

  // start captive portal on connect timeout
  if (_state == Mycila::ESPConnect::State::NETWORK_TIMEOUT) {
    _startAP();
  }

  // timeout portal if we failed to connect to WiFi (we got a SSID) and portal duration is passed
  // in order to restart and try again to connect to the configured WiFi
  if (_state == Mycila::ESPConnect::State::PORTAL_STARTED && _config.wifiSSID.length() && _durationPassed(_portalTimeout)) {
    _setState(Mycila::ESPConnect::State::PORTAL_TIMEOUT);
  }

  // disconnect from network ? reconnect!
  if (_state == Mycila::ESPConnect::State::NETWORK_DISCONNECTED) {
    _setState(Mycila::ESPConnect::State::NETWORK_RECONNECTING);
  }

  if (_state == Mycila::ESPConnect::State::AP_STARTED || _state == Mycila::ESPConnect::State::NETWORK_CONNECTED) {
    _disableCaptivePortal();
  }

  if (_state == Mycila::ESPConnect::State::PORTAL_COMPLETE || _state == Mycila::ESPConnect::State::PORTAL_TIMEOUT) {
    _stopAP();
    if (_autoRestart) {
      LOGW(TAG, "Auto Restart of ESP...");
      ESP.restart();
    } else
      _setState(Mycila::ESPConnect::State::NETWORK_ENABLED);
  }
}

void Mycila::ESPConnect::loadConfiguration(Mycila::ESPConnect::Config& config) {
  LOGD(TAG, "Loading config...");
  Preferences preferences;
  preferences.begin("espconnect", true);
  config.apMode = preferences.isKey("ap") ? preferences.getBool("ap", false) : false;
  if (preferences.isKey("ssid"))
    config.wifiSSID = preferences.getString("ssid").c_str();
  if (preferences.isKey("password"))
    config.wifiPassword = preferences.getString("password").c_str();
  if (preferences.isKey("ip"))
    config.ipConfig.ip.fromString(preferences.getString("ip"));
  if (preferences.isKey("subnet"))
    config.ipConfig.subnet.fromString(preferences.getString("subnet"));
  if (preferences.isKey("gateway"))
    config.ipConfig.gateway.fromString(preferences.getString("gateway"));
  if (preferences.isKey("dns"))
    config.ipConfig.dns.fromString(preferences.getString("dns"));
  if (preferences.isKey("hostname"))
    config.hostname = preferences.getString("hostname").c_str();
  preferences.end();
  LOGD(TAG, " - AP: %d", config.apMode);
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
  LOGD(TAG, " - SSID: %s", config.wifiSSID.c_str());
  LOGD(TAG, " - IP: %s", config.ipConfig.ip.toString().c_str());
  LOGD(TAG, " - Subnet: %s", config.ipConfig.subnet.toString().c_str());
  LOGD(TAG, " - Gateway: %s", config.ipConfig.gateway.toString().c_str());
  LOGD(TAG, " - DNS: %s", config.ipConfig.dns.toString().c_str());
  LOGD(TAG, " - Hostname: %s", config.hostname.c_str());
  Preferences preferences;
  preferences.begin("espconnect", false);
  preferences.putBool("ap", config.apMode);
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

#ifndef ESPCONNECT_NO_CAPTIVE_PORTAL
void Mycila::ESPConnect::toJson(const JsonObject& root) const {
  root["ip_address"] = getIPAddress().toString();
  root["ip_address_ap"] = getIPAddress(Mycila::ESPConnect::Mode::AP).toString();
  root["ip_address_eth"] = getIPAddress(Mycila::ESPConnect::Mode::ETH).toString();
  root["ip_address_sta"] = getIPAddress(Mycila::ESPConnect::Mode::STA).toString();
  root["hostname"] = _config.hostname.c_str();
  root["mac_address"] = getMACAddress();
  root["mac_address_ap"] = getMACAddress(Mycila::ESPConnect::Mode::AP);
  root["mac_address_eth"] = getMACAddress(Mycila::ESPConnect::Mode::ETH);
  root["mac_address_sta"] = getMACAddress(Mycila::ESPConnect::Mode::STA);
  root["mode"] = getMode() == Mycila::ESPConnect::Mode::AP ? "AP" : (getMode() == Mycila::ESPConnect::Mode::STA ? "STA" : (getMode() == Mycila::ESPConnect::Mode::ETH ? "ETH" : "NONE"));
  root["state"] = getStateName();
  root["wifi_bssid"] = getWiFiBSSID();
  root["wifi_rssi"] = getWiFiRSSI();
  root["wifi_signal"] = getWiFiSignalQuality();
  root["wifi_ssid"] = getWiFiSSID();
}
#endif

void Mycila::ESPConnect::_setState(Mycila::ESPConnect::State state) {
  if (_state == state)
    return;

  const Mycila::ESPConnect::State previous = _state;
  _state = state;
  LOGD(TAG, "State: %s => %s", getStateName(previous), getStateName(state));

  // be sure to save anything before auto restart and callback
  if (_autoSave && _state == Mycila::ESPConnect::State::PORTAL_COMPLETE) {
    saveConfiguration();
  }

  // make sure callback is called before auto restart
  if (_callback != nullptr)
    _callback(previous, state);
}

#ifdef ESPCONNECT_ETH_SUPPORT
void Mycila::ESPConnect::_startEthernet() {
  _setState(Mycila::ESPConnect::State::NETWORK_CONNECTING);

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
  bool success = true;

  #if defined(ESPCONNECT_ETH_SPI_SUPPORT)
  // https://github.com/espressif/arduino-esp32/tree/master/libraries/Ethernet/examples
  SPI.begin(ETH_PHY_SPI_SCK, ETH_PHY_SPI_MISO, ETH_PHY_SPI_MOSI);
  success = ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_CS, ETH_PHY_IRQ, ETH_PHY_RST, SPI);
  #else
  success = ETH.begin();
  #endif

  if (success) {
    LOGI(TAG, "Ethernet started.");
    if (_config.ipConfig.ip) {
      LOGI(TAG, "Set Ethernet Static IP Configuration:");
      LOGI(TAG, " - IP: %s", _config.ipConfig.ip.toString().c_str());
      LOGI(TAG, " - Gateway: %s", _config.ipConfig.gateway.toString().c_str());
      LOGI(TAG, " - Subnet: %s", _config.ipConfig.subnet.toString().c_str());
      LOGI(TAG, " - DNS: %s", _config.ipConfig.dns.toString().c_str());

      ETH.config(_config.ipConfig.ip, _config.ipConfig.gateway, _config.ipConfig.subnet, _config.ipConfig.dns);
    }
  } else {
    LOGE(TAG, "ETH failed to start!");
  }

  _lastTime = millis();
}
#endif

void Mycila::ESPConnect::_startSTA() {
  _setState(Mycila::ESPConnect::State::NETWORK_CONNECTING);

  LOGI(TAG, "Starting WiFi...");

#ifndef ESP8266
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
#endif

  WiFi.setHostname(_config.hostname.c_str());
  WiFi.setSleep(false);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);

  WiFi.mode(WIFI_STA);

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

  LOGD(TAG, "Connecting to SSID: %s...", _config.wifiSSID.c_str());
  WiFi.begin(_config.wifiSSID.c_str(), _config.wifiPassword.c_str());

  _lastTime = millis();

  LOGD(TAG, "WiFi started.");
}

void Mycila::ESPConnect::_startAP() {
  _setState(_config.apMode ? Mycila::ESPConnect::State::AP_STARTING : Mycila::ESPConnect::State::PORTAL_STARTING);

  LOGI(TAG, "Starting Access Point...");

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

  WiFi.mode(_config.apMode ? WIFI_AP : WIFI_AP_STA);

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

  LOGD(TAG, "Access Point started.");

#ifdef ESP8266
  _onWiFiEvent(ARDUINO_EVENT_WIFI_AP_START);
#endif

  if (!_config.apMode)
    _enableCaptivePortal();
}

void Mycila::ESPConnect::_stopAP() {
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

void Mycila::ESPConnect::_enableCaptivePortal() {
#ifdef ESPCONNECT_NO_CAPTIVE_PORTAL
  LOGE(TAG, "Captive Portal was disabled with ESPCONNECT_NO_CAPTIVE_PORTAL: you must provide a valid network configuration.");
#else
  LOGI(TAG, "Enable Captive Portal...");
  _scan();

  if (_scanHandler == nullptr) {
    _scanHandler = &_httpd->on("/espconnect/scan", HTTP_GET, [&](AsyncWebServerRequest* request) {
      int n = WiFi.scanComplete();

      if (n == WIFI_SCAN_RUNNING) {
        // scan still running ? wait...
        request->send(202);

      } else if (n == WIFI_SCAN_FAILED) {
        // scan error or finished with no result ?
        // re-scan
        _scan();
        request->send(202);

      } else {
        // scan results ?
        AsyncJsonResponse* response = new AsyncJsonResponse(true);
        JsonArray json = response->getRoot();

        // we have some results
        for (int i = 0; i < n; ++i) {
          JsonObject entry = json.add<JsonObject>();
          entry["name"] = WiFi.SSID(i);
          entry["rssi"] = WiFi.RSSI(i);
          entry["signal"] = _wifiSignalQuality(WiFi.RSSI(i));
          entry["open"] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
        }

        WiFi.scanDelete();
        response->setLength();
        request->send(response);
      }
    });
  }

  if (_connectHandler == nullptr) {
    _connectHandler = &_httpd->on("/espconnect/connect", HTTP_POST, [&](AsyncWebServerRequest* request) {
      _config.apMode = request->hasParam("ap_mode", true) && request->getParam("ap_mode", true)->value() == "true";
      if (_config.apMode) {
        request->send(200, "application/json", "{\"message\":\"Configuration Saved.\"}");
        _setState(Mycila::ESPConnect::State::PORTAL_COMPLETE);
      } else {
        ESPCONNECT_STRING ssid;
        ESPCONNECT_STRING password;
        if (request->hasParam("ssid", true))
          ssid = request->getParam("ssid", true)->value().c_str();
        if (request->hasParam("password", true))
          password = request->getParam("password", true)->value().c_str();
        if (!ssid.length())
          return request->send(400, "application/json", "{\"message\":\"Invalid SSID\"}");
        if (ssid.length() > 32 || password.length() > 64 || (password.length() && password.length() < 8))
          return request->send(400, "application/json", "{\"message\":\"Credentials exceed character limit of 32 & 64 respectively, or password lower than 8 characters.\"}");
        _config.wifiSSID = ssid;
        _config.wifiPassword = password;
        request->send(200, "application/json", "{\"message\":\"Configuration Saved.\"}");
        _setState(Mycila::ESPConnect::State::PORTAL_COMPLETE);
      }
    });
  }

  if (_homeHandler == nullptr) {
    _homeHandler = &_httpd->on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
      AsyncWebServerResponse* response = request->beginResponse(200, "text/html", ESPCONNECT_HTML, sizeof(ESPCONNECT_HTML));
      response->addHeader("Content-Encoding", "gzip");
      return request->send(response);
    });
    _homeHandler->setFilter([&](__unused AsyncWebServerRequest* request) {
      return _state == Mycila::ESPConnect::State::PORTAL_STARTED;
    });
  }

  _httpd->onNotFound([](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse(200, "text/html", ESPCONNECT_HTML, sizeof(ESPCONNECT_HTML));
    response->addHeader("Content-Encoding", "gzip");
    return request->send(response);
  });

  _httpd->begin();
  #ifndef ESPCONNECT_NO_MDNS
  MDNS.addService("http", "tcp", 80);
  #endif
#endif

  _lastTime = millis();
}

void Mycila::ESPConnect::_disableCaptivePortal() {
#ifndef ESPCONNECT_NO_CAPTIVE_PORTAL
  if (_homeHandler == nullptr)
    return;

  LOGI(TAG, "Disable Captive Portal...");

  WiFi.scanDelete();

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
#endif
}

void Mycila::ESPConnect::_onWiFiEvent(WiFiEvent_t event) {
  if (_state == Mycila::ESPConnect::State::NETWORK_DISABLED)
    return;

  switch (event) {
#ifdef ESPCONNECT_ETH_SUPPORT
    case ARDUINO_EVENT_ETH_START:
      if (ETH.linkUp()) {
        LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_ETH_START", getStateName());
        ETH.setHostname(_config.hostname.c_str());
      }
      break;

    case ARDUINO_EVENT_ETH_GOT_IP:
      if (_state == Mycila::ESPConnect::State::NETWORK_CONNECTING || _state == Mycila::ESPConnect::State::NETWORK_RECONNECTING || _state == Mycila::ESPConnect::State::PORTAL_STARTING || _state == Mycila::ESPConnect::State::PORTAL_STARTED) {
        LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_ETH_GOT_IP", getStateName());
        if (_state == Mycila::ESPConnect::State::PORTAL_STARTING || _state == Mycila::ESPConnect::State::PORTAL_STARTED) {
          _stopAP();
        }
        _lastTime = -1;
  #ifndef ESPCONNECT_NO_MDNS
        MDNS.begin(_config.hostname.c_str());
  #endif
        _setState(Mycila::ESPConnect::State::NETWORK_CONNECTED);
      }
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      if (_state == Mycila::ESPConnect::State::NETWORK_CONNECTED && WiFi.localIP()[0] == 0) {
        LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_ETH_DISCONNECTED", getStateName());
        _setState(Mycila::ESPConnect::State::NETWORK_DISCONNECTED);
      }
      break;
#endif

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      if (_state == Mycila::ESPConnect::State::NETWORK_CONNECTING || _state == Mycila::ESPConnect::State::NETWORK_RECONNECTING) {
        LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_WIFI_STA_GOT_IP", getStateName());
        _lastTime = -1;
#ifndef ESPCONNECT_NO_MDNS
        MDNS.begin(_config.hostname.c_str());
#endif
        _setState(Mycila::ESPConnect::State::NETWORK_CONNECTED);
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
      if (_state == Mycila::ESPConnect::State::NETWORK_CONNECTED) {
        // we have to move to state disconnected only if we are not connected to ethernet
#ifdef ESPCONNECT_ETH_SUPPORT
        if (ETH.linkUp() && ETH.localIP()[0] != 0)
          return;
#endif
        _setState(Mycila::ESPConnect::State::NETWORK_DISCONNECTED);
      }
      break;

    case ARDUINO_EVENT_WIFI_AP_START:
#ifndef ESPCONNECT_NO_MDNS
      MDNS.begin(_config.hostname.c_str());
#endif
      if (_state == Mycila::ESPConnect::State::AP_STARTING) {
        LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_WIFI_AP_START", getStateName());
        _setState(Mycila::ESPConnect::State::AP_STARTED);
      } else if (_state == Mycila::ESPConnect::State::PORTAL_STARTING) {
        LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_WIFI_AP_START", getStateName());
        _setState(Mycila::ESPConnect::State::PORTAL_STARTED);
      }
      break;

    default:
      break;
  }
}

bool Mycila::ESPConnect::_durationPassed(uint32_t intervalSec) {
  if (_lastTime >= 0 && millis() - static_cast<uint32_t>(_lastTime) >= intervalSec * 1000) {
    _lastTime = -1;
    return true;
  }
  return false;
}

void Mycila::ESPConnect::_scan() {
  WiFi.scanDelete();
#ifndef ESP8266
  WiFi.scanNetworks(true, false, false, 500, 0, nullptr, nullptr);
#else
  WiFi.scanNetworks(true);
#endif
}
