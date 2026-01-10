/*
 * ESP32-8048S070 Display Configuration for LovyanGFX
 * 7" 800x480 RGB Parallel Display
 */

#pragma once

#ifdef ESP32_8048S070

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>

class LGFX : public lgfx::LGFX_Device
{
    lgfx::v1::Panel_RGB panel_instance;
    lgfx::v1::Bus_RGB bus_instance;
    lgfx::v1::Light_PWM backlight_instance;

public:
    LGFX(void)
    {
        {
            auto cfg = bus_instance.config();

            cfg.panel = &panel_instance;
            cfg.port = 0;                   // LCD_CAM peripheral (0 for ESP32-S3)
            cfg.freq_write = 15000000;      // 15MHz pixel clock

            // Control pins
            cfg.pin_pclk    = GPIO_NUM_0;   // PCLK
            cfg.pin_vsync   = GPIO_NUM_40;  // VSYNC
            cfg.pin_hsync   = GPIO_NUM_39;  // HSYNC
            cfg.pin_henable = GPIO_NUM_41;  // DE (Data Enable)

            // RGB Data pins (16-bit)
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

            // RGB timing parameters (from Elecrow specs)
            cfg.pclk_active_neg = 1;
            cfg.de_idle_high = 0;
            cfg.pclk_idle_high = 0;

            // Horizontal timing
            cfg.hsync_polarity = 0;
            cfg.hsync_front_porch = 40;
            cfg.hsync_pulse_width = 48;
            cfg.hsync_back_porch = 40;

            // Vertical timing
            cfg.vsync_polarity = 0;
            cfg.vsync_front_porch = 1;
            cfg.vsync_pulse_width = 31;
            cfg.vsync_back_porch = 13;

            bus_instance.config(cfg);
            panel_instance.setBus(&bus_instance);
        }

        {
            auto cfg = panel_instance.config();

            cfg.memory_width  = 800;
            cfg.memory_height = 480;
            cfg.panel_width   = 800;
            cfg.panel_height  = 480;

            cfg.offset_x = 0;
            cfg.offset_y = 0;

            panel_instance.config(cfg);
        }

        {
            auto cfg = panel_instance.config_detail();

            // Use PSRAM for frame buffer
            cfg.use_psram = 2;

            panel_instance.config_detail(cfg);
        }

        {
            auto cfg = backlight_instance.config();

            cfg.pin_bl      = GPIO_NUM_2;   // Backlight PWM pin
            cfg.invert      = false;
            cfg.freq        = 44100;
            cfg.pwm_channel = 7;

            backlight_instance.config(cfg);
            panel_instance.setLight(&backlight_instance);
        }

        setPanel(&panel_instance);
    }
};

#endif // ESP32_8048S070
