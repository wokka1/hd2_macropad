/* ESP logging compatibility header for Arduino */
#pragma once

#ifdef ESP32_8048S070
// Arduino build - map to Serial
#include <Arduino.h>

#define ESP_LOGE(tag, format, ...) Serial.printf("[E][%s] " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) Serial.printf("[W][%s] " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, format, ...) Serial.printf("[I][%s] " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) Serial.printf("[D][%s] " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGV(tag, format, ...) Serial.printf("[V][%s] " format "\n", tag, ##__VA_ARGS__)

#else
// ESP-IDF build - use real esp_log
#include <esp_log.h>
#endif
