# ESP32-8048S070-espidf Branch Notes

## Branch Overview
This branch is based on ESP32-8048S070 and configured for ESP-IDF framework with Bluetooth HID support, targeting a 4MB flash constraint.

## Changes Made

### 1. Removed Components
- **TinyUSB components** - Removed entire `components/` directory (USB HID not needed)
- **Audio player library** - Removed `libraries/audio_player/`
- **USB source files** - Removed `src/usb/` directory and USB HID keyboard files
- **Extra partition files** - Kept only `partitions/4MB_app.csv`
- **Documentation** - Removed README_ESP32_8048S070.md, SETUP_SUMMARY.md, CODE_OF_CONDUCT.md, SECURITY.md
- **Utility directories** - Removed `sdcard/`, `utils/`, `screens/`
- **Old configs** - Removed old sdkconfig files
- **Arduino-specific files** - Removed `src/main_esp32_8048s070.cpp` and `src/test_minimal.cpp`

### 2. Retained Components
- **eez/** - EEZ Studio UI framework files
- **partitions/4MB_app.csv** - Partition table for 4MB flash
- **libraries/ble_hid/** - BLE HID profile library
- **libraries/eez-framework/** - EEZ framework
- **libraries/lvgl/** - LVGL graphics library
- **src/ble/** - BLE controller implementation
- **src/ui/** - UI source files
- All ESP-IDF specific display and touch drivers

### 3. platformio.ini Configuration
- Single environment: `esp32_8048s070_espidf`
- Framework: ESP-IDF (not Arduino)
- Board: esp32-8048s070
- Flash: 4MB with QIO mode
- PSRAM: OPI mode enabled
- Build flags optimized for size (-Os, -Wl,--gc-sections)
- **Removed all JTAG references** from previous attempts
- BLE enabled with BLE-only mode
- Upload speed: 921600 baud
- Source filter: Includes BLE files, excludes .cpp files

### 4. Bluetooth Implementation
Based on the working BLE implementation from `bluetooth-testing` and `main` branches:
- **BLE HID Library**: Complete HID profile implementation in `libraries/ble_hid/`
  - esp_hidd_prf_api.c/h - HID profile API
  - hid_dev.c/h - HID device implementation
  - hid_device_le_prf.c - HID LE profile
  - hidd_le_prf_int.h - Internal HID profile definitions

- **BLE Controller**: `src/ble/ble_controller.c/h`
  - Device name: "HD2 Macropad BT"
  - HID keyboard support
  - Bonding/pairing support
  - Connection status tracking
  - Key: `ble_controller_init()`, `ble_keyboard_send()`, `ble_connected()`

## Next Steps

### To minimize flash usage:
1. Review and optimize LVGL configuration in `src/lv_conf.h`
2. Disable unused LVGL features
3. Consider reducing font sizes or removing unused fonts
4. Review EEZ UI assets and remove unused images
5. Update `sdkconfig.defaults` to:
   - Enable BLE-only mode
   - Disable unused ESP-IDF components
   - Optimize Bluetooth stack memory
   - Reduce log levels for production

### Build Configuration Recommendations:
- Consider using `CONFIG_COMPILER_OPTIMIZATION_SIZE=y`
- Enable `CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_DISABLE=y`
- Disable unused networking features
- Minimize Bluetooth security features if not needed
- Use minimal logging (ERROR level only)

### Testing Plan:
1. Build and verify flash size fits within 4MB partition
2. Test BLE pairing and connection
3. Test keyboard input functionality
4. Test UI rendering and touch input
5. Monitor heap usage and optimize if needed

## BLE Integration
The BLE controller needs to be initialized in `main.c` and integrated with the UI layer for connection status display. The UI should show BLE connection status using the `ble_connected()` function.

## Size Optimization Tips
- Use `pio run -t size` to analyze binary size
- Check `pio run -v` output for component sizes
- Consider stripping debug symbols in release builds
- Use `CONFIG_LOG_DEFAULT_LEVEL_ERROR` to reduce string storage
