// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2025 Mathieu Carbou
 */
#pragma once

#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>

#include <string>

#define ESPCONNECT_VERSION          "7.1.1"
#define ESPCONNECT_VERSION_MAJOR    7
#define ESPCONNECT_VERSION_MINOR    1
#define ESPCONNECT_VERSION_REVISION 1

#ifndef ESPCONNECT_CONNECTION_TIMEOUT
  #define ESPCONNECT_CONNECTION_TIMEOUT 20
#endif

#ifndef ESPCONNECT_PORTAL_TIMEOUT
  #define ESPCONNECT_PORTAL_TIMEOUT 180
#endif

//  Uncomment to disable the captive portal handler and UI
// #define ESPCONNECT_NO_CAPTIVE_PORTAL

namespace Mycila {
  class ESPConnect {
    public:
      enum class State {
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

      enum class Mode {
        NONE = 0,
        // wifi ap
        AP,
        // wifi sta
        STA,
        // ethernet
        ETH
      };

      typedef std::function<void(State previous, State state)> StateCallback;

      typedef struct {
          // Static IP address to use when connecting to WiFi (STA mode) or Ethernet
          // If not set, DHCP will be used
          IPAddress ip;
          // Subnet mask: 255.255.255.0
          IPAddress subnet;
          IPAddress gateway;
          IPAddress dns;
      } IPConfig;

      typedef struct {
          // SSID name to connect to, loaded from config or set from begin(), or from the captive portal
          std::string wifiSSID;
          // Password for the WiFi to connect to, loaded from config or set from begin(), or from the captive portal
          std::string wifiPassword;
          // whether we need to set the ESP to stay in AP mode or not, loaded from config, begin(), or from captive portal
          bool apMode;
          // Static IP configuration to use (if any)
          IPConfig ipConfig;
      } Config;

    public:
      explicit ESPConnect(AsyncWebServer& httpd) : _httpd(&httpd) {}
      ~ESPConnect() { end(); }

      // Start ESPConnect:
      //
      // 1. Load the configuration
      // 2. If apMode is true, starts in AP Mode
      // 3. If apMode is false, try to start in STA mode
      // 4. If STA mode times out, or nothing configured, starts the captive portal
      //
      // Using this method will activate auto-load and auto-save of the configuration
      void begin(const char* hostname, const char* apSSID, const char* apPassword = ""); // NOLINT

      // Start ESPConnect:
      //
      // 1. If apMode is true, starts in AP Mode
      // 2. If apMode is false, try to start in STA mode
      // 3. If STA mode fails, or empty WiFi credentials were passed, starts the captive portal
      //
      // Using this method will NOT auto-load or auto-save any configuration
      void begin(const char* hostname, const char* apSSID, const char* apPassword, const Config& config); // NOLINT

      // loop() method to be called from main loop()
      void loop();

      // Stops the network stack
      void end();

      // Listen for network state change
      void listen(StateCallback callback) { _callback = callback; }

      // Returns the current network state
      State getState() const { return _state; }
      // Returns the current network state name
      const char* getStateName() const;
      const char* getStateName(State state) const;

      // returns the current default mode of the ESP (STA, AP, ETH). ETH has priority over STA if both are connected
      Mode getMode() const;

      bool isConnected() const { return getIPAddress()[0] != 0; }

      std::string getMACAddress() const { return getMACAddress(getMode()); }
      std::string getMACAddress(Mode mode) const;

      // Returns the IP address of the current Ethernet, WiFi, or IP address of the AP or captive portal, or empty if not available
      IPAddress getIPAddress() const { return getIPAddress(getMode()); }
      IPAddress getIPAddress(Mode mode) const;

      // Returns the configured WiFi SSID or the configured SSID of the AP or captive portal, or empty if not available, depending on the current mode
      std::string getWiFiSSID() const;
      // Returns the BSSID of the current WiFi, or BSSID of the AP or captive portal, or empty if not available
      std::string getWiFiBSSID() const;
      // Returns the RSSI of the current WiFi, or -1 if not available
      int8_t getWiFiRSSI() const;
      // Returns the signal quality (percentage from 0 to 100) of the current WiFi, or -1 if not available
      int8_t getWiFiSignalQuality() const;

      // the hostname passed from begin()
      const std::string& getHostname() const { return _hostname; }

      // SSID name used for the captive portal or in AP mode
      const std::string& getAccessPointSSID() const { return _apSSID; }
      // Password used for the captive portal or in AP mode
      const std::string& getAccessPointPassword() const { return _apPassword; }

      // Returns the current configuration loaded or passed from begin() or from captive portal
      const Config& getConfig() const { return _config; }
      // SSID name to connect to, loaded from config or set from begin(), or from the captive portal
      const std::string& getConfiguredWiFiSSID() const { return _config.wifiSSID; }
      // Password for the WiFi to connect to, loaded from config or set from begin(), or from the captive portal
      const std::string& getConfiguredWiFiPassword() const { return _config.wifiPassword; }
      // whether we need to set the ESP to stay in AP mode or not, loaded from config, begin(), or from captive portal
      bool hasConfiguredAPMode() const { return _config.apMode; }

      // IP configuration used for WiFi or ETH
      const IPConfig& getIPConfig() const { return _config.ipConfig; }
      // Static IP configuration: by default, DHCP is used
      // The static IP configuration applies to the WiFi STA connection, except if ETH is used for ETH board, then it applies only to the Ethernet connection.
      void setIPConfig(const IPConfig& ipConfig) { _config.ipConfig = ipConfig; }

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

      // load configuration from NVS
      void loadConfiguration() { loadConfiguration(_config); }
      // load configuration from NVS
      static void loadConfiguration(Config& config); // NOLINT

      // save configuration to NVS
      void saveConfiguration() { saveConfiguration(_config); }
      // save configuration to NVS
      static void saveConfiguration(const Config& config);

      // when using auto-load and save of configuration, this method can clear saved states.
      void clearConfiguration();

      void toJson(const JsonObject& root) const;

    private:
      AsyncWebServer* _httpd = nullptr;
      State _state = State::NETWORK_DISABLED;
      StateCallback _callback = nullptr;
      DNSServer* _dnsServer = nullptr;
      int64_t _lastTime = -1;
      std::string _hostname;
      std::string _apSSID;
      std::string _apPassword;
      uint32_t _connectTimeout = ESPCONNECT_CONNECTION_TIMEOUT;
      uint32_t _portalTimeout = ESPCONNECT_PORTAL_TIMEOUT;
      Config _config;
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
      void _setState(State state);
      void _startSTA();
      void _startAP();
      void _stopAP();
      void _enableCaptivePortal();
      void _disableCaptivePortal();
      void _onWiFiEvent(WiFiEvent_t event);
      bool _durationPassed(uint32_t intervalSec);
      void _scan();
#ifdef ESPCONNECT_ETH_SUPPORT
      void _startEthernet();
#endif

    private:
      static int8_t _wifiSignalQuality(int32_t rssi);
  };
} // namespace Mycila
