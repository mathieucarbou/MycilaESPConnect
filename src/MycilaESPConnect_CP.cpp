// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2025 Mathieu Carbou
 */
#ifndef ESPCONNECT_NO_CAPTIVE_PORTAL
  #include "MycilaESPConnect.h"
  #include "MycilaESPConnect_Includes.h"
  #include "MycilaESPConnect_Logging.h"
  #include "espconnect_webpage.h"

void Mycila::ESPConnect::_startCaptivePortal() {
  LOGI(TAG, "Starting Captive Portal...");
  _setState(Mycila::ESPConnect::State::PORTAL_STARTING);

  #ifndef ESP8266
  WiFi.softAPsetHostname(_config.hostname.c_str());
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  #endif

  WiFi.setHostname(_config.hostname.c_str());
  WiFi.setSleep(false);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);

  // Configure AP with specific IP range so devices recognize it as a captive portal
  WiFi.softAPConfig(IPAddress(4, 3, 2, 1), IPAddress(4, 3, 2, 1), IPAddress(255, 255, 255, 0));
  WiFi.mode(WIFI_AP_STA);

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
          entry["bssid"] = WiFi.BSSIDstr(i);
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
        ESPCONNECT_STRING bssid;
        ESPCONNECT_STRING ssid;
        ESPCONNECT_STRING password;
        if (request->hasParam("bssid", true))
          bssid = request->getParam("bssid", true)->value().c_str();
        if (request->hasParam("ssid", true))
          ssid = request->getParam("ssid", true)->value().c_str();
        if (request->hasParam("password", true))
          password = request->getParam("password", true)->value().c_str();
        if (!bssid.length())
          return request->send(400, "application/json", "{\"message\":\"Invalid BSSID\"}");
        if (!ssid.length())
          return request->send(400, "application/json", "{\"message\":\"Invalid SSID\"}");
        if (ssid.length() > 32 || password.length() > 64 || (password.length() && password.length() < 8))
          return request->send(400, "application/json", "{\"message\":\"Credentials exceed character limit of 32 & 64 respectively, or password lower than 8 characters.\"}");
        _config.wifiBSSID = bssid;
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

  #ifndef ESPCONNECT_NO_COMPAT_CP
  // Microsoft Windows connectivity check - redirects to logout.net to trigger captive portal detection
  if (_connecttestHandler == nullptr)
    _connecttestHandler = &_httpd->on("/connecttest.txt", [](AsyncWebServerRequest* request) {
      request->redirect("http://logout.net");
    });

  // Web Proxy Auto-Discovery Protocol - returns 404 as we don't provide proxy configuration
  if (_wpadHandler == nullptr)
    _wpadHandler = &_httpd->on("/wpad.dat", [](AsyncWebServerRequest* request) {
      request->send(404);
    });

  // Android connectivity check - redirects to captive portal when no internet detected
  if (_generate204Handler == nullptr)
    _generate204Handler = &_httpd->on("/generate_204", [this](AsyncWebServerRequest* request) {
      request->redirect((WiFi.softAPIP().toString()).c_str());
    });

  // Generic redirect endpoint - forwards to captive portal interface
  if (_redirectHandler == nullptr)
    _redirectHandler = &_httpd->on("/redirect", [this](AsyncWebServerRequest* request) {
      request->redirect((WiFi.softAPIP().toString()).c_str());
    });

  // Apple iOS/macOS hotspot detection - redirects to captive portal when connectivity test fails
  if (_hotspotDetectHandler == nullptr)
    _hotspotDetectHandler = &_httpd->on("/hotspot-detect.html", [this](AsyncWebServerRequest* request) {
      request->redirect((WiFi.softAPIP().toString()).c_str());
    });

  // Ubuntu/Linux connectivity check - redirects to captive portal configuration page
  if (_canonicalHandler == nullptr)
    _canonicalHandler = &_httpd->on("/canonical.html", [this](AsyncWebServerRequest* request) {
      request->redirect((WiFi.softAPIP().toString()).c_str());
    });

  // Microsoft connectivity test success page - returns 200 OK to indicate successful connection
  if (_successHandler == nullptr)
    _successHandler = &_httpd->on("/success.txt", [](AsyncWebServerRequest* request) {
      request->send(200);
    });

  // Microsoft Network Connectivity Status Indicator - redirects to portal for configuration
  if (_ncsiHandler == nullptr)
    _ncsiHandler = &_httpd->on("/ncsi.txt", [this](AsyncWebServerRequest* request) {
      request->redirect((WiFi.softAPIP().toString()).c_str());
    });

  // Generic start page endpoint - redirects users to the main captive portal interface
  if (_startpageHandler == nullptr)
    _startpageHandler = &_httpd->on("/startpage", [this](AsyncWebServerRequest* request) {
      request->redirect((WiFi.softAPIP().toString()).c_str());
    });
  #endif

  _httpd->onNotFound([](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse(200, "text/html", ESPCONNECT_HTML, sizeof(ESPCONNECT_HTML));
    response->addHeader("Content-Encoding", "gzip");
    return request->send(response);
  });

  _httpd->begin();

  #ifdef ESP8266
  _onWiFiEvent(ARDUINO_EVENT_WIFI_AP_START);
  #endif

  LOGI(TAG, "Captive Portal started.");
  _lastTime = millis();
}

