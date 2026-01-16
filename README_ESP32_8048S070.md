# ESP32-8048S070 Display Adaptation

This document describes the adaptation of the HD2 Macropad project to work with the ESP32-8048S070 display module.

## Hardware Specifications

### ESP32-8048S070 Display Module
- **Display**: 7" 800x480 RGB parallel interface
- **Touch Controller**: GT911 (I2C)
- **MCU**: ESP32-S3 with OPI PSRAM
- **Framework**: ESP-IDF

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
- **INT**: GPIO 18

#### Display Timing
- **Resolution**: 800x480
- **Pixel Clock**: 14MHz
- **HSYNC**: Front Porch=40, Pulse=48, Back Porch=40
- **VSYNC**: Front Porch=1, Pulse=31, Back Porch=13

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

### 3. `src/main.c`
ESP-IDF main application file for the ESP32-8048S070 environment.

**Key Features:**
- LVGL initialization with ESP LCD RGB panel
- Touch input handling via GT911
- Double buffering for smooth graphics
- Full HD2 Macropad UI integrated

### 4. PlatformIO Environment
Updated `platformio.ini` with new `esp32_8048s070_espidf` environment.

## Building and Uploading

### Build for ESP32-8048S070
```bash
pio run -e esp32_8048s070_espidf
```

### Upload to ESP32-8048S070
```bash
pio run -e esp32_8048s070_espidf -t upload
```

### Monitor Serial Output
```bash
pio device monitor -e esp32_8048s070_espidf
```

### Build Original Project (480x320)
```bash
pio run -e LVGL-320-480
```

## Differences from Original

| Feature | Original (LVGL-320-480) | ESP32-8048S070 |
|---------|------------------------|----------------|
| Framework | ESP-IDF | ESP-IDF |
| Display | 480x320 SPI (ST7796) | 800x480 RGB Parallel |
| Touch | FT6336 (I2C) | GT911 (I2C) |
| Display Library | ESP LCD (native) | ESP LCD RGB Panel |
| Resolution | 480x320 | 800x480 |
| Interface | QSPI | RGB Parallel |

## Current Status

### ✅ Completed
- [x] Display driver configuration (ESP LCD RGB Panel)
- [x] Touch driver configuration (GT911)
- [x] PlatformIO environment setup
- [x] ESP-IDF main application
- [x] LVGL initialization
- [x] Source file filtering for environment isolation
- [x] Full HD2 Macropad UI integrated
- [x] BLE HID functionality
- [x] NVS configuration storage
- [x] UI layouts adapted for 800x480 resolution

### 🚧 TODO
- [ ] USB HID functionality (not supported on this board - BT only)
- [ ] Audio playback functionality (limited by 4MB flash)
- [ ] Comprehensive testing of all features

## Integration Notes

The current implementation provides a fully functional HD2 Macropad with:
1. Working display output via ESP LCD RGB Panel
2. Working touch input via GT911
3. LVGL integration with proper buffering
4. Full HD2 Macropad UI with stratagem selection
5. BLE HID keyboard functionality
6. NVS configuration storage

### Hardware Limitations (Basic 4MB Board)
- No USB HID capability on this board variant
- Audio disabled due to flash constraints
- WiFi disabled due to flash constraints

The advanced 16MB version of this board may support additional features.

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
- Verify pixel clock frequency (14MHz for this display)
- Adjust timing parameters if display shows tearing

### Touch Issues
- Verify I2C pins (SDA=19, SCL=20)
- Check touch reset pin (GPIO 38) and interrupt pin (GPIO 18)
- Monitor serial for touch initialization errors

### Memory Issues
- Display buffers use significant RAM
- Reduce `LVGL_BUFFER_SIZE` if memory allocation fails
- Consider using PSRAM for buffers

### Build Errors
- Check that build_src_filter excludes conflicting files
- Verify ESP-IDF version compatibility
- Ensure all required components are available
