/* HID device stub for Arduino compatibility */
#pragma once

#ifdef ESP32_8048S070
// Arduino build - use stub
#include "hid_dev_stub.h"
#else
// ESP-IDF build - use real implementation
#include "../libraries/ble_hid/hid_dev.h"
#endif
