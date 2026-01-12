/*
 * HD2 Macropad - ESP32-8048S070 Arduino Main File
 * 7" 800x480 RGB Display with GT911 Touch
 *
 * This is the Arduino framework adaptation of the HD2 Macropad
 * for the ESP32-8048S070 display module.
 */

#ifdef ESP32_8048S070

#include <Arduino.h>
#include <lvgl.h>
#include <Preferences.h>
#include "display_ESP32_8048S070.h"
#include "touch_ESP32_8048S070.h"
#include "usb_hid_keyboard.h"

// UI includes - eez-flow.h has C++ templates, so include it outside extern "C"
#include "ui/eez-flow.h"

// UI includes (C library)
extern "C" {
    #include "ui/vars.h"
    #include "ui/screens.h"
    #include "ui/actions.h"
    #include "ui/images.h"
    #include "ui/ui_events.h"
    #include "ui/ui_post.h"
    #include "keymaps.h"
    #include "version.h"
    #include "main.h"

    // Forward declare ui functions
    void ui_init();
    void ui_tick();
}

// Screen dimensions
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480

// Buffer size (using 1/10 of screen for better performance)
#define LVGL_BUFFER_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10)

// Tag for logging
static const char *TAG = "HD2-8048S070";

// Debug logging macros (only work when CDC is enabled)
#if ARDUINO_USB_CDC_ON_BOOT
    #define DEBUG_PRINT(x) Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
    #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
    #define DEBUG_FLUSH() Serial.flush()
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(...)
    #define DEBUG_FLUSH()
#endif

// Display and LVGL objects
static LGFX *lcd = nullptr;  // Create as pointer to avoid early initialization
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf;
static lv_color_t *disp_draw_buf2;
static lv_disp_drv_t disp_drv;
static lv_disp_t *disp;
static lv_indev_drv_t indev_drv;
static lv_indev_t *indev;

// Preferences for configuration storage (Arduino alternative to NVS)
Preferences preferences;

// Global variables from main.h
uint8_t connectionType = CT_NONE;
bool lvglReady = false;
uint8_t stratagemCode[8];
uint8_t stratagemMask;
bool soundPlayback = false;
char *soundFile;
bool playerMuted = false;
int inputDelay = 100;
int screenRotation = 0; // 0 = no rotation for landscape 800x480
int currentBrightness = 255;

// Arduino-compatible stub functions for modules not yet ported
// These replace ESP-IDF specific implementations

// Configuration functions (Arduino Preferences wrapper)
extern "C" {

esp_err_t initConfig() {
    preferences.begin("hd2_config", false);
    DEBUG_PRINTLN("Config initialized (Arduino Preferences)");
    return ESP_OK;
}

int8_t getConfig(char *key, int8_t defaultValue) {
    return preferences.getChar(key, defaultValue);
}

void setConfig(char *key, uint8_t value) {
    preferences.putChar(key, value);
}

int8_t peekConfig(char *key, int8_t defaultValue) {
    return preferences.getChar(key, defaultValue);
}

esp_err_t openConfig() {
    return ESP_OK; // Already opened in initConfig
}

void closeConfig() {
    // Preferences auto-commits, no explicit close needed
}

void resetConfig() {
    preferences.clear();
    DEBUG_PRINTLN("Config reset");
}

void loadConfig() {
    // Load all configuration values
    inputDelay = preferences.getInt("delay", 100);
    playerMuted = preferences.getBool("muted", false);
    currentBrightness = preferences.getInt("brightness", 255);
    screenRotation = preferences.getInt("rotation", 0);

    DEBUG_PRINTF("Config loaded: delay=%d, muted=%d, brightness=%d\n",
                  inputDelay, playerMuted, currentBrightness);
}

void setDelay(int delay, bool restore) {
    inputDelay = delay;
    if (restore) {
        preferences.putInt("delay", delay);
    }
}

void setRotation(int rotation, bool restore) {
    screenRotation = rotation;
    if (restore) {
        preferences.putInt("rotation", rotation);
    }
    // TODO: Apply rotation to display
}

void setBrightness(int brightness, bool restore) {
    currentBrightness = brightness;
    if (lcd) lcd->setBrightness(brightness);
    if (restore) {
        preferences.putInt("brightness", brightness);
    }
}

void setMuted(bool muted, bool restore) {
    playerMuted = muted;
    if (restore) {
        preferences.putBool("muted", muted);
    }
}

void setConnectivity(uint8_t index, bool restore) {
    // TODO: Implement BLE/USB switching
    if (restore) {
        preferences.putUChar("connectivity", index);
    }
}

void setKeymap(uint8_t index, bool restore) {
    // TODO: Implement keymap switching
    if (restore) {
        preferences.putUChar("keymap", index);
    }
}

// Audio playback stub (to be implemented with Arduino audio library)
void playbackSound(char *path) {
    soundPlayback = true;
    soundFile = path;
    DEBUG_PRINTF("Sound requested: %s (not yet implemented)\n", path);
    // TODO: Implement with ESP32-audioI2S or similar Arduino library
}

// HID functions stubs (to be implemented with BLE/USB HID libraries)
void setStratagemCode(uint8_t sequence[8], uint8_t mask, bool plain) {
    // Validate connection type (matching original implementation)
    switch (connectionType)
    {
        case CT_USB:
            if (!usb_connected()) {
                return;
            }
            break;
        default:
            return;
    }

    // Clear the buffer first
    for (uint8_t c = 0; c < 8; c++)
    {
        stratagemCode[c] = 0;
    }

    for (uint8_t c = 0; c < 8; c++)
    {
        if (sequence[c] > 0)
        {
            uint8_t rawCode = sequence[c];
            stratagemCode[c] = plain ? rawCode : LookupKeycode(rawCode);
        }
        else
        {
            break;
        }
    }

    stratagemMask = mask;
}

// Update functions stubs
void updateConnection() {
    // TODO: Update UI with connection status
    DEBUG_PRINTLN("Connection update (stub)");
}

void updateBatteryInfo() {
    // TODO: Read battery voltage and update UI
    // ESP32-8048S070 might not have battery monitoring
}

void dimScreen(int brightness) {
    if (lcd) lcd->setBrightness(brightness);
}

} // extern "C"

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    if (!lcd) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    lcd->startWrite();
    lcd->setAddrWindow(area->x1, area->y1, w, h);
    lcd->pushPixels((uint16_t *)&color_p->full, w * h, true);
    lcd->endWrite();

    lv_disp_flush_ready(disp_drv);
}

