// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2024 Mathieu Carbou
 */
#pragma once

#include <ArduinoJson.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>

#ifdef ESP8266
  #include <ESP8266WiFi.h>
#else
  #include <WiFi.h>
#endif

#define ESPCONNECT_VERSION          "4.6.0"
#define ESPCONNECT_VERSION_MAJOR    4
#define ESPCONNECT_VERSION_MINOR    6
#define ESPCONNECT_VERSION_REVISION 0

#ifndef ESPCONNECT_CONNECTION_TIMEOUT
  #define ESPCONNECT_CONNECTION_TIMEOUT 20
#endif

#ifndef ESPCONNECT_PORTAL_TIMEOUT
  #define ESPCONNECT_PORTAL_TIMEOUT 180
#endif

#ifndef ESPCONNECT_PORTAL_SCAN_TIMEOUT
  #define ESPCONNECT_PORTAL_SCAN_TIMEOUT 20
#endif

enum class ESPConnectState {
  // end() => NETWORK_DISABLED
  NETWORK_DISABLED = 0,
  // NETWORK_DISABLED => NETWORK_ENABLED
  NETWORK_ENABLED,

  // NETWORK_ENABLED => NETWORK_CONNECTING
  NETWORK_CONNECTING,
  // NETWORK_CONNECTING => NETWORK_TIMEOUT
  NETWORK_TIMEOUT,
  // NETWORK_CONNECTING => NETWORK_CONNECTED
  // NETWORK_RECONNECTING => NETWORK_CONNECTED
  NETWORK_CONNECTED, // final state
  // NETWORK_CONNECTED => NETWORK_DISCONNECTED
  NETWORK_DISCONNECTED,
  // NETWORK_DISCONNECTED => NETWORK_RECONNECTING
  NETWORK_RECONNECTING,

  // NETWORK_ENABLED => AP_STARTING
  AP_STARTING,
  // AP AP_STARTING => AP_STARTED
  AP_STARTED, // final state

  // NETWORK_ENABLED => PORTAL_STARTING
  // NETWORK_TIMEOUT => PORTAL_STARTING
  PORTAL_STARTING,
  // PORTAL_STARTING => PORTAL_STARTED
  PORTAL_STARTED,
  // PORTAL_STARTED => PORTAL_COMPLETE
  PORTAL_COMPLETE, // final state
  // PORTAL_STARTED => PORTAL_TIMEOUT
  PORTAL_TIMEOUT, // final state
};

enum class ESPConnectMode {
  NONE = 0,
  // wifi ap
  AP,
  // wifi sta
  STA,
  // ethernet
  ETH
};

typedef std::function<void(ESPConnectState previous, ESPConnectState state)> ESPConnectStateCallback;

typedef struct {
    // SSID name to connect to, loaded from config or set from begin(), or from the captive portal
    String wifiSSID;
    // Password for the WiFi to connect to, loaded from config or set from begin(), or from the captive portal
    String wifiPassword;
    // whether we need to set the ESP to stay in AP mode or not, loaded from config, begin(), or from captive portal
    bool apMode;
} ESPConnectConfig;

class ESPConnectClass {
  public:
    ~ESPConnectClass() { end(); }

    // Start ESPConnect:
    //
    // 1. Load the configuration
    // 2. If apMode is true, starts in AP Mode
    // 3. If apMode is false, try to start in STA mode
    // 4. If STA mode times out, or nothing configured, starts the captive portal
    //
    // Using this method will activate auto-load and auto-save of the configuration
    void begin(AsyncWebServer& httpd, const String& hostname, const String& apSSID, const String& apPassword = emptyString); // NOLINT

    // Start ESPConnect:
    //
    // 1. If apMode is true, starts in AP Mode
    // 2. If apMode is false, try to start in STA mode
    // 3. If STA mode fails, or empty WiFi credentials were passed, starts the captive portal
    //
    // Using this method will NOT auto-load or auto-save any configuration
    void begin(AsyncWebServer& httpd, const String& hostname, const String& apSSID, const String& apPassword, const ESPConnectConfig& config); // NOLINT

    // loop() method to be called from main loop()
    void loop();

    // Stops the network stack
    void end();

    // Listen for network state change
    void listen(ESPConnectStateCallback callback) { _callback = callback; }

    // Returns the current network state
    ESPConnectState getState() const { return _state; }
    // Returns the current network state name
    const char* getStateName() const;
    const char* getStateName(ESPConnectState state) const;

    // returns the current default mode of the ESP (STA, AP, ETH). ETH has priority over STA if both are connected
    ESPConnectMode getMode() const;

    inline bool isConnected() const { return getIPAddress()[0] != 0; }

    inline const String getMACAddress() const { return getMACAddress(getMode()); }
    const String getMACAddress(ESPConnectMode mode) const;

