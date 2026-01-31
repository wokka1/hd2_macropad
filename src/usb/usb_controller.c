/*
 * USB HID Controller for HD2 Macropad
 * Uses TinyUSB for USB HID keyboard functionality on ESP32-S3
 */

#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "tinyusb.h"
#include "class/hid/hid_device.h"
#include "usb_controller.h"

static const char *TAG = "USB Controller";

/************* TinyUSB Descriptors ****************/

/*
 * HID Report Descriptor - Standard keyboard
 */
static const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD()
};

/*
 * String Descriptor - Device identification strings
 */
static const char *string_descriptor[] = {
    (const char[]){0x09, 0x04},  // 0: Language ID (English)
    "HD2 Macropad",              // 1: Manufacturer
    "HD2 Macropad USB",          // 2: Product
    "123456",                    // 3: Serial Number
};

/*
 * Configuration Descriptor - Single HID interface
 */
#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

static const uint8_t configuration_descriptor[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_KEYBOARD, sizeof(hid_report_descriptor), 0x81, 8, 10),
};

/************* TinyUSB Callbacks ****************/

/*
 * Invoked when received GET_HID_REPORT_DESCRIPTOR
 * Returns pointer to HID report descriptor
 */
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void) instance;
    return hid_report_descriptor;
}

/*
 * Invoked when received GET_REPORT control request
 * Application must fill buffer report's content and return its length.
 * Return zero will cause the stack to STALL request
 */
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
    return 0;
}

/*
 * Invoked when received SET_REPORT control request or
 * received data on OUT endpoint ( Report ID = 0, Type = 0 )
 */
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;
}

/************* Public API ****************/

esp_err_t usb_controller_init(void)
{
    ESP_LOGI(TAG, "Initializing USB HID controller");

    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,  // Use default device descriptor
        .string_descriptor = string_descriptor,
        .string_descriptor_count = sizeof(string_descriptor) / sizeof(string_descriptor[0]),
        .external_phy = false,
        .configuration_descriptor = configuration_descriptor,
    };

    esp_err_t ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install TinyUSB driver: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "USB HID controller initialized successfully");
    return ESP_OK;
}

esp_err_t usb_controller_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing USB HID controller");

    // TinyUSB doesn't have a clean uninstall in ESP-IDF currently
    // Just log that we're deinitializing
    tud_disconnect();

    return ESP_OK;
}

bool usb_connected(void)
{
    return tud_mounted();
}

void usb_keyboard_send(uint8_t special_key_mask, uint8_t keyboard_cmd, uint8_t num_key)
{
    if (!tud_mounted()) {
        return;
    }

    // Wait until HID is ready
    if (!tud_hid_ready()) {
        return;
    }

    uint8_t keycode[6] = {0};

    if (num_key > 0 && keyboard_cmd != 0) {
        keycode[0] = keyboard_cmd;
    }

    // Send keyboard report
    // modifier, reserved, keycode[6]
    tud_hid_keyboard_report(0, special_key_mask, keycode);

    // Small delay to ensure report is sent
    vTaskDelay(pdMS_TO_TICKS(1));
}
