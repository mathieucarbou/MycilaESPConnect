// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2025 Mathieu Carbou
 */
#include "MycilaESPConnect.h"
#include "MycilaESPConnect_Includes.h"
#include "MycilaESPConnect_Logging.h"

#ifndef ESPCONNECT_NO_MDNS
  #ifdef ESP8266
    #include <ESP8266mDNS.h>
  #else
    #include <ESPmDNS.h>
  #endif
#endif

#include <utility>

void Mycila::ESPConnect::begin(const char* hostname, const char* apSSID, const char* apPassword) {
  if (_state != Mycila::ESPConnect::State::NETWORK_DISABLED)
    return;

  _autoSave = true;
  Config config;
  loadConfiguration(config);
  config.hostname = hostname == nullptr ? "" : hostname;
  begin(apSSID, apPassword, std::move(config));
}

void Mycila::ESPConnect::begin(const char* apSSID, const char* apPassword, Mycila::ESPConnect::Config config) {
  if (_state != Mycila::ESPConnect::State::NETWORK_DISABLED)
    return;

  _apSSID = apSSID;
  _apPassword = apPassword;
  _config = std::move(config);

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

#ifndef ESPCONNECT_NO_CAPTIVE_PORTAL
  _processCredentialTest();
#endif

  // Network has just been enable ?
  if (_state == Mycila::ESPConnect::State::NETWORK_ENABLED) {
    // AP Mode has higher priority
    if (_config.apMode) {
      _startAP();
      return;
    }

    // No AP mode ?

#ifdef ESPCONNECT_ETH_SUPPORT
    // If we have an ETH board, let's activate both ETH and WiFi if a SSID is configured
    _startEthernet();
    if (_config.wifiSSID.length()) {
      _startSTA();
    }
    return;
#else
    // No ETH board ? Then just start WiFi if a SSID is configured, otherwise start captive portal directly
    if (_config.wifiSSID.length()) {
      _startSTA();
    } else {
  #if !defined(ESPCONNECT_NO_CAPTIVE_PORTAL)
      _startCaptivePortal();
  #else
      LOGE(TAG, "No SSID configured and captive portal disabled. Staying idle...");
      _setState(Mycila::ESPConnect::State::NETWORK_DISABLED);
  #endif
    }
    return;
#endif
  }

  // connection to WiFi or Ethernet times out ?
  if (_state == Mycila::ESPConnect::State::NETWORK_CONNECTING && _durationPassed(_connectTimeout)) {
    if (WiFi.getMode() != WIFI_MODE_NULL) {
      WiFi.config(static_cast<uint32_t>(0x00000000), static_cast<uint32_t>(0x00000000), static_cast<uint32_t>(0x00000000), static_cast<uint32_t>(0x00000000));
      WiFi.disconnect(true, true);
    }
    _setState(Mycila::ESPConnect::State::NETWORK_TIMEOUT);
    return;
  }

  // start captive portal on connect timeout
  if (_state == Mycila::ESPConnect::State::NETWORK_TIMEOUT) {
#if !defined(ESPCONNECT_NO_CAPTIVE_PORTAL)
    _startCaptivePortal();
#else
    LOGW(TAG, "Connect timeout and captive portal disabled. Retrying to connect...");
    _setState(Mycila::ESPConnect::State::NETWORK_ENABLED);
#endif
    return;
  }

  // disconnect from network ? reconnect!
  if (_state == Mycila::ESPConnect::State::NETWORK_DISCONNECTED) {
    _setState(Mycila::ESPConnect::State::NETWORK_RECONNECTING);
    return;
  }

#ifndef ESPCONNECT_NO_CAPTIVE_PORTAL
  // timeout portal if we failed to connect to WiFi (we got a SSID) and portal duration is passed
  // in order to restart and try again to connect to the configured WiFi
  if (_state == Mycila::ESPConnect::State::PORTAL_STARTED && _config.wifiSSID.length() && _durationPassed(_portalTimeout)) {
    _setState(Mycila::ESPConnect::State::PORTAL_TIMEOUT);
    return;
  }

  if (_state == Mycila::ESPConnect::State::PORTAL_TIMEOUT) {
    LOGW(TAG, "Portal timeout!");
    if (_autoRestart) {
      LOGW(TAG, "Restarting ESP...");
      ESP.restart();
    } else {
      // try to reconnect again with configured settings
      _stopAP();
      _setState(Mycila::ESPConnect::State::NETWORK_ENABLED);
    }
    return;
  }

  if (_state == Mycila::ESPConnect::State::PORTAL_COMPLETE) {
    if (_autoRestart) {
      if (!_restartRequestTime) {
        // init _restartRequestTime if not already set to teh time when the portal completed
        _restartRequestTime = millis();
      } else if (millis() - _restartRequestTime >= _restartDelay) {
        // delay is over restart
        LOGW(TAG, "Auto Restart of ESP...");
        ESP.restart();
      }
    } else {
      _stopCaptivePortal();
      // try to reconnect again with new configured settings
      _setState(Mycila::ESPConnect::State::NETWORK_ENABLED);
    }
    return;
  }
#endif
}

