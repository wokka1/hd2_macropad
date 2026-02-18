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
// Values must match upstream for compatibility with stratagems.h shipModules field
enum ModuleType
{
    SHIP_LVC = 1,    // Liquid-Ventilated Cockpit: -50% cooldown (Eagle strats)
    SHIP_ZBL = 2,    // Zero-G Breaching Logistics: -10% cooldown (Orbital strats)
    SHIP_HC = 4,     // Hand Carts: -10% cooldown (Backpack strats)
    SHIP_MA = 8,     // Morale Augmentation: -5% cooldown (All strats)
    SHIP_SRP = 16,   // Streamlined Request Process: -10% cooldown (Support Weapon strats)
    SHIP_SS = 32,    // Synthetic Supplementation: -10% cooldown (Sentry, Emplacement, Resupply)
    SHIP_TSU = 64,   // Targeting Software Upgrade: +1s call-in (Orbital strats)
    SHIP_RLS = 128,  // Rapid Launch System: +3s call-in (Emplacement strats)
    SHIP_DT = 256    // Dynamic Tracking: +3s call-in (Sentry strats)
};

#define MAX_SHIP_MODULES 9

// Ship module checkbox mapping for NVS save/restore
// Uses two 8-bit keys (shipModLo/shipModHi) to avoid 16-bit NVS issues
typedef struct
{
    uint16_t value;
    lv_obj_t *checkbox;
    bool isHighByte;  // true if stored in shipModHi key
} shipModule;

// Ship module details for cooldown/call-in calculations
typedef struct
{
    uint16_t value;
    lv_obj_t *checkbox;
    uint8_t shift;       // Bit position for checking stratagem compatibility
    double cooldown;     // Cooldown reduction factor (e.g., 0.5 = 50% reduction)
    double callin;       // Additional call-in time in seconds
} shipModuleDetails;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif