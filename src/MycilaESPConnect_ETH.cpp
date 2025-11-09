// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2025 Mathieu Carbou
 */
#ifdef ESPCONNECT_ETH_SUPPORT
  #include "MycilaESPConnect.h"
  #include "MycilaESPConnect_Includes.h"
  #include "MycilaESPConnect_Logging.h"

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
