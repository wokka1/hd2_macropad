# CrowPanel Advance 7" Configuration Guide

This document details the hardware differences between the **CrowPanel ESP32 HMI 7.0-inch Display (Basic)** and **CrowPanel Advance 7"** boards, and the corresponding software configuration required.

## Product Links

### CrowPanel ESP32 HMI 7.0-inch Display (Basic)
- **Product Page:** https://www.elecrow.com/esp32-display-7-inch-hmi-display-rgb-tft-lcd-touch-screen-support-lvgl.html
- **Wiki:** https://www.elecrow.com/pub/wiki/esp32-display-702727-intelligent-touch-screen-wi-fi26ble-800480-hmi-display.html

### CrowPanel Advance 7" HMI ESP32 AI Display
- **Product Page:** https://www.elecrow.com/crowpanel-advance-7-hmi-esp32-ai-display-800x480-ai-ips-touch-screen.html
- **Wiki:** https://www.elecrow.com/pub/wiki/ESP32_Display-7.0_inch%28Advance_Series%29wiki.html
- **Factory Source Code:** https://github.com/Elecrow-RD/CrowPanel-Advance-7-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-800x480

## Hardware Overview

| Feature | CrowPanel Basic 7" | CrowPanel Advance 7" |
|---------|-------------------|----------------------|
| **ESP32 Module** | ESP32-S3-WROOM-1-N4R8 | ESP32-S3-WROOM-1-N16R8 |
| **CPU** | Dual-core LX7 @ 240MHz | Dual-core LX7 @ 240MHz |
| **Flash** | 4MB | 16MB |
| **PSRAM** | 8MB | 8MB |
| **SRAM** | 512KB | 512KB |
| **Display Size** | 800x480 (7.0") | 800x480 (7.0") |
| **Panel Type** | TN | IPS (178° viewing angle) |
| **Display Driver IC** | EK9716BD3 + EK73002ACGB | ST7277 |
| **Touch Controller** | GT911 (Capacitive) | GT911 (Capacitive) |
| **Backlight Control** | PWM (GPIO 2) | I2C (STC8H1K28 @ 0x30) |
| **WiFi** | 2.4GHz 802.11 b/g/n | 2.4GHz 802.11 b/g/n |
| **Bluetooth** | BT 5.0 + BLE | BT 5.0 + BLE |
| **Audio** | Speaker output (I2S) | Microphone + Speaker |
| **Expansion** | TF Card, UART, I2C, GPIO | TF Card, UART, I2C, GPIO, Wireless modules |

## Pin Configuration Comparison

### Display Control Pins

| Signal | CrowPanel Basic | CrowPanel Advance |
|--------|-----------------|-------------------|
| HSYNC | GPIO 39 | GPIO 40 |
| VSYNC | GPIO 40 | GPIO 41 |
| DE | GPIO 41 | GPIO 42 |
| PCLK | GPIO 0 | GPIO 39 |

### RGB Data Pins (16-bit RGB565)

| Signal | CrowPanel Basic | CrowPanel Advance |
|--------|-----------------|-------------------|
| B0 (DATA0) | GPIO 15 | GPIO 21 |
| B1 (DATA1) | GPIO 7 | GPIO 47 |
| B2 (DATA2) | GPIO 6 | GPIO 48 |
| B3 (DATA3) | GPIO 5 | GPIO 45 |
| B4 (DATA4) | GPIO 4 | GPIO 38 |
| G0 (DATA5) | GPIO 9 | GPIO 9 |
| G1 (DATA6) | GPIO 46 | GPIO 10 |
| G2 (DATA7) | GPIO 3 | GPIO 11 |
| G3 (DATA8) | GPIO 8 | GPIO 12 |
| G4 (DATA9) | GPIO 16 | GPIO 13 |
| G5 (DATA10) | GPIO 1 | GPIO 14 |
| R0 (DATA11) | GPIO 14 | GPIO 7 |
| R1 (DATA12) | GPIO 21 | GPIO 17 |
| R2 (DATA13) | GPIO 47 | GPIO 18 |
| R3 (DATA14) | GPIO 48 | GPIO 3 |
| R4 (DATA15) | GPIO 45 | GPIO 46 |

### Touch Controller (GT911) I2C Pins

| Signal | CrowPanel Basic | CrowPanel Advance |
|--------|-----------------|-------------------|
| SDA | GPIO 19 | GPIO 15 |
| SCL | GPIO 20 | GPIO 16 |
| RST | GPIO 38 | Not used (-1) |
| INT | GPIO 18 | Not used (-1) |

### I2C Devices on CrowPanel Advance

| Address | Device | Function |
|---------|--------|----------|
| 0x30 | STC8H1K28 | Backlight/buzzer control |
| 0x51 | EEPROM | Storage |
| 0x5D | GT911 | Touch controller |

**STC8H1K28 Protocol (write single byte to 0x30):**
- `0` = Maximum brightness
- `1-244` = Dimming levels (1=brightest dim, 244=dimmest)
- `245` = Backlight off
- `246` = Buzzer on
- `247` = Buzzer off

## Display Timing Configuration

### CrowPanel Basic 7"

```c
.pclk_hz = 14000000,           // 14 MHz
.hsync_pulse_width = 1,
.hsync_back_porch = 40,
.hsync_front_porch = 48,
.vsync_pulse_width = 1,
.vsync_back_porch = 40,
.vsync_front_porch = 13,
.pclk_active_neg = false,
```