    // Returns the IP address of the current Ethernet, WiFi, or IP address of the AP or captive portal, or empty if not available
    inline const IPAddress getIPAddress() const { return getIPAddress(getMode()); }
    const IPAddress getIPAddress(ESPConnectMode mode) const;

    // Returns the SSID of the current WiFi, or SSID of the AP or captive portal, or empty if not available
    const String getWiFiSSID() const;
    // Returns the BSSID of the current WiFi, or BSSID of the AP or captive portal, or empty if not available
    const String getWiFiBSSID() const;
    // Returns the RSSI of the current WiFi, or -1 if not available
    int8_t getWiFiRSSI() const;
    // Returns the signal quality (percentage from 0 to 100) of the current WiFi, or -1 if not available
    int8_t getWiFiSignalQuality() const;

    // the hostname passed from begin()
    const String& getHostname() const { return _hostname; }

    // SSID name used for the captive portal or in AP mode
    const String& getAccessPointSSID() const { return _apSSID; }
    // Password used for the captive portal or in AP mode
    const String& getAccessPointPassword() const { return _apPassword; }

    // Returns the current configuration loaded or passed from begin() or from captive portal
    const ESPConnectConfig& getConfig() const { return _config; }
    // SSID name to connect to, loaded from config or set from begin(), or from the captive portal
    const String& getConfiguredWiFiSSID() const { return _config.wifiSSID; }
    // Password for the WiFi to connect to, loaded from config or set from begin(), or from the captive portal
    const String& getConfiguredWiFiPassword() const { return _config.wifiPassword; }
    // whether we need to set the ESP to stay in AP mode or not, loaded from config, begin(), or from captive portal
    bool hasConfiguredAPMode() const { return _config.apMode; }

    // Maximum duration that the captive portal will be active before closing
    uint32_t getCaptivePortalTimeout() const { return _portalTimeout; }
    // Maximum duration that the captive portal will be active before closing
    void setCaptivePortalTimeout(uint32_t timeout) { _portalTimeout = timeout; }

    // Maximum duration that the ESP will try to connect to the WiFi before giving up and start the captive portal
    uint32_t getConnectTimeout() const { return _connectTimeout; }
    // Maximum duration that the ESP will try to connect to the WiFi before giving up and start the captive portal
    void setConnectTimeout(uint32_t timeout) { _connectTimeout = timeout; }

    // Whether ESPConnect will block in the begin() method until the network is ready or not (old behaviour)
    bool isBlocking() const { return _blocking; }
    // Whether ESPConnect will block in the begin() method until the network is ready or not (old behaviour)
    void setBlocking(bool blocking) { _blocking = blocking; }

    // Whether ESPConnect will restart the ESP if the captive portal times out or once it has completed (old behaviour)
    bool isAutoRestart() const { return _autoRestart; }
    // Whether ESPConnect will restart the ESP if the captive portal times out or once it has completed (old behaviour)
    void setAutoRestart(bool autoRestart) { _autoRestart = autoRestart; }

    // when using auto-load and save of configuration, this method can clear saved states.
    void clearConfiguration();

    void toJson(const JsonObject& root) const;

  private:
    AsyncWebServer* _httpd = nullptr;
    ESPConnectState _state = ESPConnectState::NETWORK_DISABLED;
    ESPConnectStateCallback _callback = nullptr;
    DNSServer* _dnsServer = nullptr;
    int64_t _lastTime = -1;
    String _hostname = emptyString;
    String _apSSID = emptyString;
    String _apPassword = emptyString;
    uint32_t _connectTimeout = ESPCONNECT_CONNECTION_TIMEOUT;
    uint32_t _portalTimeout = ESPCONNECT_PORTAL_TIMEOUT;
    uint32_t _scanStart = 0;
    uint32_t _scanTimeout = ESPCONNECT_PORTAL_SCAN_TIMEOUT;
    ESPConnectConfig _config;
#ifndef ESP8266
    WiFiEventId_t _wifiEventListenerId = 0;
#endif
    bool _blocking = true;
    bool _autoRestart = true;
    bool _autoSave = false;
    AsyncCallbackWebHandler* _scanHandler = nullptr;
    AsyncCallbackWebHandler* _connectHandler = nullptr;
    AsyncCallbackWebHandler* _homeHandler = nullptr;
#ifdef ESP8266
    WiFiEventHandler onStationModeGotIP;
    WiFiEventHandler onStationModeDHCPTimeout;
    WiFiEventHandler onStationModeDisconnected;
#endif

  private:
    void _setState(ESPConnectState state);
    void _startSTA();
    void _startAP();
    void _stopAP();
    void _enableCaptivePortal();
    void _disableCaptivePortal();
    void _onWiFiEvent(WiFiEvent_t event);
    bool _durationPassed(uint32_t intervalSec);
#ifdef ESPCONNECT_ETH_SUPPORT
    void _startEthernet();
#endif

  private:
    static int8_t _wifiSignalQuality(int32_t rssi);
};

extern ESPConnectClass ESPConnect;
