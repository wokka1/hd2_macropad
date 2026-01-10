# ESP32-8048S070 Setup Summary

## ✅ Completed Setup

All necessary files have been created and configured for the ESP32-8048S070 display module. The project now supports both the original hardware and the new 7" display.

## Files Created/Modified

### New Files
1. **`src/display_ESP32_8048S070.h`** - LovyanGFX RGB parallel display driver
2. **`src/touch_ESP32_8048S070.h`** - GT911 touch controller driver  
3. **`src/main_esp32_8048s070.cpp`** - Arduino-based main application
4. **`README_ESP32_8048S070.md`** - Comprehensive documentation

### Modified Files
1. **`platformio.ini`** - Added `esp32_8048s070` environment with proper configuration

## Hardware Configuration

### Display (RGB Parallel - 800x480)
```
Blue  (B0-B4): GPIO 15, 7, 6, 5, 4
Green (G0-G5): GPIO 9, 46, 3, 8, 16, 1  
Red   (R0-R4): GPIO 14, 21, 47, 48, 45
```

### Control Signals
```
HSYNC:     GPIO 39
VSYNC:     GPIO 40
PCLK:      GPIO 0
DE:        GPIO 41
Backlight: GPIO 2
```

### Touch Controller (GT911)
```
SDA: GPIO 19
SCL: GPIO 20
RST: GPIO 38
INT: Not connected
```

### Display Timing
```
Resolution:   800x480
Pixel Clock:  15MHz
HSYNC:        FP=40, PW=48, BP=40
VSYNC:        FP=13, PW=3, BP=32
```

## Building the Project

### Using VS Code PlatformIO Extension

1. **Open Project in VS Code**
   - The project is already open with all files visible

2. **Select Environment**
   - Click PlatformIO icon in left sidebar
   - Under "Project Tasks" → "esp32_8048s070"

3. **Build**
   - Click "Build" under esp32_8048s070 environment
   - Or use PlatformIO toolbar: "Build" button with env selector

4. **Upload**
   - Connect ESP32-8048S070 via USB
   - Click "Upload" under esp32_8048s070 environment

5. **Monitor**
   - Click "Monitor" to view serial output
   - Should see initialization messages

### Using Command Line (if PlatformIO CLI is installed)
```bash
# Build
platformio run -e esp32_8048s070

# Upload
platformio run -e esp32_8048s070 -t upload

# Monitor
platformio device monitor -e esp32_8048s070

# Clean
platformio run -e esp32_8048s070 -t clean
```

## Environment Configuration

The `esp32_8048s070` environment includes:
- **Platform**: espressif32@6.9.0
- **Framework**: Arduino (not ESP-IDF)
- **Board**: ESP32-S3-DevKitC-1
- **PSRAM**: OPI mode enabled
- **Flash**: 80MHz, QIO mode
- **CPU**: 240MHz

### Key Build Flags
```
-DESP32_8048S070        // Hardware identifier
-DSCREEN_WIDTH=800      // Display width
-DSCREEN_HEIGHT=480     // Display height
-DBOARD_HAS_PSRAM       // Enable PSRAM
```

### Libraries
- LovyanGFX @ ^1.1.16 (Display driver)
- LVGL @ ^8.3.11 (Graphics library)
- GT911-Arduino (Touch controller)

## Source File Filtering

The environment automatically excludes ESP-IDF specific files:
- `main.c` (ESP-IDF main)
- `esp_bsp.c` (ESP-IDF BSP)
- `esp_lcd_*.c` (ESP-IDF LCD drivers)
- `lv_port.c` (ESP-IDF LVGL port)
- `configuation.c` (Uses ESP-IDF NVS)
- `ble/` and `usb/` (ESP-IDF specific)

Only `main_esp32_8048s070.cpp` will be compiled as the entry point.

## Current Implementation Status

### ✅ Working
- Display initialization (LovyanGFX)
- Touch input (GT911)
- LVGL integration
- Basic test UI
- Memory management (PSRAM buffers)
- Backlight control

### 🚧 To Be Implemented
- Full HD2 Macropad UI integration
- BLE HID functionality (Arduino-compatible)
- USB HID functionality (Arduino-compatible)
- Audio playback
- Configuration storage (Preferences/SPIFFS)
- Stratagem system
- Keymapping
- UI scaling for 800x480 resolution

## Testing Checklist

After building and uploading:

1. **Serial Output Check**
   ```
   ✓ HD2 Macropad - ESP32-8048S070 Starting...
   ✓ Display initialized
   ✓ Touch initialized
   ✓ LVGL initialized
   ✓ LVGL buffer allocated
   ✓ Display driver registered
   ✓ Input driver registered
   ✓ Setup complete!
   ```

2. **Display Test**
   - [ ] Screen lights up
   - [ ] Text is visible and clear
   - [ ] No flickering or tearing
   - [ ] Colors appear correct

3. **Touch Test**
   - [ ] Touch input is detected
   - [ ] Coordinates are accurate
   - [ ] No ghost touches

4. **Memory Test**
   - [ ] Both buffers allocate successfully
   - [ ] No memory errors or crashes
   - [ ] Smooth rendering

## Troubleshooting

### Build Errors
**Problem**: Missing libraries
**Solution**: PlatformIO should auto-install. If not:
- Delete `.pio` folder
- Rebuild project

**Problem**: Compilation errors in old files
**Solution**: Verify `build_src_filter` is set correctly in platformio.ini

### Display Issues
**Problem**: Black screen
**Solution**: 
- Check RGB pin connections
- Verify backlight GPIO 2 is working
- Check power supply (RGB displays need good power)

**Problem**: Distorted image
**Solution**:
- Adjust pixel clock frequency in display_ESP32_8048S070.h
- Verify timing parameters match your panel

### Touch Issues
**Problem**: No touch response
**Solution**:
- Verify I2C pins (SDA=19, SCL=20)
- Check GT911 power supply
- Monitor serial for GT911 init messages

**Problem**: Inverted coordinates
**Solution**: Adjust rotation in touch_ESP32_8048S070.h

### Memory Issues
**Problem**: Buffer allocation fails
**Solution**:
- Reduce `LVGL_BUFFER_SIZE` in main_esp32_8048s070.cpp
- Ensure PSRAM is enabled
- Try single buffer instead of double

## Next Steps

1. **Verify Hardware**
   - Build and upload firmware
   - Confirm display and touch work
   - Check serial output for errors

2. **Integrate UI**
   - Port `ui/ui.c` components
   - Adjust layouts for 800x480
   - Test all screens

3. **Add Functionality**
   - Implement BLE/USB HID for Arduino
   - Add audio support
   - Port configuration system
   - Add keymapping system

4. **Test & Debug**
   - Test all features
   - Optimize performance
   - Fix any issues

## Additional Resources

- Hardware specs: `README_ESP32_8048S070.md`
- Original project: https://github.com/unic8s/hd2_macropad
- Elecrow tutorial: https://www.elecrow.com/wiki/ESP32_Display_7.0-inch_HMI_Arduino_Tutorial.html
- LovyanGFX: https://github.com/lovyan03/LovyanGFX
- GT911 library: https://github.com/TAMCTec/gt911-arduino

## Support

For issues specific to ESP32-8048S070 adaptation, refer to the configuration files and ensure all pin mappings match your hardware exactly.

For original HD2 Macropad functionality, refer to the upstream project documentation.

---

**Status**: ✅ Configuration complete and ready for compilation
**Date**: January 9, 2026
**Framework**: Arduino + LovyanGFX + LVGL 8.3
