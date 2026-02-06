#include <esp_system.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
// #include "i2s_sdcard.h"  // Removed - audio not needed for BLE-only build
#include <lvgl.h>
#include "ui/ui.h"
#include "ui/screens.h"
#include "main.h"
#include <string.h>

const char *TAG_CFG = "Configuration";

// Handle for NVS config
nvs_handle_t nvsConfig;

// Async NVS write queue to prevent blocking BLE during flash writes
typedef struct {
    char key[16];
    uint8_t value;
} nvs_write_msg_t;

static QueueHandle_t nvs_write_queue = NULL;

// Task to process NVS writes asynchronously (low priority)
static void nvs_write_task(void *pvParameters)
{
    nvs_write_msg_t msg;
    nvs_handle_t handle;

    while (1)
    {
        if (xQueueReceive(nvs_write_queue, &msg, portMAX_DELAY) == pdTRUE)
        {
            esp_err_t ret = nvs_open("config", NVS_READWRITE, &handle);
            if (ret == ESP_OK)
            {
                ret = nvs_set_u8(handle, msg.key, msg.value);
                if (ret == ESP_OK)
                {
                    nvs_commit(handle);
                    ESP_LOGD(TAG_CFG, "Async NVS: %s = %d", msg.key, msg.value);
                }
                nvs_close(handle);
            }
            // Small delay between writes
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

extern bool playerMuted;
extern int inputDelay;
extern int screenRotation;
extern uint8_t keymapIndex;
extern uint8_t connectionType;

extern esp_err_t ble_controller_init();
extern esp_err_t ble_controller_deinit();
extern esp_err_t usb_controller_init();
extern esp_err_t usb_controller_deinit();

// Init configuration from NVS
esp_err_t initConfig()
{
    // Initialize NVS.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Create async NVS write queue and task
    nvs_write_queue = xQueueCreate(10, sizeof(nvs_write_msg_t));
    if (nvs_write_queue != NULL)
    {
        // Low priority task (1) to handle NVS writes without blocking BLE
        xTaskCreate(nvs_write_task, "nvs_write", 2048, NULL, 1, NULL);
        ESP_LOGI(TAG_CFG, "Async NVS write task started");
    }

    // SD card and FAT filesystem removed for BLE-only build
    // ESP_ERROR_CHECK(init_sdcard());
    // lv_fs_fatfs_init();

    return ret;
}

// Read configuration value by key
// key - Identifier for configuration value
// defaultValue - Default value which will be returned if the key/value does not exist
uint8_t getConfig(char *key, int8_t defaultValue)
{
    uint8_t value;

    esp_err_t ret = nvs_get_u8(nvsConfig, key, &value);

    switch (ret)
    {
    case ESP_OK:
        ESP_LOGI(TAG_CFG, "%s = %" PRIu8 "\n", key, value);
        return value;
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGI(TAG_CFG, "The value is not initialized yet!\n");
        break;
    default:
        ESP_LOGE(TAG_CFG, "Error (%s) reading!\n", esp_err_to_name(ret));
    }

    return defaultValue;
}

// Write configuration value by key (async - does not block)
// key - Identifier for configuration value
// value - Value which should be stored
void setConfig(char *key, uint8_t value)
{
    if (nvs_write_queue == NULL)
    {
        ESP_LOGE(TAG_CFG, "NVS write queue not initialized");
        return;
    }

    nvs_write_msg_t msg;
    strncpy(msg.key, key, sizeof(msg.key) - 1);
    msg.key[sizeof(msg.key) - 1] = '\0';
    msg.value = value;

    // Post to queue (non-blocking) - BLE won't be disrupted
    if (xQueueSend(nvs_write_queue, &msg, 0) != pdTRUE)
    {
        ESP_LOGW(TAG_CFG, "NVS write queue full, dropping: %s", key);
    }
}

// Write the HID input delay to configuration
void setDelay(int delay, bool restore)
{
    inputDelay = delay;

    char *textDelay = (char *)malloc(7 * sizeof(char));
    sprintf(textDelay, "%d %s", delay, " ms");

    lv_label_set_text(objects.lbl_delay, (void *)textDelay);

    if (restore)
    {
        lv_slider_set_value(objects.sld_delay, (int)(delay / 10), LV_ANIM_OFF);
    }
    else
    {
        setConfig("delay", delay);
    }
}

// Write the display rotation to configuration
void setRotation(int rotation, bool restore)
{
    bool restart = !restore && screenRotation != rotation;

    screenRotation = rotation;

    if (restore)
    {
        if (screenRotation == LV_DISP_ROT_270)
        {
            lv_obj_add_state(objects.chb_flip, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(objects.chb_flip, LV_STATE_CHECKED);
        }
    }
    else
    {
        setConfig("rotation", screenRotation);
    }

    // playbackSound(SND_SWITCH);  // Audio removed for BLE-only build

    if (restart)
    {
        esp_restart();
    }
}

// Write the display brightness to configuration
void setBrightness(int brightness, bool restore)
{
    dimScreen(brightness);

    char *textBrightness = (char *)malloc(5 * sizeof(char));
    sprintf(textBrightness, "%d %s", brightness, " %");

    lv_label_set_text(objects.lbl_brightness, (void *)textBrightness);

    if (restore)
    {
        lv_slider_set_value(objects.sld_brightness, (int)(brightness / 10), LV_ANIM_OFF);
    }
    else
    {
        setConfig("brightness", brightness);
    }
}

// Write the sound mute state to configuration
void setMuted(bool muted, bool restore)
{
    playerMuted = muted;

    if (restore)
    {
        if (muted)
        {
            lv_obj_add_state(objects.chb_mute, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(objects.chb_mute, LV_STATE_CHECKED);
        }
    }
    else
    {
        setConfig("muted", playerMuted ? 1 : 0);
    }

    // playbackSound(SND_SWITCH);  // Audio removed for BLE-only build
}

// Write the debug logging state to configuration
void setDebugLogging(bool debug, bool restore)
{
    extern bool debugLogging;
    debugLogging = debug;

    if (restore)
    {
        if (debug)
        {
            lv_obj_add_state(objects.chb_debug_1, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(objects.chb_debug_1, LV_STATE_CHECKED);
        }
    }
    else
    {
        setConfig("debug", debugLogging ? 1 : 0);
    }
}

// Write the cooldown display state to configuration
void setShowCooldowns(bool show, bool restore)
{
    extern bool showCooldowns;
    showCooldowns = show;

    if (restore)
    {
        if (show)
        {
            lv_obj_add_state(objects.chb_cooldowns_1, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(objects.chb_cooldowns_1, LV_STATE_CHECKED);
        }
    }
    else
    {
        setConfig("cooldowns", showCooldowns ? 1 : 0);
    }
}

// Write the keymap assignment to configuration
void setConnectivity(uint8_t index, bool restore)
{
    if (restore)
    {
        lv_dropdown_set_selected(objects.dd_connectivity, index - 1);
    }
    else
    {
        setConfig("connectivity", index);
    }

    // playbackSound(SND_SWITCH);  // Audio removed for BLE-only build

    // Deinit previous connection type (USB stays running always)
    switch (connectionType)
    {
    case CT_BLUETOOTH:
        // Deinit Bluetooth controller
        ble_controller_deinit();
        break;
    case CT_USB:
        // USB stays initialized - nothing to deinit
        break;
    default:
        break;
    }

    connectionType = index;

    // Init new connection type (USB is already running from startup)
    switch (connectionType)
    {
    case CT_BLUETOOTH:
        // Init Bluetooth controller
        ble_controller_init();
        break;
    case CT_USB:
        // USB is already initialized at startup - nothing to do
        break;
    default:
        break;
    }

    vTaskDelay(500 / portTICK_PERIOD_MS);

    updateConnection();
}

// Write the keymap assignment to configuration
void setKeymap(uint8_t index, bool restore)
{
    keymapIndex = index;

    if (restore)
    {
        lv_dropdown_set_selected(objects.dd_keymap, index);
    }
    else
    {
        setConfig("keymap", index);
    }

    // playbackSound(SND_SWITCH);  // Audio removed for BLE-only build
}

esp_err_t openConfig()
{
    esp_err_t ret = nvs_open("config", NVS_READWRITE, &nvsConfig);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG_CFG, "Error (%s) opening NVS handle!\n", esp_err_to_name(ret));
        return ret;
    }

    return ret;
}

void closeConfig()
{
    nvs_close(nvsConfig);
}

// Load complete configuration from NVS
void loadConfig()
{
    if (openConfig() != ESP_OK)
    {
        return;
    }

    uint8_t delay = getConfig("delay", 100);
    setDelay(delay, true);

    uint8_t rotation = getConfig("rotation", 100);
    setRotation(rotation, true);

    uint8_t screen_brightness = getConfig("brightness", 50);
    setBrightness(screen_brightness, true);

    uint8_t sound_muted = getConfig("muted", 0);
    setMuted(sound_muted == 1, true);

    uint8_t debug_enabled = getConfig("debug", 0);
    setDebugLogging(debug_enabled == 1, true);

    uint8_t cooldowns_enabled = getConfig("cooldowns", 1);  // Default ON
    setShowCooldowns(cooldowns_enabled == 1, true);

    uint8_t connectivity_index = getConfig("connectivity", 0);
    setConnectivity(connectivity_index, true);

    uint8_t keymap_index = getConfig("keymap", 0);
    setKeymap(keymap_index, true);

    closeConfig();
}

// Load single configuration of a key/value from NVS
int8_t peekConfig(char *key, int8_t defaultValue)
{
    if (openConfig() != ESP_OK)
    {
        return defaultValue;
    }

    int8_t value = getConfig(key, defaultValue);

    closeConfig();

    return value;
}

// Clear all stored configuration in NVS and write it to default values
void resetConfig()
{
    if (openConfig() != ESP_OK)
    {
        return;
    }

    nvs_erase_all(nvsConfig);

    closeConfig();

    setDelay(100, true);
    setBrightness(50, true);
    setMuted(0, true);
    setKeymap(0, true);
    setRotation(LV_DISP_ROT_90, true);
}