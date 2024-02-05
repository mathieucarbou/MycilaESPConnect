# MycilaESPConnect

[![License: MIT](https://img.shields.io/badge/License-GPL%203.0-yellow.svg)](https://opensource.org/licenses/gpl-3-0)
[![Continuous Integration](https://github.com/mathieucarbou/MycilaESPConnect/actions/workflows/ci.yml/badge.svg)](https://github.com/mathieucarbou/MycilaESPConnect/actions/workflows/ci.yml)
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/mathieucarbou/library/MycilaESPConnect.svg)](https://registry.platformio.org/libraries/mathieucarbou/MycilaESPConnect)

---

Simple & Easy WiFi Manager with Captive Portal for ESP32

A simplistic approach to a WiFi Manager on ESP32 MCUs. Comes with captive portal to configure modules without any hassle.

---

> This fork is based on [https://github.com/ayushsharma82/ESPConnect](https://github.com/ayushsharma82/ESPConnect).
> I highly recommend looking at all OSS projects (and products) from [@ayushsharma82](https://github.com/ayushsharma82).
> He is making great Arduino libraries.

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

## Ethernet Support

Compile with `-D CONFIG_ETH_ENABLED` ([ref](https://community.platformio.org/t/please-help-me-fix-error-wifi-h-when-build-project-ethernet-testing-use-esp32-eth01/37573/3))

For wt32-eth01, alo use the following flags:

```cpp
  -D ESPCONNECT_ETH_RESET_ON_START
  -D ETH_PHY_TYPE=ETH_PHY_LAN8720
  -D ETH_PHY_ADDR=1
  -D ETH_PHY_MDC=23
  -D ETH_PHY_MDIO=18
  -D ETH_CLK_MODE=ETH_CLOCK_GPIO0_IN
  -D ETH_PHY_POWER=16
```

In your application, if Ethernet is allowed, call `ESPConnect.allowEthernet()`.

- Ethernet takes precedence over WiFi, but you can have both connected at the same time
- Ethernet takes precedence over Captive Portal: if it is running and you connect an Ethernet cable, the Captive Portal will be closed
- Ethernet _does not_ take precedence over AP Mode: if AP mode is configured, then Ethernet won't be started even if a cable is connected
