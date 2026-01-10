/*
 * ESP32-8048S070 Display Configuration for LovyanGFX
 * 7" 800x480 RGB Parallel Display
 * Based on working Elecrow reference implementation
 */

#pragma once

#ifdef ESP32_8048S070

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>

class LGFX : public lgfx::LGFX_Device
{
public:
    lgfx::Bus_RGB     _bus_instance;
    lgfx::Panel_RGB   _panel_instance;
    lgfx::Light_PWM   _light_instance;

    LGFX(void)
    {
        // Configure panel first
        {
            auto cfg = _panel_instance.config();
            cfg.memory_width  = 800;
            cfg.memory_height = 480;
            cfg.panel_width   = 800;
            cfg.panel_height  = 480;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            _panel_instance.config(cfg);
        }

        // Configure PSRAM usage
        {
            auto cfg = _panel_instance.config_detail();
            cfg.use_psram = 1;  // 1 = use PSRAM for framebuffer
            _panel_instance.config_detail(cfg);
        }

        // Configure RGB bus
        {
            auto cfg = _bus_instance.config();
            cfg.panel = &_panel_instance;

            // RGB Data pins
            cfg.pin_d0  = GPIO_NUM_15;  // B0
            cfg.pin_d1  = GPIO_NUM_7;   // B1
            cfg.pin_d2  = GPIO_NUM_6;   // B2
            cfg.pin_d3  = GPIO_NUM_5;   // B3
            cfg.pin_d4  = GPIO_NUM_4;   // B4

            cfg.pin_d5  = GPIO_NUM_9;   // G0
            cfg.pin_d6  = GPIO_NUM_46;  // G1
            cfg.pin_d7  = GPIO_NUM_3;   // G2
            cfg.pin_d8  = GPIO_NUM_8;   // G3
            cfg.pin_d9  = GPIO_NUM_16;  // G4
            cfg.pin_d10 = GPIO_NUM_1;   // G5

            cfg.pin_d11 = GPIO_NUM_14;  // R0
            cfg.pin_d12 = GPIO_NUM_21;  // R1
            cfg.pin_d13 = GPIO_NUM_47;  // R2
            cfg.pin_d14 = GPIO_NUM_48;  // R3
            cfg.pin_d15 = GPIO_NUM_45;  // R4

            // Control pins
            cfg.pin_henable = GPIO_NUM_41;  // DE (Data Enable)
            cfg.pin_vsync   = GPIO_NUM_40;  // VSYNC
            cfg.pin_hsync   = GPIO_NUM_39;  // HSYNC
            cfg.pin_pclk    = GPIO_NUM_0;   // PCLK
            cfg.freq_write  = 15000000;     // 15MHz pixel clock

            // Horizontal timing
            cfg.hsync_polarity    = 0;
            cfg.hsync_front_porch = 40;
            cfg.hsync_pulse_width = 48;
            cfg.hsync_back_porch  = 40;

            // Vertical timing
            cfg.vsync_polarity    = 0;
            cfg.vsync_front_porch = 1;
            cfg.vsync_pulse_width = 31;
            cfg.vsync_back_porch  = 13;

            // Polarity settings
            cfg.pclk_active_neg = 1;
            cfg.de_idle_high    = 0;
            cfg.pclk_idle_high  = 0;

            _bus_instance.config(cfg);
        }

        // Set bus to panel
        _panel_instance.setBus(&_bus_instance);

        // Configure backlight
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl      = GPIO_NUM_2;   // Backlight PWM pin
            cfg.invert      = false;
            cfg.freq        = 44100;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
        }

        // Set backlight to panel
        _panel_instance.setLight(&_light_instance);

        // Register panel with device
        setPanel(&_panel_instance);
    }
};

#endif // ESP32_8048S070
