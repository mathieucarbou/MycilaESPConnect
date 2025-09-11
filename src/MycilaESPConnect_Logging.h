// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2025 Mathieu Carbou
 */
#pragma once

#ifdef ESPCONNECT_NO_LOGGING
  #define LOGD(tag, format, ...)
  #define LOGI(tag, format, ...)
  #define LOGW(tag, format, ...)
  #define LOGE(tag, format, ...)
#elif defined(MYCILA_LOGGER_SUPPORT)
  #include <MycilaLogger.h>
extern Mycila::Logger logger;
  #define LOGD(tag, format, ...) logger.debug(tag, format, ##__VA_ARGS__)
  #define LOGI(tag, format, ...) logger.info(tag, format, ##__VA_ARGS__)
  #define LOGW(tag, format, ...) logger.warn(tag, format, ##__VA_ARGS__)
  #define LOGE(tag, format, ...) logger.error(tag, format, ##__VA_ARGS__)
#elif defined(ESP8266)
  #include <HardwareSerial.h>
  #define LOGD(tag, format, ...)                         \
    {                                                    \
      Serial.printf("%6lu [%s] DEBUG: ", millis(), tag); \
      Serial.printf(format "\n", ##__VA_ARGS__);         \
    }
  #define LOGI(tag, format, ...)                        \
    {                                                   \
      Serial.printf("%6lu [%s] INFO: ", millis(), tag); \
      Serial.printf(format "\n", ##__VA_ARGS__);        \
    }
  #define LOGW(tag, format, ...)                        \
    {                                                   \
      Serial.printf("%6lu [%s] WARN: ", millis(), tag); \
      Serial.printf(format "\n", ##__VA_ARGS__);        \
    }
  #define LOGE(tag, format, ...)                         \
    {                                                    \
      Serial.printf("%6lu [%s] ERROR: ", millis(), tag); \
      Serial.printf(format "\n", ##__VA_ARGS__);         \
    }
#else
  #define LOGD(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__)
  #define LOGI(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)
  #define LOGW(tag, format, ...) ESP_LOGW(tag, format, ##__VA_ARGS__)
  #define LOGE(tag, format, ...) ESP_LOGE(tag, format, ##__VA_ARGS__)
#endif

#define TAG "ESPCONNECT"
