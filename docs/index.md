# MycilaESPConnect

[![Latest Release](https://img.shields.io/github/release/mathieucarbou/MycilaESPConnect.svg)](https://GitHub.com/mathieucarbou/MycilaESPConnect/releases/)
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/mathieucarbou/library/MycilaESPConnect.svg)](https://registry.platformio.org/libraries/mathieucarbou/MycilaESPConnect)

[![GPLv3 license](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0.txt)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT) (with `-D ESPCONNECT_NO_CAPTIVE_PORTAL`)

[![Contributor Covenant](https://img.shields.io/badge/Contributor%20Covenant-2.1-4baaaa.svg)](code_of_conduct.md)

[![Build](https://github.com/mathieucarbou/MycilaESPConnect/actions/workflows/ci.yml/badge.svg)](https://github.com/mathieucarbou/MycilaESPConnect/actions/workflows/ci.yml)
[![GitHub latest commit](https://badgen.net/github/last-commit/mathieucarbou/MycilaESPConnect)](https://GitHub.com/mathieucarbou/MycilaESPConnect/commit/)
[![Gitpod Ready-to-Code](https://img.shields.io/badge/Gitpod-Ready--to--Code-blue?logo=gitpod)](https://gitpod.io/#https://github.com/mathieucarbou/MycilaESPConnect)

Simple & Easy Network Manager for ESP32 with WiFi, Ethernet and Captive Portal support

[![](https://mathieu.carbou.me/MycilaESPConnect/screenshot.png)](https://mathieu.carbou.me/MycilaESPConnect/screenshot.png)

This library is based on the UI from [https://github.com/ayushsharma82/ESPConnect](https://github.com/ayushsharma82/ESPConnect) (and this part falls under GPL v3).
I highly recommend looking at all OSS projects (and products) from [@ayushsharma82](https://github.com/ayushsharma82).
He is making great Arduino libraries.

- [Changes](#changes)
- [Usage](#usage)
  - [API](#api)
  - [Blocking mode](#blocking-mode)
  - [Non-blocking mode](#non-blocking-mode)
  - [Use an external configuration system](#use-an-external-configuration-system)
  - [ESP8266 Specifics](#esp8266-specifics)
  - [Ethernet Support](#ethernet-support)
  - [Logo](#logo)
  - [mDNS](#mdns)
  - [Compile Flags](#compile-flags)

## Changes

- **Logo**: user is responsible to provide a logo at this path: `/logo`
- **AP Mode**: a new choice is added to the captive portal so that the user can remain in AP mode
- **Network State Machine**: a better state machine is implemented to handle switching between Captive Portal, AP Mode and STA mode
- **New API**: API has been completely rewritten
- **Callback**: Listen to Network State changes
- **Blocking and Non-blocking modes**: ESPConnect can be configured to loop and wait for the user to complete the Captive Portal steps, or the app can continue working in the background and the Captive Portal will be launched as needed.
- **Flexible Configuration:** ESPConnect can either control the configuration persistence for you or let you do it
- **mDNS / DNS Support**
- **Ethernet support**
- **Ready for Arduino 3 (ESP-IDF >= 5.1)**
- **ESP8266 support** (except for Ethernet)
- **Supports Static IP configuration**

## Usage

### API

- `espConnect.setAutoRestart(bool)`: will automatically restart the ESP after the captive portal times out, or after the captive portal has been answered by te user
- `espConnect.setBlocking(bool)`: will block the execution of the program in the begin code to handle the connect. If false, the setup code will continue in the background and the network setup will be done in the background from the main loop.
- `espConnect.listen()`: register a callback for all ESPConnect events

2 flavors of `begin()` methods:

1. `espConnect.begin("hostname", "ssid", "password")` / `espConnect.begin("hostname", "ssid")`
2. `espConnect.begin("ssid", "password", Mycila::ESPConnect::Config)` where config is `{.hostname = ..., .wifiSSID = ..., .wifiPassword = ..., .apMode = ...}`

The first flavors will automatically handle the persistance of user choices and reload them at startup.

The second choice let the user handle the load/save of the configuration.

Please have a look at the self-documented API for the other methods and teh examples.

### Blocking mode

```cpp
  AsyncWebServer server(80);
  Mycila::ESPConnect espConnect(server);

  espConnect.listen([](__unused Mycila::ESPConnect::State previous, __unused Mycila::ESPConnect::State state) {
    // ...
  });

  espConnect.setAutoRestart(true);
  espConnect.setBlocking(true);
  espConnect.begin("arduino", "Captive Portal SSID");
  Serial.println("ESPConnect completed!");
```

### Non-blocking mode

```cpp
void setup() {
  AsyncWebServer server(80);
  Mycila::ESPConnect espConnect(server);

  espConnect.listen([](__unused Mycila::ESPConnect::State previous, __unused Mycila::ESPConnect::State state) {
    // ...
  });

  espConnect.setAutoRestart(true);
  espConnect.setBlocking(false);
  espConnect.begin("arduino", "Captive Portal SSID");
  Serial.println("ESPConnect started!");
}

void loop() {
  espConnect.loop();
}
```

### Set static IP

```cpp
Mycila::ESPConnect::IPConfig ipConfig;

ipConfig.ip.fromString("192.168.125.99");
ipConfig.gateway.fromString("192.168.125.1");
ipConfig.subnet.fromString("255.255.255.0");
ipConfig.dns.fromString("192.168.125.1");

espConnect.setIPConfig(ipConfig);
```

### Use an external configuration system

```cpp
  AsyncWebServer server(80);
  Mycila::ESPConnect espConnect(server);

  espConnect.listen([](__unused Mycila::ESPConnect::State previous, __unused Mycila::ESPConnect::State state) {
    switch (state) {
      case Mycila::ESPConnect::State::PORTAL_COMPLETE:
        bool apMode = espConnect.hasConfiguredAPMode();
        std::string wifiSSID = espConnect.getConfiguredWiFiSSID();
        std::string wifiPassword = espConnect.getConfiguredWiFiPassword();
        if (apMode) {
          Serial.println("====> Captive Portal: Access Point configured");
        } else {
          Serial.println("====> Captive Portal: WiFi configured");
        }
        saveConfig(wifiSSID, wifiPassword, apMode);
        break;

      default:
        break;
    }
  });

  espConnect.setAutoRestart(true);
  espConnect.setBlocking(true);

  // load config from external system
  Mycila::ESPConnect::Config config = {
    .hostname = "arduino",
    .wifiSSID = ...,
    .wifiPassword = ...,
    .apMode = ...
  };

  espConnect.begin("Captive Portal SSID", "", config);
```

### ESP8266 Specifics

The dependency `vshymanskyy/Preferences` is required when using the auto-load avd auto-save feature.

### Ethernet Support

Set `-D ESPCONNECT_ETH_SUPPORT` to add Ethernet support.

- Ethernet takes precedence over WiFi, but you can have both connected at the same time
- Ethernet takes precedence over Captive Portal: if it is running and you connect an Ethernet cable, the Captive Portal will be closed
- Ethernet _does not_ take precedence over AP Mode: if AP mode is configured, then Ethernet won't be started even if a cable is connected

**Hints**:

- If your ethernet card requires to set `ETH_PHY_POWER`, the library will automatically power the pin.

- If you need to reset the pin before powering it up, use `ESPCONNECT_ETH_RESET_ON_START`

Known **compatibilities**:

| **Board**                                                                                                                        | **Compile** | **Tested** |
| :------------------------------------------------------------------------------------------------------------------------------- | :---------: | :--------: |
| [OLIMEX ESP32-PoE](https://docs.platformio.org/en/stable/boards/espressif32/esp32-poe.html) (esp32-poe)                          |     ✅      |     ✅     |
| [OLIMEX ESP32-GATEWAY](https://docs.platformio.org/en/stable/boards/espressif32/esp32-gateway.html)                              |     ✅      |     ✅     |
| [Wireless-Tag WT32-ETH01 Ethernet Module](https://docs.platformio.org/en/stable/boards/espressif32/wt32-eth01.html) (wt32-eth01) |     ✅      |     ✅     |
| [T-ETH-Lite ESP32 S3](https://github.com/Xinyuan-LilyGO/LilyGO-T-ETH-Series/) (esp32s3box)                                       |     ✅      |     ✅     |
| [USR-ES1 W5500](https://fr.aliexpress.com/item/1005001636214844.html)                                                            |     ✅      |     ✅     |

Example of flags for **wt32-eth01**:

```cpp
  -D ESPCONNECT_ETH_SUPPORT
  -D ETH_PHY_ADDR=1
  -D ETH_PHY_POWER=16
```

Example of flags for **T-ETH-Lite ESP32 S3**:

```cpp
  -D ESPCONNECT_ETH_SUPPORT
  -D ETH_PHY_ADDR=1
  -D ETH_PHY_CS=9
  -D ETH_PHY_IRQ=13
  -D ETH_PHY_RST=14
  -D ETH_PHY_SPI_MISO=11
  -D ETH_PHY_SPI_MOSI=12
  -D ETH_PHY_SPI_SCK=10
  ; can only be activated with ESP-IDF >= 5
  ; -D ETH_PHY_TYPE=ETH_PHY_W5500
```

Example of flags for **USR-ES1 W5500 with esp32dev** (tested by [@MicSG-dev](https://github.com/mathieucarbou/ESPAsyncWebServer/discussions/36#discussioncomment-9826045)):

```cpp
  -D ESPCONNECT_ETH_SUPPORT
  -D ETH_PHY_ADDR=1
  -D ETH_PHY_CS=5
  -D ETH_PHY_IRQ=4
  -D ETH_PHY_RST=14
  -D ETH_PHY_SPI_MISO=19
  -D ETH_PHY_SPI_MOSI=23
  -D ETH_PHY_SPI_SCK=18
  ; can only be activated with ESP-IDF >= 5
  ; -D ETH_PHY_TYPE=ETH_PHY_W5500
```

Note: this project is making use of the `ETHClass` library from [Lewis He](https://github.com/Xinyuan-LilyGO/LilyGO-T-ETH-Series/tree/master/lib/ETHClass)

### Logo

You can customize the logo by providing a web handler for `/logo`:

```c++
  server.on("/logo", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse(200, "image/png", logo_png_gz_start, logo_png_gz_end - logo_png_gz_start);
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Cache-Control", "public, max-age=900");
    request->send(response);
  });
```

If not provided, the logo won't appear in the Captive Portal.

### mDNS

mDNS takes quite a lot of space in flash (about 25KB).
You can disable it by setting `-D ESPCONNECT_NO_MDNS`.

### Compile Flags

- `-D ESPCONNECT_NO_MDNS`: disable mDNS
- `-D ESPCONNECT_DEBUG`: enable debug logs on ESP8266
- `-D ESPCONNECT_NO_CAPTIVE_PORTAL`: disable Captive Portal and ESPAsyncWebServer dependency
- `-D ESPCONNECT_NO_STD_STRING`: use Arduino `String` instead of `std::string`
- `-D ESPCONNECT_NO_LOGGING`: disable logging
