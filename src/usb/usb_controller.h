/*
 * USB HID Controller for HD2 Macropad
 * Uses TinyUSB for USB HID keyboard functionality on ESP32-S3
 */

#ifndef _USB_CONTROLLER_H
#define _USB_CONTROLLER_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the USB HID controller
 *
 * Configures TinyUSB with HID keyboard descriptors and starts the USB device.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t usb_controller_init(void);

/**
 * @brief Deinitialize the USB HID controller
 *
 * Disconnects and cleans up the USB driver.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t usb_controller_deinit(void);

/**
 * @brief Check if USB is connected
 *
 * @return true if USB device is connected and mounted, false otherwise
 */
bool usb_connected(void);

/**
 * @brief Send keyboard HID report
 *
 * @param special_key_mask Modifier keys (Ctrl, Shift, Alt, etc.)
 * @param keyboard_cmd Key code to send
 * @param num_key Number of keys (1 = press, 0 = release)
 */
void usb_keyboard_send(uint8_t special_key_mask, uint8_t keyboard_cmd, uint8_t num_key);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* _USB_CONTROLLER_H */