/* Read the touchpad */
void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    if (touch_has_signal())
    {
        if (touch_touched())
        {
            data->state = LV_INDEV_STATE_PR;
            // Invert coordinates: bottom-right was triggering top-left
            data->point.x = SCREEN_WIDTH - touch_last_x();
            data->point.y = SCREEN_HEIGHT - touch_last_y();
        }
        else
        {
            data->state = LV_INDEV_STATE_REL;
        }
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }
}

// Task for execution of EEZ Flows (LVGL timer callback)
void flow_tick_task(lv_timer_t *timer)
{
    ui_tick();
}

// HID input task - processes keyboard commands from stratagemCode buffer
void hid_input_task(void *pvParameter)
{
    const int INPUT_CHECK_DELAY = 50; // ms

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(INPUT_CHECK_DELAY));

        // Check if there's a command in the buffer
        if (stratagemCode[0] > 0)
        {
            uint8_t cmdIndex = 0;

            // Check connection type
            if (connectionType != CT_USB)
            {
                // Clear buffer and skip if not USB
                memset(stratagemCode, 0, sizeof(stratagemCode));
                continue;
            }

            // Send modifier keys (mask) first
            usb_keyboard_send(stratagemMask, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(inputDelay));

            // Loop through command sequence from buffer
            while (stratagemCode[cmdIndex] > 0 && cmdIndex < 8)
            {
                // Press key defined by the keycode
                usb_keyboard_send(stratagemMask, stratagemCode[cmdIndex], 1);
                vTaskDelay(pdMS_TO_TICKS(inputDelay));

                // Release key
                usb_keyboard_send(0, 0, 0);
                vTaskDelay(pdMS_TO_TICKS(inputDelay));

                cmdIndex++;
            }

            // Release all modifier keys
            usb_keyboard_send(0, 0, 0);

            // Clear the command buffer
            memset(stratagemCode, 0, sizeof(stratagemCode));
            stratagemMask = 0;
        }
    }
}