### CrowPanel Advance 7"

```c
.pclk_hz = 21000000,           // 21 MHz
.hsync_pulse_width = 4,
.hsync_back_porch = 8,
.hsync_front_porch = 8,
.vsync_pulse_width = 4,
.vsync_back_porch = 8,
.vsync_front_porch = 8,
.pclk_active_neg = true,       // CRITICAL: Must be true
```

**Source:** Official Elecrow factory code:
`factory_sourcecode/V1.2/HMI-bigInch7/LovyanGFX_Driver.h`

## Backlight Control

### CrowPanel Basic - PWM

```c
#define BACKLIGHT_USE_I2C   0
#define PIN_NUM_BK_LIGHT    2

// Uses LEDC PWM for dimming (0-100%)
ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty_cycle);
```

### CrowPanel Advance - I2C (STC8H1K28)

```c
#define BACKLIGHT_USE_I2C   1
#define BACKLIGHT_I2C_ADDR  0x30
#define BACKLIGHT_MAX       0    // 0 = maximum brightness
#define BACKLIGHT_OFF       245  // 245 = off
```

> **Note:** According to the Elecrow wiki, CrowPanel Advance versions 1.0-1.2 use GPIO-based backlight control, while version 1.3+ uses I2C via STC8H1K28. However, our testing on a V1.2 board found I2C backlight control working at address 0x30. Your mileage may vary depending on board revision.

## Display Offset Fix (Bounce Buffer)

Both boards require SPIRAM optimizations to prevent CPU flash access from interfering with RGB panel DMA timing. The CrowPanel Advance additionally requires a bounce buffer.

### sdkconfig.defaults (Both Boards)

```
CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y
CONFIG_SPIRAM_RODATA=y
```

### CrowPanel Advance Only - Bounce Buffer

```c
#ifdef CROWPANEL_ADVANCE
    // Use bounce buffer to prevent PSRAM access from causing display offset
    // 10 lines * 800 pixels * 2 bytes = 16000 bytes from internal SRAM
    .bounce_buffer_size_px = 800 * 10,
#endif
```

This prevents horizontal display offset that occurs when PSRAM access interferes with the RGB panel's DMA timing.

## Build Configuration

### platformio.ini

**CrowPanel Basic:**
```ini
[env:esp32_8048s070_espidf]
build_flags =
    -DBOARD_HAS_PSRAM
    ; No CROWPANEL_ADVANCE flag
```

**CrowPanel Advance:**
```ini
[env:crowpanel_advance_16mb]
build_flags =
    -DBOARD_HAS_PSRAM
    -DCROWPANEL_ADVANCE
```

## Code Structure

The BSP (Board Support Package) uses conditional compilation:

```c
#ifdef CROWPANEL_ADVANCE
    // CrowPanel Advance specific code
#else
    // CrowPanel Basic board code
#endif
```

Key files:
- `src/esp_bsp.h` - Pin definitions and hardware constants
- `src/esp_bsp.c` - Display, touch, and backlight driver implementation
- `platformio.ini` - Build environments and flags
- `sdkconfig.defaults` - ESP-IDF configuration

## Troubleshooting

### Display Not Working (Black Screen)

1. **Check I2C devices:** Boot log should show devices at 0x30, 0x51, 0x5D
2. **Verify pin configuration:** Ensure `CROWPANEL_ADVANCE` is defined
3. **Check backlight:** STC8H1K28 should receive I2C commands at 0x30

### Display Offset (Image Shifted)

1. Ensure `CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y` and `CONFIG_SPIRAM_RODATA=y` in sdkconfig
2. Verify bounce buffer is enabled: `.bounce_buffer_size_px = 800 * 10`

### Touch Not Working

1. Verify I2C scan shows GT911 at 0x5D
2. Check I2C pins are correct: SDA=15, SCL=16
3. GT911 should report "Product ID: 911" in boot log

### Colors Wrong or Image Distorted

1. Verify RGB pin mapping matches table above
2. Ensure `.pclk_active_neg = true` for CrowPanel Advance
3. Check pixel clock is 21MHz (not 14MHz or 18MHz)

## References

- [CrowPanel Basic 7" Product Page](https://www.elecrow.com/esp32-display-7-inch-hmi-display-rgb-tft-lcd-touch-screen-support-lvgl.html)
- [CrowPanel Basic 7" Wiki](https://www.elecrow.com/pub/wiki/esp32-display-702727-intelligent-touch-screen-wi-fi26ble-800480-hmi-display.html)
- [CrowPanel Advance 7" Product Page](https://www.elecrow.com/crowpanel-advance-7-hmi-esp32-ai-display-800x480-ai-ips-touch-screen.html)
- [CrowPanel Advance 7" Wiki](https://www.elecrow.com/pub/wiki/ESP32_Display-7.0_inch%28Advance_Series%29wiki.html)
- [Official Factory Source Code (GitHub)](https://github.com/Elecrow-RD/CrowPanel-Advance-7-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-800x480)
- [ESP-IDF RGB LCD Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd.html)

## Version History

- **2026-01-30**: Initial working configuration with official Elecrow factory settings
  - I2C backlight control via STC8H1K28
  - Correct RGB pin mapping from LovyanGFX_Driver.h
  - Bounce buffer for display offset fix
  - 21MHz pixel clock with pclk_active_neg=true
