# ESP32-8048S070 Display Adaptation

This document describes the adaptation of the HD2 Macropad project to work with the ESP32-8048S070 display module.

## Hardware Specifications

### ESP32-8048S070 Display Module
- **Display**: 7" 800x480 RGB parallel interface
- **Touch Controller**: GT911 (I2C)
- **MCU**: ESP32-S3 with OPI PSRAM
- **Framework**: Arduino (vs ESP-IDF in original)

### Pin Configuration

#### RGB Display Pins
- **Blue (B0-B4)**: GPIO 15, 7, 6, 5, 4
- **Green (G0-G5)**: GPIO 9, 46, 3, 8, 16, 1
- **Red (R0-R4)**: GPIO 14, 21, 47, 48, 45

#### Control Pins
- **HSYNC**: GPIO 39
- **VSYNC**: GPIO 40
- **PCLK**: GPIO 0
- **DE (Data Enable)**: GPIO 41
- **Backlight**: GPIO 2

#### Touch Controller (GT911)
- **SDA**: GPIO 19
- **SCL**: GPIO 20
- **RST**: GPIO 38
- **INT**: Not connected (-1)

#### Display Timing
- **Resolution**: 800x480
- **Pixel Clock**: 15MHz
- **HSYNC**: Front Porch=40, Pulse=48, Back Porch=40
- **VSYNC**: Front Porch=13, Pulse=3, Back Porch=32

## Files Created

### 1. `src/display_ESP32_8048S070.h`
LovyanGFX configuration for the RGB parallel display. This file defines the LGFX class with proper pin mappings and timing parameters for the 800x480 RGB display.

**Key Features:**
- RGB parallel bus configuration (16-bit data bus)
- PWM backlight control
- Proper timing parameters for 60Hz refresh rate

### 2. `src/touch_ESP32_8048S070.h`
GT911 touch controller driver using the TAMC_GT911 library.

**Key Features:**
- I2C touch interface
- Touch coordinate mapping for 800x480 resolution
- Simple API for touch state and coordinates

### 3. `src/main_esp32_8048s070.cpp`
Arduino-based main application file for the ESP32-8048S070 environment.

**Key Features:**
- LVGL initialization with LovyanGFX
- Touch input handling
- Double buffering for smooth graphics
- Basic test UI (to be extended with full HD2 Macropad UI)

### 4. PlatformIO Environment
Updated `platformio.ini` with new `esp32_8048s070` environment.

## Building and Uploading

### Build for ESP32-8048S070
```bash
pio run -e esp32_8048s070
```

### Upload to ESP32-8048S070
```bash
pio run -e esp32_8048s070 -t upload
```

### Monitor Serial Output
```bash
pio device monitor -e esp32_8048s070
```

### Build Original Project (480x320)
```bash
pio run -e LVGL-320-480
```

## Differences from Original

| Feature | Original (LVGL-320-480) | ESP32-8048S070 |
|---------|------------------------|----------------|
| Framework | ESP-IDF | Arduino |
| Display | 480x320 SPI (ST7796) | 800x480 RGB Parallel |
| Touch | FT6336 (I2C) | GT911 (I2C) |
| Display Library | ESP LCD (native) | LovyanGFX |
| Resolution | 480x320 | 800x480 |
| Interface | QSPI | RGB Parallel |

## Current Status

### ✅ Completed
- [x] Display driver configuration (LovyanGFX)
- [x] Touch driver configuration (GT911)
- [x] PlatformIO environment setup
- [x] Basic Arduino main file
- [x] LVGL initialization
- [x] Source file filtering for environment isolation

### 🚧 TODO
- [ ] Integrate full HD2 Macropad UI (currently shows test screen)
- [ ] Port BLE HID functionality to Arduino framework
- [ ] Port USB HID functionality to Arduino framework
- [ ] Port audio playback functionality
- [ ] Port NVS configuration storage
- [ ] Adapt UI layouts for 800x480 resolution
- [ ] Test all HD2 Macropad features

## Integration Notes

The current implementation provides a basic framework with:
1. Working display output via LovyanGFX
2. Working touch input via GT911
3. LVGL integration with proper buffering
4. Simple test UI to verify functionality

To complete the integration, the following components from the original project need to be ported:
- `ui/ui.c` - Main UI components (may need layout adjustments)
- BLE and USB HID controllers (Arduino-compatible versions)
- Audio player functionality
- Configuration management (NVS or SPIFFS)
- Stratagem system and keymapping

## References

- [ESP32-8048S070 Hardware Wiki](https://www.elecrow.com/wiki/ESP32_Display_7.0-inch_HMI_Arduino_Tutorial.html)
- [LovyanGFX Documentation](https://github.com/lovyan03/LovyanGFX)
- [GT911 Touch Library](https://github.com/TAMCTec/gt911-arduino)
- [Original HD2 Macropad Project](https://github.com/unic8s/hd2_macropad)

## Testing

To verify the setup works:
1. Build and upload the firmware
2. Check serial output for initialization messages
3. Look for test UI with text displayed on screen
4. Touch the screen to verify touch input is working

Expected serial output:
```
HD2 Macropad - ESP32-8048S070 Starting...
Display initialized
Touch initialized
LVGL initialized
LVGL buffer 1 allocated: ...
LVGL buffer 2 allocated: ...
Display driver registered
Input driver registered
Setup complete!
```

## Troubleshooting

### Display Issues
- Check RGB pin connections match the hardware
- Verify pixel clock frequency (15MHz for this display)
- Adjust timing parameters if display shows tearing

### Touch Issues
- Verify I2C pins (SDA=19, SCL=20)
- Check touch reset pin (GPIO 38)
- Monitor serial for touch initialization errors

### Memory Issues
- Display buffers use significant RAM
- Reduce `LVGL_BUFFER_SIZE` if memory allocation fails
- Consider using PSRAM for buffers

### Build Errors
- Ensure LovyanGFX and GT911 libraries are installed
- Check that build_src_filter excludes conflicting files
- Verify ESP32 Arduino core version compatibility
