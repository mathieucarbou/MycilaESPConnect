# MycilaESPConnect

[![Latest Release](https://img.shields.io/github/release/mathieucarbou/MycilaESPConnect.svg)](https://GitHub.com/mathieucarbou/MycilaESPConnect/releases/)
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/mathieucarbou/library/MycilaESPConnect.svg)](https://registry.platformio.org/libraries/mathieucarbou/MycilaESPConnect)

[![GPLv3 license](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0.txt)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

[![Contributor Covenant](https://img.shields.io/badge/Contributor%20Covenant-2.1-4baaaa.svg)](code_of_conduct.md)

[![Build](https://github.com/mathieucarbou/MycilaESPConnect/actions/workflows/ci.yml/badge.svg)](https://github.com/mathieucarbou/MycilaESPConnect/actions/workflows/ci.yml)
[![GitHub latest commit](https://badgen.net/github/last-commit/mathieucarbou/MycilaESPConnect)](https://GitHub.com/mathieucarbou/MycilaESPConnect/commit/)

Simple & Easy Network Manager for ESP32 and ESP8266 with WiFi, Ethernet and Captive Portal support.

[![](https://mathieu.carbou.me/MycilaESPConnect/screenshot.png)](https://mathieu.carbou.me/MycilaESPConnect/screenshot.png)

This library is based on the UI from [https://github.com/ayushsharma82/ESPConnect](https://github.com/ayushsharma82/ESPConnect) (and this part falls under GPL v3).
I highly recommend looking at all OSS projects (and products) from [@ayushsharma82](https://github.com/ayushsharma82).
He is making great Arduino libraries.

- [MycilaESPConnect](#mycilaespconnect)
  - [Features](#features)
  - [Installation](#installation)
    - [PlatformIO](#platformio)
    - [Arduino IDE](#arduino-ide)
    - [Compile Flags](#compile-flags)
    - [mDNS](#mdns)
  - [Usage](#usage)
    - [Blocking mode](#blocking-mode)
    - [Non-blocking mode](#non-blocking-mode)
    - [No Captive Portal mode](#no-captive-portal-mode)
    - [External configuration system](#external-configuration-system)
    - [Static IP](#static-ip)
  - [API Reference](#api-reference)
    - [Constructor](#constructor)
    - [Lifecycle](#lifecycle)
    - [Configuration](#configuration)
    - [State and Mode](#state-and-mode)
    - [Network Information](#network-information)
    - [JSON serialization](#json-serialization)
    - [State machine](#state-machine)
    - [Config struct](#config-struct)
  - [ESP8266 Specifics](#esp8266-specifics)
  - [Ethernet Support](#ethernet-support)
  - [Logo](#logo)
  - [Captive Portal Detection Endpoints](#captive-portal-detection-endpoints)

## Features

- **Captive Portal**: embedded web UI to let the user configure the WiFi network or choose AP mode, with logo support
- **AP Mode**: the user can choose to remain in AP mode from the captive portal
- **Network State Machine**: robust state machine handling transitions between Captive Portal, AP Mode, STA mode and Ethernet
- **Callback**: listen to network state changes
- **Blocking and Non-blocking modes**: `begin()` can block until the network is ready, or return immediately and let `loop()` handle the rest
- **Flexible Configuration**: ESPConnect can handle configuration persistence automatically (NVS/Preferences), or let the application manage it
- **mDNS / DNS Support**
- **Ethernet support** (ESP32 only, both built-in RMII and SPI-based adapters)
- **IPv6 support** (ESP32 only)
- **Static IP configuration** (WiFi and Ethernet)
- **Arduino 3 / ESP-IDF 5 ready**
- **ESP32 and ESP8266 support**

## Installation

### PlatformIO

Add the library to your `platformio.ini` with its required dependencies:

**ESP32 (WiFi only):**

```ini
[env:esp32dev]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.39/platform-espressif32.zip
board = esp32dev
framework = arduino
lib_compat_mode = strict
lib_ldf_mode = chain
lib_deps =
  mathieucarbou/MycilaESPConnect @ ^10.6.0
  ESP32Async/AsyncTCP @ ^3.4.10
  ESP32Async/ESPAsyncWebServer @ ^3.11.2
  bblanchon/ArduinoJson @ ^7.4.3
```

**ESP32 (with Ethernet support):**

```ini
[env:esp32-eth]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.39/platform-espressif32.zip
board = esp32dev
framework = arduino
build_flags =
  -D ESPCONNECT_ETH_SUPPORT
lib_compat_mode = strict
lib_ldf_mode = chain
lib_deps =
  mathieucarbou/MycilaESPConnect @ ^10.6.0
  ESP32Async/AsyncTCP @ ^3.4.10
  ESP32Async/ESPAsyncWebServer @ ^3.11.2
  bblanchon/ArduinoJson @ ^7.4.3
```

**ESP8266:**

```ini
[env:esp8266]
platform = espressif8266
board = huzzah
framework = arduino
lib_compat_mode = strict
lib_ldf_mode = chain
lib_deps =
  mathieucarbou/MycilaESPConnect @ ^10.6.0
  ESP32Async/ESPAsyncTCP @ ^2.0.0
  ESP32Async/ESPAsyncWebServer @ ^3.11.2
  bblanchon/ArduinoJson @ ^7.4.3
  vshymanskyy/Preferences @ ^2.1.0
```

> **Note:** `ESPAsyncWebServer` and `ArduinoJson` are mandatory. `AsyncTCP` (or `ESPAsyncTCP` on ESP8266) must be provided separately as a transport layer. `Preferences` is only needed on ESP8266 when using the built-in configuration persistence.
>
> **ESP32 platform:** The official `espressif32` platform dropped Arduino support. Use the [pioarduino](https://github.com/pioarduino/platform-espressif32) fork instead, as shown above.

### Arduino IDE

Search for **MycilaESPConnect** in the Arduino Library Manager and install it along with its dependencies:

- [ESP32Async/ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer)
- [ESP32Async/AsyncTCP](https://github.com/ESP32Async/AsyncTCP) (ESP32) or [ESP32Async/ESPAsyncTCP](https://github.com/ESP32Async/ESPAsyncTCP) (ESP8266)
- [bblanchon/ArduinoJson](https://github.com/bblanchon/ArduinoJson)
- [vshymanskyy/Preferences](https://github.com/vshymanskyy/Preferences) _(ESP8266 only, for config persistence)_

### Compile Flags

| Flag | Description |
|---|---|
| `-D ESPCONNECT_ETH_SUPPORT` | Enable Ethernet support (ESP32 only) |
| `-D ESPCONNECT_ETH_RESET_ON_START` | Pull `ETH_PHY_POWER` LOW before powering the Ethernet PHY (useful for some boards) |
| `-D ESPCONNECT_NO_CAPTIVE_PORTAL` | Disable Captive Portal and the `ESPAsyncWebServer` / `ArduinoJson` dependencies |
| `-D ESPCONNECT_NO_MDNS` | Disable mDNS (~25 KB flash saving) |
| `-D ESPCONNECT_NO_COMPAT_CP` | Disable multi-OS captive portal detection endpoints (~2 KB flash saving) |
| `-D ESPCONNECT_NO_STD_STRING` | Use Arduino `String` instead of `std::string` |
| `-D ESPCONNECT_NO_LOGGING` | Disable all serial logging |
| `-D ESPCONNECT_CONNECTION_TIMEOUT=<sec>` | Override the default WiFi connection timeout (default: `20` seconds) |
| `-D ESPCONNECT_PORTAL_TIMEOUT=<sec>` | Override the default captive portal timeout (default: `180` seconds) |

### mDNS

mDNS takes quite a lot of space in flash (about 25 KB).
You can disable it with `-D ESPCONNECT_NO_MDNS`.
When enabled, the hostname is registered automatically when `NETWORK_CONNECTED` is reached.

## Usage

### Blocking mode

`begin()` blocks until the network is ready (either `AP_STARTED` or `NETWORK_CONNECTED`).
The captive portal is served inline while blocking.
With `setAutoRestart(true)` (the default), the ESP restarts automatically after the captive portal completes or times out, so execution never reaches the code after `begin()` in those cases.

```cpp
#include <MycilaESPConnect.h>

AsyncWebServer server(80);
Mycila::ESPConnect espConnect(server);

void setup() {
  Serial.begin(115200);

  espConnect.listen([](Mycila::ESPConnect::State previous, Mycila::ESPConnect::State state) {
    // react to state changes
  });

  espConnect.setAutoRestart(true);
  espConnect.setBlocking(true);
  espConnect.begin("arduino", "My Captive Portal");

  // reached only when NETWORK_CONNECTED or AP_STARTED
  Serial.println("Network is ready!");
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/plain", "Hello World!");
  });
  server.begin();
}

void loop() {
  espConnect.loop();
}
```

See also the [BlockingCaptivePortal](examples/BlockingCaptivePortal/BlockingCaptivePortal.ino) example.

### Non-blocking mode

`begin()` returns immediately. All network transitions happen inside `loop()`.
Use the state-change callback to start or stop your server in reaction to network events.

```cpp
#include <MycilaESPConnect.h>

AsyncWebServer server(80);
Mycila::ESPConnect espConnect(server);

void setup() {
  Serial.begin(115200);

  espConnect.listen([](Mycila::ESPConnect::State previous, Mycila::ESPConnect::State state) {
    switch (state) {
      case Mycila::ESPConnect::State::NETWORK_CONNECTED:
      case Mycila::ESPConnect::State::AP_STARTED:
        server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
          request->send(200, "text/plain", "Hello World!");
        }).setFilter([](AsyncWebServerRequest*) {
          return espConnect.getState() != Mycila::ESPConnect::State::PORTAL_STARTED;
        });
        server.begin();
        break;
      case Mycila::ESPConnect::State::NETWORK_DISCONNECTED:
        server.end();
        break;
      default:
        break;
    }
  });

  espConnect.setAutoRestart(true);
  espConnect.setBlocking(false);
  espConnect.begin("arduino", "My Captive Portal");

  Serial.println("setup() done, network starting in background...");
}

void loop() {
  espConnect.loop();
}
```

See also the [NonBlockingCaptivePortal](examples/NonBlockingCaptivePortal/NonBlockingCaptivePortal.ino) example.

### No Captive Portal mode

Compile with `-D ESPCONNECT_NO_CAPTIVE_PORTAL` to remove the web UI and its dependencies entirely.
The constructor takes no arguments in this mode. On connection timeout, the state reaches `NETWORK_TIMEOUT` and you must handle it manually (e.g. switch to AP mode).

```cpp
#include <MycilaESPConnect.h>

// Compile with: -D ESPCONNECT_NO_CAPTIVE_PORTAL

Mycila::ESPConnect espConnect;

void setup() {
  Serial.begin(115200);

  espConnect.listen([](Mycila::ESPConnect::State previous, Mycila::ESPConnect::State state) {
    if (state == Mycila::ESPConnect::State::NETWORK_TIMEOUT) {
      // Failed to connect: fall back to AP mode
      espConnect.getConfig().apMode = true;
    }
  });

  espConnect.setBlocking(true);
  espConnect.begin("arduino", "AP SSID");
}

void loop() {
  delay(100);
}
```

See also the [NoCaptivePortal](examples/NoCaptivePortal/NoCaptivePortal.ino) example.

### External configuration system

Use the two-argument `begin()` overload to supply and manage your own `Config` struct.
ESPConnect will not touch NVS — you load and save the configuration yourself.

```cpp
#include <MycilaESPConnect.h>
#include <Preferences.h>

AsyncWebServer server(80);
Mycila::ESPConnect espConnect(server);

void setup() {
  Serial.begin(115200);

  espConnect.listen([](Mycila::ESPConnect::State previous, Mycila::ESPConnect::State state) {
    switch (state) {
      case Mycila::ESPConnect::State::NETWORK_CONNECTED:
      case Mycila::ESPConnect::State::AP_STARTED:
        server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
          request->send(200, "text/plain", "Hello World!");
        }).setFilter([](AsyncWebServerRequest*) {
          return espConnect.getState() != Mycila::ESPConnect::State::PORTAL_STARTED;
        });
        server.begin();
        break;
      case Mycila::ESPConnect::State::NETWORK_DISCONNECTED:
        server.end();
        break;
      case Mycila::ESPConnect::State::PORTAL_COMPLETE: {
        // save whatever the user chose in the captive portal
        Preferences prefs;
        prefs.begin("app", false);
        prefs.putBool("ap", espConnect.getConfig().apMode);
        prefs.putString("ssid", espConnect.getConfig().wifiSSID.c_str());
        prefs.putString("pass", espConnect.getConfig().wifiPassword.c_str());
        prefs.end();
        break;
      }
      default:
        break;
    }
  });

  espConnect.setAutoRestart(true);
  espConnect.setBlocking(false);

  // load config from your own storage
  Preferences prefs;
  prefs.begin("app", true);
  Mycila::ESPConnect::Config config = {
    .hostname    = "arduino",
    .wifiSSID    = prefs.getString("ssid", "").c_str(),
    .wifiPassword = prefs.getString("pass", "").c_str(),
    .apMode      = prefs.getBool("ap", false),
  };
  prefs.end();

  espConnect.begin("My Captive Portal", "", config);
}

void loop() {
  espConnect.loop();
}
```

See also the [AdvancedCaptivePortal](examples/AdvancedCaptivePortal/AdvancedCaptivePortal.ino) and [LoadSaveConfig](examples/LoadSaveConfig/LoadSaveConfig.ino) examples.

### Static IP

Set static IP fields on the `Config` before (or after) calling `begin()` and call `saveConfiguration()` to persist them.
Pass an empty `IPConfig` (all zeroes) to revert to DHCP.

```cpp
// Set static IP
espConnect.getConfig().ipConfig.ip.fromString("192.168.1.99");
espConnect.getConfig().ipConfig.gateway.fromString("192.168.1.1");
espConnect.getConfig().ipConfig.subnet.fromString("255.255.255.0");
espConnect.getConfig().ipConfig.dns.fromString("192.168.1.1");
espConnect.saveConfiguration();

// Revert to DHCP
espConnect.getConfig().ipConfig = {};
espConnect.saveConfiguration();
```

The static IP is applied automatically on the next connection attempt.
See also the [WiFiStaticIP](examples/WiFiStaticIP/WiFiStaticIP.ino) example.

## API Reference

### Constructor

```cpp
// With Captive Portal (default)
Mycila::ESPConnect espConnect(server);   // AsyncWebServer& required

// Without Captive Portal (-D ESPCONNECT_NO_CAPTIVE_PORTAL)
Mycila::ESPConnect espConnect;
```

### Lifecycle

```cpp
// Auto-load/save variant: loads config from NVS, persists changes automatically.
// hostname  — mDNS name and AP hostname
// apSSID    — SSID of the captive portal / AP
// apPassword — optional password (must be >= 8 chars or it is ignored)
void begin(const char* hostname, const char* apSSID, const char* apPassword = "");

// Manual config variant: you provide and own the Config struct; nothing is read from / written to NVS.
void begin(const char* apSSID, const char* apPassword, Mycila::ESPConnect::Config config);

// Must be called from the Arduino loop() function.
void loop();

// Stops the network stack and resets the state machine to NETWORK_DISABLED.
void end();
```

### Configuration

```cpp
// Register a callback invoked on every state transition.
void listen(StateCallback callback);
// callback signature: void(Mycila::ESPConnect::State previous, Mycila::ESPConnect::State current)

// Blocking behaviour (default: true)
// When true, begin() loops internally until AP_STARTED or NETWORK_CONNECTED is reached.
void setBlocking(bool blocking);
bool isBlocking() const;

// Auto-restart (default: true)
// When true, the ESP restarts automatically after PORTAL_COMPLETE or PORTAL_TIMEOUT.
// When false, ESPConnect re-enters the state machine with the new configuration.
void setAutoRestart(bool autoRestart);
bool isAutoRestart() const;

// Delay in milliseconds between PORTAL_COMPLETE and the actual restart (default: 1000 ms).
void setRestartDelay(uint32_t delayMs);
uint32_t getRestartDelay() const;

// Maximum time in seconds to wait for a WiFi connection before giving up (default: 20 s).
void setConnectTimeout(uint32_t seconds);
uint32_t getConnectTimeout() const;

// Maximum time in seconds the captive portal stays open before timing out (default: 180 s).
// Only applies when a WiFi SSID is already configured; the portal stays open indefinitely
// when no SSID is known.
void setCaptivePortalTimeout(uint32_t seconds);
uint32_t getCaptivePortalTimeout() const;

// Access the current Config (mutable — changes take effect on the next connection attempt).
Mycila::ESPConnect::Config& getConfig();
const Mycila::ESPConnect::Config& getConfig() const;
void setConfig(Mycila::ESPConnect::Config config);

// NVS persistence (only relevant when using the auto-load/save begin() overload,
// or when managing persistence yourself via the manual-config overload).
void loadConfiguration();                        // load into internal Config
static void loadConfiguration(Config& config);   // load into a provided Config
void saveConfiguration();                        // save internal Config to NVS
static void saveConfiguration(const Config& config);
void clearConfiguration();                       // erase NVS entry and reset Config

// SSID and password used for the captive portal / AP.
const ESPCONNECT_STRING& getAccessPointSSID() const;
const ESPCONNECT_STRING& getAccessPointPassword() const;
```

### State and Mode

```cpp
// Current state
Mycila::ESPConnect::State getState() const;
const char* getStateName() const;
const char* getStateName(State state) const;

// Current network mode (AP / STA / ETH / NONE).
// ETH takes priority over STA when both are connected.
Mycila::ESPConnect::Mode getMode() const;

// True when any interface has a valid IP address.
bool isConnected() const;
```

### Network Information

```cpp
// MAC address of the active interface, or of a specific interface.
ESPCONNECT_STRING getMACAddress() const;
ESPCONNECT_STRING getMACAddress(Mode mode) const;   // Mode::AP, STA, or ETH

// IPv4 address of the active interface, or of a specific interface.
IPAddress getIPAddress() const;
IPAddress getIPAddress(Mode mode) const;

// IPv6 addresses (ESP32 only).
IPAddress getLinkLocalIPv6Address() const;
IPAddress getLinkLocalIPv6Address(Mode mode) const;
IPAddress getGlobalIPv6Address() const;
IPAddress getGlobalIPv6Address(Mode mode) const;

// WiFi-specific
ESPCONNECT_STRING getWiFiSSID() const;           // configured SSID or AP SSID
ESPCONNECT_STRING getWiFiBSSID() const;          // BSSID of connected AP, or "" if not connected
int8_t getWiFiRSSI() const;                      // signal strength in dBm, or -1
int8_t getWiFiSignalQuality() const;             // 0–100 %, or -1
```

### JSON serialization

`toJson()` is available when the Captive Portal is enabled (i.e. `ESPCONNECT_NO_CAPTIVE_PORTAL` is not set):

```cpp
JsonDocument doc;
espConnect.toJson(doc.to<JsonObject>());
serializeJsonPretty(doc, Serial);
```

The JSON object contains:

| Key | Description |
|---|---|
| `ip_address` | Active IP address |
| `ip_address_ap` | AP interface IP |
| `ip_address_sta_v4` | STA IPv4 address |
| `ip_address_eth_v4` | ETH IPv4 address |
| `ip_address_sta_v6_local` | STA link-local IPv6 |
| `ip_address_sta_v6_global` | STA global IPv6 |
| `ip_address_eth_v6_local` | ETH link-local IPv6 |
| `ip_address_eth_v6_global` | ETH global IPv6 |
| `hostname` | Configured hostname |
| `mac_address` | Active interface MAC |
| `mac_address_ap` | AP MAC address |
| `mac_address_sta` | STA MAC address |
| `mac_address_eth` | ETH MAC address |
| `mode` | `"AP"`, `"STA"`, `"ETH"`, or `"NONE"` |
| `state` | Current state name string |
| `wifi_ssid` | Connected / configured SSID |
| `wifi_bssid` | Connected AP BSSID |
| `wifi_rssi` | RSSI in dBm |
| `wifi_signal` | Signal quality 0–100 % |

### State machine

```
NETWORK_DISABLED
  └─ begin() ──────────────────────────────────────────► NETWORK_ENABLED
                                                              │
                           ┌──────────────────────────────────┤
                           │                                  │
                   apMode=true                        apMode=false
                           │                                  │
                           ▼                       wifiSSID configured?
                      AP_STARTING                 YES ◄──────┴──────► NO
                           │                      │                    │
                           ▼                      ▼              (captive portal
                       AP_STARTED           NETWORK_CONNECTING    or idle if
                      (final state)               │            NO_CAPTIVE_PORTAL)
                                          timeout?│connected?         │
                                             ▼    │    ▼              ▼
                                      NETWORK_TIMEOUT  NETWORK_CONNECTED   PORTAL_STARTING
                                             │         (final state)        │
                                             │                              ▼
                                     PORTAL_STARTING               PORTAL_STARTED
                                                                          │
                                                         user submits ────┤──── timeout
                                                              │                      │
                                                              ▼                      ▼
                                                       PORTAL_COMPLETE        PORTAL_TIMEOUT
                                                       (final state)          (final state)

NETWORK_CONNECTED ──── disconnected ──► NETWORK_DISCONNECTED ──► NETWORK_RECONNECTING ──► (reconnects)
```

**Final states** are states in which ESPConnect stays until the application takes action:

- `AP_STARTED` — AP is running. Application can start its server.
- `NETWORK_CONNECTED` — WiFi or Ethernet is connected. Application can start its server.
- `PORTAL_COMPLETE` — User submitted credentials in the portal. With `autoRestart=true` the ESP restarts; with `autoRestart=false` the state machine re-enters `NETWORK_ENABLED`.
- `PORTAL_TIMEOUT` — Portal timed out. With `autoRestart=true` the ESP restarts; with `autoRestart=false` the state machine re-enters `NETWORK_ENABLED`.

### Config struct

```cpp
struct Mycila::ESPConnect::Config {
  ESPCONNECT_STRING hostname;     // mDNS hostname and AP hostname
  ESPCONNECT_STRING wifiBSSID;    // preferred BSSID (useful in mesh networks)
  ESPCONNECT_STRING wifiSSID;     // WiFi SSID to connect to
  ESPCONNECT_STRING wifiPassword; // WiFi password
  bool apMode;                    // force AP mode (ignores wifiSSID/wifiPassword)
  IPConfig ipConfig;              // optional static IP (all-zero = DHCP)
};

struct Mycila::ESPConnect::IPConfig {
  IPAddress ip;      // static IP address (leave as 0.0.0.0 for DHCP)
  IPAddress subnet;  // subnet mask (e.g. 255.255.255.0)
  IPAddress gateway; // default gateway
  IPAddress dns;     // DNS server
};
```

## ESP8266 Specifics

- The dependency `vshymanskyy/Preferences` is required when using the auto-load/save `begin()` overload.
- Ethernet is not supported on ESP8266.
- IPv6 is not supported on ESP8266.

## Ethernet Support

Set `-D ESPCONNECT_ETH_SUPPORT` to add Ethernet support (ESP32 only).

**Behaviour:**

- Ethernet takes precedence over WiFi: `getMode()` returns `ETH` when both are connected.
- Both Ethernet and WiFi can be active simultaneously.
- Ethernet takes precedence over the Captive Portal: if the portal is running and an Ethernet cable is plugged in, the portal is closed automatically.
- Ethernet does _not_ take precedence over AP Mode: if `apMode = true` in the config, Ethernet will not be started.

**SPI-based adapters** (W5500, etc.) are detected automatically when all of `ETH_PHY_SPI_SCK`, `ETH_PHY_SPI_MISO`, `ETH_PHY_SPI_MOSI`, `ETH_PHY_CS`, `ETH_PHY_IRQ`, and `ETH_PHY_RST` are defined. In that case `SPI.begin()` and `ETH.begin()` are called with those pins.

**Hints**:

- If your board requires `ETH_PHY_POWER`, the library powers the pin automatically.
- Add `-D ESPCONNECT_ETH_RESET_ON_START` to pull the power pin LOW for 350 ms before powering it HIGH (required by some boards).

Known **compatibilities**:

| **Board**                                                                                                                        | **Compile** | **Tested** |
| :------------------------------------------------------------------------------------------------------------------------------- | :---------: | :--------: |
| [OLIMEX ESP32-PoE](https://docs.platformio.org/en/stable/boards/espressif32/esp32-poe.html) (esp32-poe)                          |     ✅      |     ✅     |
| [OLIMEX ESP32-GATEWAY](https://docs.platformio.org/en/stable/boards/espressif32/esp32-gateway.html)                              |     ✅      |     ✅     |
| [Wireless-Tag WT32-ETH01 Ethernet Module](https://docs.platformio.org/en/stable/boards/espressif32/wt32-eth01.html) (wt32-eth01) |     ✅      |     ✅     |
| [T-ETH-Lite ESP32 S3](https://github.com/Xinyuan-LilyGO/LilyGO-T-ETH-Series/) (esp32s3box)                                       |     ✅      |     ✅     |
| [USR-ES1 W5500](https://fr.aliexpress.com/item/1005001636214844.html)                                                            |     ✅      |     ✅     |
| [Waveshare ESP32-S3 ETH Board](https://www.waveshare.com/wiki/ESP32-S3-ETH)                                                      |     ✅      |     ✅     |

Example of flags for **wt32-eth01**:

```ini
build_flags =
  -D ESPCONNECT_ETH_SUPPORT
  -D ETH_PHY_ADDR=1
  -D ETH_PHY_POWER=16
```

Example of flags for **T-ETH-Lite ESP32 S3** (SPI W5500):

```ini
build_flags =
  -D ESPCONNECT_ETH_SUPPORT
  -D ETH_PHY_ADDR=1
  -D ETH_PHY_CS=9
  -D ETH_PHY_IRQ=13
  -D ETH_PHY_RST=14
  -D ETH_PHY_SPI_MISO=11
  -D ETH_PHY_SPI_MOSI=12
  -D ETH_PHY_SPI_SCK=10
  ; requires ESP-IDF >= 5
  ; -D ETH_PHY_TYPE=ETH_PHY_W5500
```

Example of flags for **USR-ES1 W5500 with esp32dev** (tested by [@MicSG-dev](https://github.com/mathieucarbou/ESPAsyncWebServer/discussions/36#discussioncomment-9826045)):

```ini
build_flags =
  -D ESPCONNECT_ETH_SUPPORT
  -D ETH_PHY_ADDR=1
  -D ETH_PHY_CS=5
  -D ETH_PHY_IRQ=4
  -D ETH_PHY_RST=14
  -D ETH_PHY_SPI_MISO=19
  -D ETH_PHY_SPI_MOSI=23
  -D ETH_PHY_SPI_SCK=18
  ; requires ESP-IDF >= 5
  ; -D ETH_PHY_TYPE=ETH_PHY_W5500
```

Note: this project makes use of the `ETHClass` library from [Lewis He](https://github.com/Xinyuan-LilyGO/LilyGO-T-ETH-Series/tree/master/lib/ETHClass).

## Logo

You can display a custom logo in the captive portal by registering a `/logo` route **before** calling `begin()`:

```cpp
server.on("/logo", HTTP_GET, [](AsyncWebServerRequest* request) {
  AsyncWebServerResponse* response = request->beginResponse(
    200, "image/png", logo_png_gz_start, logo_png_gz_end - logo_png_gz_start);
  response->addHeader("Content-Encoding", "gzip");
  response->addHeader("Cache-Control", "public, max-age=900");
  request->send(response);
});
```

If no `/logo` handler is registered, the logo area in the portal is simply hidden.

## Captive Portal Detection Endpoints

MycilaESPConnect serves OS-specific detection endpoints so that Android, iOS, Windows, and Linux all automatically open the captive portal UI when a device joins the ESP's access point network.

| **Endpoint** | **OS / Client** | **Action** |
|---|---|---|
| `/connecttest.txt` | Windows | Redirects to `http://logout.net` |
| `/wpad.dat` | All | Returns 404 (no proxy) |
| `/generate_204` | Android | Redirects to AP IP |
| `/redirect` | Generic | Redirects to AP IP |
| `/hotspot-detect.html` | Apple iOS / macOS | Redirects to AP IP |
| `/canonical.html` | Ubuntu / Linux | Redirects to AP IP |
| `/success.txt` | Microsoft | Returns 200 OK |
| `/ncsi.txt` | Microsoft NCSI | Redirects to portal |
| `/startpage` | Generic | Redirects to portal |

Disable all of these endpoints with `-D ESPCONNECT_NO_COMPAT_CP` (saves ~2 KB flash). This may reduce automatic portal detection reliability on some devices.