void setup()
{
#if ARDUINO_USB_CDC_ON_BOOT
    // Initialize USB and Serial for debugging (only when CDC is enabled)
    USB.begin();
    Serial.begin(115200);
    delay(1000); // Give serial time to initialize
    DEBUG_PRINTLN("\n\n========================================");
    DEBUG_PRINTLN("HD2 Macropad - ESP32-8048S070");
    DEBUG_PRINTF("Version: %s\n", SW_VER);
    DEBUG_PRINTLN("========================================\n");
    DEBUG_FLUSH();
#endif
    // When CDC_ON_BOOT=0, USB will be initialized by Keyboard.begin() later

    // Initialize configuration
    DEBUG_PRINTLN("Step 1: Initializing config...");
    DEBUG_FLUSH();
    initConfig();
    DEBUG_PRINTLN("Step 2: Loading config...");
    DEBUG_FLUSH();
    loadConfig();
    DEBUG_FLUSH();

    // Initialize LCD
    DEBUG_PRINTLN("Step 3: Creating display object...");
    DEBUG_FLUSH();
    lcd = new LGFX();
    DEBUG_PRINTLN("Step 4: Configuring display...");
    DEBUG_FLUSH();
    lcd->begin();  // Configure the display hardware
    DEBUG_PRINTLN("Step 5: Initializing display...");
    DEBUG_FLUSH();
    lcd->init();
    DEBUG_PRINTLN("Step 6: Setting rotation...");
    DEBUG_FLUSH();
    lcd->setRotation(screenRotation);
    DEBUG_PRINTLN("Step 7: Filling screen black...");
    DEBUG_FLUSH();
    lcd->fillScreen(TFT_BLACK);

    DEBUG_PRINTLN("Display initialized");
    DEBUG_FLUSH();

    // Initialize touch
    DEBUG_PRINTLN("Initializing touch...");
    if (!touch_init())
    {
        DEBUG_PRINTLN("ERROR: Touch initialization failed!");
    }
    else
    {
        DEBUG_PRINTLN("Touch initialized");
    }

    // Initialize LVGL
    DEBUG_PRINTLN("Initializing LVGL...");
    lv_init();

    // Allocate LVGL draw buffers
    disp_draw_buf = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * LVGL_BUFFER_SIZE,
                                                     MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!disp_draw_buf)
    {
        DEBUG_PRINTLN("ERROR: LVGL buffer 1 allocation failed!");
        while(1) delay(1000);
    }
    else
    {
        DEBUG_PRINTF("LVGL buffer 1 allocated: %d bytes\n", sizeof(lv_color_t) * LVGL_BUFFER_SIZE);
    }

    disp_draw_buf2 = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * LVGL_BUFFER_SIZE,
                                                      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!disp_draw_buf2)
    {
        DEBUG_PRINTLN("LVGL buffer 2 allocation failed! Using single buffer.");
        lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, LVGL_BUFFER_SIZE);
    }
    else
    {
        DEBUG_PRINTF("LVGL buffer 2 allocated: %d bytes\n", sizeof(lv_color_t) * LVGL_BUFFER_SIZE);
        lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, disp_draw_buf2, LVGL_BUFFER_SIZE);
    }

    // Initialize display driver
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    disp = lv_disp_drv_register(&disp_drv);

    DEBUG_PRINTLN("Display driver registered");

    // Initialize input device driver
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    indev = lv_indev_drv_register(&indev_drv);

    DEBUG_PRINTLN("Input driver registered");

    // Initialize USB HID Keyboard (moved after UI to speed up boot)
    // Will initialize later in setup
    connectionType = CT_USB;  // Set connection type to USB

    // Initialize the HD2 Macropad UI
    DEBUG_PRINTLN("Initializing UI...");
    ui_init();

    // UI post processing
    ui_post();

    DEBUG_PRINTLN("UI initialized");

    // Debug: Print some object addresses to verify UI was created
    DEBUG_PRINTF("DEBUG: objects.game = 0x%08X\n", (uint32_t)objects.game);
    DEBUG_PRINTF("DEBUG: objects.btn_reinforce = 0x%08X\n", (uint32_t)objects.btn_reinforce);
    DEBUG_PRINTF("DEBUG: objects.custom_stratagem1 = 0x%08X\n", (uint32_t)objects.custom_stratagem1);

    // Small delay before turning on backlight
    delay(200);

    // Turn on display backlight
    lcd->setBrightness(currentBrightness);
    DEBUG_PRINTLN("Backlight on");

    // Mark LVGL as ready
    lvglReady = true;

    // Play intro sound
    playbackSound(SND_INTRO);

    // Update software version in UI
    char softwareVersion[12];
    strcpy(softwareVersion, SW_VER);
    lv_label_set_text(objects.lbl_version, softwareVersion);

    // Update UI elements
    updateConnection();

    // Note: These functions are defined in ui/actions.c and may need the UI to be fully loaded
    // updateStratagemSelection();
    // updatePresets();

    // Setup timer for EEZ Flow ui tick
    lv_timer_t *flowTickTimer = lv_timer_create(flow_tick_task, 10, NULL);
    flowTickTimer->repeat_count = -1;

    // Initialize USB HID Keyboard (done last to avoid slowing boot)
    DEBUG_PRINTLN("Initializing USB HID...");
    usb_hid_init();
    DEBUG_PRINTLN("USB HID initialized");

    // Setup HID input task (async)
    DEBUG_PRINTLN("Creating HID input task...");
    xTaskCreatePinnedToCore(&hid_input_task, "hid_input_task", 4096, NULL, 5, NULL, 0);
    DEBUG_PRINTLN("HID input task created");

    DEBUG_PRINTLN("\n========================================");
    DEBUG_PRINTLN("Setup complete!");
    DEBUG_PRINTLN("HD2 Macropad ready for use");
    DEBUG_PRINTLN("========================================\n");
}

void loop()
{
    // Update LVGL tick - critical for timers and animations
    static uint32_t last_tick = 0;
    uint32_t now = millis();
    lv_tick_inc(now - last_tick);
    last_tick = now;

    lv_timer_handler(); // Handle LVGL tasks
    delay(5);

    // Handle sound playback (when implemented)
    if (soundPlayback && !playerMuted)
    {
        soundPlayback = false;
        // TODO: Actually play the sound
    }
}

#endif // ESP32_8048S070
