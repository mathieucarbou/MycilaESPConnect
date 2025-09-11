// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2025 Mathieu Carbou
 */
#pragma once

#ifdef ESP8266
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
  #include <esp_mac.h>
#endif

#if __has_include(<MacAddress.h>)
  #include <MacAddress.h>
#else
  #include "./backport/MacAddress.h"
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
