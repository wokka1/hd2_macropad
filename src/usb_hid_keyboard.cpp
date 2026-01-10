/*
 * USB HID Keyboard for ESP32-S3 (Arduino Framework)
 */

#ifdef ESP32_8048S070

#include "usb_hid_keyboard.h"

// Global USB HID Keyboard instance
USBHIDKeyboard Keyboard;

bool usb_hid_init()
{
    // Initialize USB with HID Keyboard
    Keyboard.begin();
    USB.begin();

    Serial.println("USB HID Keyboard initialized");
    return true;
}

bool usb_connected()
{
    // Check if USB is connected/mounted
    // On ESP32-S3, USB is always "connected" when initialized
    // You can add more sophisticated checking if needed
    return true;
}

void usb_keyboard_send(uint8_t special_key_mask, uint8_t keyboard_cmd, uint8_t num_key)
{
    if (num_key > 0)
    {
        // Press key with modifiers
        if (special_key_mask != 0)
        {
            // Send modifiers first
            if (special_key_mask & HID_KEY_MODIFIER_LEFT_CTRL)   Keyboard.press(KEY_LEFT_CTRL);
            if (special_key_mask & HID_KEY_MODIFIER_LEFT_SHIFT)  Keyboard.press(KEY_LEFT_SHIFT);
            if (special_key_mask & HID_KEY_MODIFIER_LEFT_ALT)    Keyboard.press(KEY_LEFT_ALT);
            if (special_key_mask & HID_KEY_MODIFIER_LEFT_GUI)    Keyboard.press(KEY_LEFT_GUI);
            if (special_key_mask & HID_KEY_MODIFIER_RIGHT_CTRL)  Keyboard.press(KEY_RIGHT_CTRL);
            if (special_key_mask & HID_KEY_MODIFIER_RIGHT_SHIFT) Keyboard.press(KEY_RIGHT_SHIFT);
            if (special_key_mask & HID_KEY_MODIFIER_RIGHT_ALT)   Keyboard.press(KEY_RIGHT_ALT);
            if (special_key_mask & HID_KEY_MODIFIER_RIGHT_GUI)   Keyboard.press(KEY_RIGHT_GUI);
        }

        // Press the main key
        Keyboard.press(keyboard_cmd);
    }
    else
    {
        // Release all keys
        Keyboard.releaseAll();
    }
}

#endif // ESP32_8048S070
