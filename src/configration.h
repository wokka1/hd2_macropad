#ifndef _CONFIGURATION_H
#define _CONFIGURATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <esp_system.h>
#include <lvgl.h>

// Enable ship module UI support (checkboxes for cooldown reduction)
#define HAS_SHIP_MODULE_UI

esp_err_t initConfig();
uint8_t getConfig(char *key, int8_t defaultValue);
void setConfig(char *key, uint8_t value);
void setDelay(int delay, bool restore);
void setRotation(int rotation, bool restore);
void setBrightness(int brightness, bool restore);
void setMuted(bool muted, bool restore);
void setDebugLogging(bool debug, bool restore);
void setShowCooldowns(bool show, bool restore);
void setConnectivity(uint8_t index, bool restore);
void setKeymap(uint8_t index, bool restore);
void setShipModules(bool restore);
void loadConfig();
esp_err_t openConfig();
void closeConfig();
void resetConfig();
int8_t peekConfig(char *key, int8_t defaultValue);

// Ship module bitmask values for cooldown reduction calculation
// First 8 modules fit in one byte (shipModLo)
enum ShipModuleType
{
    SHIP_LVC = 1,    // Localization Confusion: -50% cooldown
    SHIP_ZBL = 2,    // Zero-G Breaching Logistics: -10% cooldown
    SHIP_HC = 4,     // Hangar Capacity: -10% cooldown
    SHIP_MA = 8,     // Morale Amplifier: -5% cooldown
    SHIP_SRP = 16,   // Superior Replenishment Pack: -10% cooldown
    SHIP_SS = 32,    // Synthetic Supplementation: -10% cooldown
    SHIP_TSU = 64,   // Targeting Software Upgrade: +1s call-in
    SHIP_RLS = 128,  // Rapid Launch System: +3s call-in
    // 9th module stored separately in shipModHi
    SHIP_DT = 1      // Dynamic Tracking: +3s call-in (stored in high byte)
};

#define MAX_SHIP_MODULES 9

// Ship module checkbox mapping
typedef struct
{
    uint8_t value;
    lv_obj_t *checkbox;
    bool isHighByte;  // true if stored in shipModHi key
} shipModule;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif