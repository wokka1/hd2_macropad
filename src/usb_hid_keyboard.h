/*
 * USB HID Keyboard for ESP32-S3 (Arduino Framework)
 * Implements USB HID keyboard functionality using Arduino USB library
 */

#ifndef _USB_HID_KEYBOARD_H
#define _USB_HID_KEYBOARD_H

#ifdef ESP32_8048S070

#include <Arduino.h>
#include "USB.h"
#include "USBHIDKeyboard.h"

// USB HID Keyboard instance
extern USBHIDKeyboard Keyboard;

// Initialize USB HID
bool usb_hid_init();

// Check if USB is connected
bool usb_connected();

// Send keyboard command
// special_key_mask: modifier keys (Ctrl, Alt, Shift, etc.)
// keyboard_cmd: HID keycode to send
// num_key: if > 0, press the key; if 0, release all keys
void usb_keyboard_send(uint8_t special_key_mask, uint8_t keyboard_cmd, uint8_t num_key);

// HID modifier key masks (match the original implementation)
#define HID_KEY_MODIFIER_LEFT_CTRL   0x01
#define HID_KEY_MODIFIER_LEFT_SHIFT  0x02
#define HID_KEY_MODIFIER_LEFT_ALT    0x04
#define HID_KEY_MODIFIER_LEFT_GUI    0x08
#define HID_KEY_MODIFIER_RIGHT_CTRL  0x10
#define HID_KEY_MODIFIER_RIGHT_SHIFT 0x20
#define HID_KEY_MODIFIER_RIGHT_ALT   0x40
#define HID_KEY_MODIFIER_RIGHT_GUI   0x80

#endif // ESP32_8048S070

#endif // _USB_HID_KEYBOARD_H