void Mycila::ESPConnect::_stopCaptivePortal() {
  LOGI(TAG, "Stopping Captive Portal...");
  _lastTime = -1;

  WiFi.softAPdisconnect(true);

  if (_dnsServer != nullptr) {
    _dnsServer->stop();
    delete _dnsServer;
    _dnsServer = nullptr;
  }

  if (_homeHandler == nullptr)
    return;

  LOGI(TAG, "Disable Captive Portal...");

  WiFi.scanDelete();

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

  #ifndef ESPCONNECT_NO_COMPAT_CP
  if (_connecttestHandler != nullptr) {
    _httpd->removeHandler(_connecttestHandler);
    _connecttestHandler = nullptr;
  }
  if (_wpadHandler != nullptr) {
    _httpd->removeHandler(_wpadHandler);
    _wpadHandler = nullptr;
  }
  if (_generate204Handler != nullptr) {
    _httpd->removeHandler(_generate204Handler);
    _generate204Handler = nullptr;
  }
  if (_redirectHandler != nullptr) {
    _httpd->removeHandler(_redirectHandler);
    _redirectHandler = nullptr;
  }
  if (_hotspotDetectHandler != nullptr) {
    _httpd->removeHandler(_hotspotDetectHandler);
    _hotspotDetectHandler = nullptr;
  }
  if (_canonicalHandler != nullptr) {
    _httpd->removeHandler(_canonicalHandler);
    _canonicalHandler = nullptr;
  }
  if (_successHandler != nullptr) {
    _httpd->removeHandler(_successHandler);
    _successHandler = nullptr;
  }
  if (_ncsiHandler != nullptr) {
    _httpd->removeHandler(_ncsiHandler);
    _ncsiHandler = nullptr;
  }
  if (_startpageHandler != nullptr) {
    _httpd->removeHandler(_startpageHandler);
    _startpageHandler = nullptr;
  }
  #endif

  LOGI(TAG, "Captive Portal stopped.");
}

void Mycila::ESPConnect::toJson(const JsonObject& root) const {
  root["ip_address"] = getIPAddress().toString();
  root["ip_address_ap"] = getIPAddress(Mycila::ESPConnect::Mode::AP).toString();
  root["ip_address_eth_v4"] = getIPAddress(Mycila::ESPConnect::Mode::ETH).toString();
  root["ip_address_sta_v4"] = getIPAddress(Mycila::ESPConnect::Mode::STA).toString();
  root["ip_address_eth_v6_local"] = getLinkLocalIPv6Address(Mycila::ESPConnect::Mode::ETH).toString();
  root["ip_address_sta_v6_local"] = getLinkLocalIPv6Address(Mycila::ESPConnect::Mode::STA).toString();
  root["ip_address_eth_v6_global"] = getGlobalIPv6Address(Mycila::ESPConnect::Mode::ETH).toString();
  root["ip_address_sta_v6_global"] = getGlobalIPv6Address(Mycila::ESPConnect::Mode::STA).toString();
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

void Mycila::ESPConnect::_scan() {
  WiFi.scanDelete();
#ifndef ESP8266
  WiFi.scanNetworks(true, false, false, 500, 0, nullptr, nullptr);
#else
  WiFi.scanNetworks(true);
#endif
}

#endif
