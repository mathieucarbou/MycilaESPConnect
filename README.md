# MycilaESPConnect

[![License: MIT](https://img.shields.io/badge/License-GPL%203.0-yellow.svg)](https://opensource.org/licenses/gpl-3-0)
[![Continuous Integration](https://github.com/mathieucarbou/MycilaESPConnect/actions/workflows/ci.yml/badge.svg)](https://github.com/mathieucarbou/MycilaESPConnect/actions/workflows/ci.yml)
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/mathieucarbou/library/MycilaESPConnect.svg)](https://registry.platformio.org/libraries/mathieucarbou/MycilaESPConnect)

Simple & Easy Network Manager for ESP32 with WiFi, Ethernet and Captive Portal support

This fork is based on [https://github.com/ayushsharma82/ESPConnect](https://github.com/ayushsharma82/ESPConnect).
I highly recommend looking at all OSS projects (and products) from [@ayushsharma82](https://github.com/ayushsharma82).
He is making great Arduino libraries.

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
- **Ready for Arduino 3 (ESP-IDF 5.1)**
- **Experimental ESP8266 support:** it compiles but I am not able to correctly test it. Feedbacks and testers are welcomed.

## Ethernet Support

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
| [OLIMEX ESP32-PoE](https://docs.platformio.org/en/stable/boards/espressif32/esp32-poe.html) (esp32-poe)                          |     ✅      |     ❌     |
| [Wireless-Tag WT32-ETH01 Ethernet Module](https://docs.platformio.org/en/stable/boards/espressif32/wt32-eth01.html) (wt32-eth01) |     ✅      |     ✅     |
| [T-ETH-Lite ESP32 S3](https://github.com/Xinyuan-LilyGO/LilyGO-T-ETH-Series/) (esp32s3box)                                       |     ✅      |     ✅     |

Flags for **wt32-eth01**:

```cpp
  -D ESPCONNECT_ETH_SUPPORT
  -D ETH_PHY_ADDR=1
  -D ETH_PHY_POWER=16
```

Flags for **T-ETH-Lite ESP32 S3**:

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

Note: this project is making use of the `ETHClass` library from [Lewis He](https://github.com/Xinyuan-LilyGO/LilyGO-T-ETH-Series/tree/master/lib/ETHClass)