void Mycila::ESPConnect::_setState(Mycila::ESPConnect::State state) {
  if (_state == state)
    return;

  const Mycila::ESPConnect::State previous = _state;
  _state = state;
  LOGD(TAG, "State: %s => %s", getStateName(previous), getStateName(state));

#ifndef ESPCONNECT_NO_CAPTIVE_PORTAL
  // be sure to save anything before auto restart and callback
  if (_state == Mycila::ESPConnect::State::PORTAL_COMPLETE && _autoSave) {
    saveConfiguration();
  }
#endif

  // make sure callback is called before auto restart
  if (_callback != nullptr)
    _callback(previous, state);
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
  #ifndef ESPCONNECT_NO_CAPTIVE_PORTAL
      if (_state == Mycila::ESPConnect::State::PORTAL_STARTING || _state == Mycila::ESPConnect::State::PORTAL_STARTED) {
        _stopCaptivePortal();
      }
  #endif
      if (_state != Mycila::ESPConnect::State::NETWORK_CONNECTED && _state != Mycila::ESPConnect::State::NETWORK_DISABLED && _state != Mycila::ESPConnect::State::AP_STARTING && _state != Mycila::ESPConnect::State::AP_STARTED) {
        LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_ETH_GOT_IP", getStateName());

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
      LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_WIFI_STA_GOT_IP: %s", getStateName(), WiFi.localIP().toString().c_str());
      if (_state == Mycila::ESPConnect::State::NETWORK_CONNECTING || _state == Mycila::ESPConnect::State::NETWORK_RECONNECTING) {
        _lastTime = -1;
        _setState(Mycila::ESPConnect::State::NETWORK_CONNECTED);
      }
#ifndef ESPCONNECT_NO_MDNS
      MDNS.begin(_config.hostname.c_str());
#endif
      break;

#ifndef ESP8266
    case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
      if (WiFi.linkLocalIPv6() != IN6ADDR_ANY) {
        if (WiFi.globalIPv6() == IN6ADDR_ANY) {
          LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_WIFI_STA_GOT_IP6: Link-local: %s, global: <empty>", getStateName(), WiFi.linkLocalIPv6().toString().c_str());
        } else {
          LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_WIFI_STA_GOT_IP6: Link-local: %s, global: %s", getStateName(), WiFi.linkLocalIPv6().toString().c_str(), WiFi.globalIPv6().toString().c_str());
        }
      }
      break;
#endif

    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      // try to reconnect to WiFi only if we have a SSID configured
      if (_config.wifiSSID.length()) {
        WiFi.reconnect();
      }
      if (_state == Mycila::ESPConnect::State::NETWORK_CONNECTED) {
        // log event
        if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
          LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_WIFI_STA_DISCONNECTED", getStateName());
        } else {
          LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_WIFI_STA_LOST_IP", getStateName());
        }

        // we have to move to state disconnected only if we are not connected to ethernet with LAN
#ifdef ESPCONNECT_ETH_SUPPORT
        if (ETH.linkUp() && ETH.localIP()[0] != 0)
          break;
#endif
        _setState(Mycila::ESPConnect::State::NETWORK_DISCONNECTED);
      }
      break;

    case ARDUINO_EVENT_WIFI_AP_START:
      if (_state == Mycila::ESPConnect::State::AP_STARTING) {
        LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_WIFI_AP_START", getStateName());
        _setState(Mycila::ESPConnect::State::AP_STARTED);
      }

#ifndef ESPCONNECT_NO_CAPTIVE_PORTAL
      if (_state == Mycila::ESPConnect::State::PORTAL_STARTING) {
        LOGD(TAG, "[%s] WiFiEvent: ARDUINO_EVENT_WIFI_AP_START", getStateName());
        _setState(Mycila::ESPConnect::State::PORTAL_STARTED);
      }
#endif
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
