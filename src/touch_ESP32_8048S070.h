/*
 * ESP32-8048S070 Touch Configuration for GT911
 * I2C Touch Driver
 */

#pragma once

#ifdef ESP32_8048S070

#include <Wire.h>
#include <TAMC_GT911.h>

// Touch Configuration
#define TOUCH_GT911_SDA     19
#define TOUCH_GT911_SCL     20
#define TOUCH_GT911_INT     -1
#define TOUCH_GT911_RST     38
#define TOUCH_GT911_ROTATION ROTATION_NORMAL

// Touch map configuration
#define TOUCH_MAP_X1        0
#define TOUCH_MAP_X2        800
#define TOUCH_MAP_Y1        0
#define TOUCH_MAP_Y2        480

// Global touch instance
TAMC_GT911 *ts = nullptr;

// Initialize touch driver
bool touch_init(void)
{
    // Initialize I2C for touch
    Wire.begin(TOUCH_GT911_SDA, TOUCH_GT911_SCL);
    
    // Create GT911 touch instance
    ts = new TAMC_GT911(TOUCH_GT911_SDA, TOUCH_GT911_SCL, TOUCH_GT911_INT, TOUCH_GT911_RST, max(TOUCH_MAP_X1, TOUCH_MAP_X2), max(TOUCH_MAP_Y1, TOUCH_MAP_Y2));
    
    if (ts == nullptr) {
        return false;
    }
    
    // Initialize the touch driver
    ts->begin();
    ts->setRotation(TOUCH_GT911_ROTATION);
    
    return true;
}

// Read touch data (returns true if touched)
bool touch_has_signal(void)
{
    if (ts == nullptr) {
        return false;
    }
    
    ts->read();
    return ts->isTouched;
}

// Get touch coordinates
bool touch_touched(void)
{
    if (ts == nullptr) {
        return false;
    }
    
    return ts->isTouched;
}

// Get touch X coordinate
int16_t touch_last_x(void)
{
    if (ts == nullptr || !ts->isTouched) {
        return 0;
    }
    
    return ts->points[0].x;
}

// Get touch Y coordinate
int16_t touch_last_y(void)
{
    if (ts == nullptr || !ts->isTouched) {
        return 0;
    }
    
    return ts->points[0].y;
}

#endif // ESP32_8048S070
