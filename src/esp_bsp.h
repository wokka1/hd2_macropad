/*
 * ESP32-8048S070 Board Support Package
 * RGB Panel Display Driver for ESP-IDF
 */

#pragma once

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "lvgl.h"

/**************************************************************************************************
 *  Display Configuration - 800x480 RGB Panel
 **************************************************************************************************/
#define LCD_WIDTH           800
#define LCD_HEIGHT          480
#define LCD_BIT_PER_PIXEL   16

// LCD RGB Interface Pins for ESP32-8048S070
#define LCD_PIXEL_CLOCK_HZ  (14 * 1000 * 1000)  // 14MHz
#define LCD_BK_LIGHT_ON_LEVEL  1
#define LCD_BK_LIGHT_OFF_LEVEL !LCD_BK_LIGHT_ON_LEVEL
#define PIN_NUM_BK_LIGHT    2

#define PIN_NUM_HSYNC       39
#define PIN_NUM_VSYNC       40
#define PIN_NUM_DE          41
#define PIN_NUM_PCLK        0

// RGB Data pins (16-bit)
#define PIN_NUM_DATA0       15  // B0
#define PIN_NUM_DATA1       7   // B1
#define PIN_NUM_DATA2       6   // B2
#define PIN_NUM_DATA3       5   // B3
#define PIN_NUM_DATA4       4   // B4
#define PIN_NUM_DATA5       9   // G0
#define PIN_NUM_DATA6       46  // G1
#define PIN_NUM_DATA7       3   // G2
#define PIN_NUM_DATA8       8   // G3
#define PIN_NUM_DATA9       16  // G4
#define PIN_NUM_DATA10      1   // G5
#define PIN_NUM_DATA11      14  // R0
#define PIN_NUM_DATA12      21  // R1
#define PIN_NUM_DATA13      47  // R2
#define PIN_NUM_DATA14      48  // R3
#define PIN_NUM_DATA15      45  // R4

// Touch I2C pins (GT911)
#define PIN_NUM_TOUCH_SDA   19
#define PIN_NUM_TOUCH_SCL   20
#define PIN_NUM_TOUCH_RST   38
#define PIN_NUM_TOUCH_INT   18

// I2C Configuration
#define BSP_I2C_NUM         0
#define BSP_I2C_CLK_SPEED_HZ 400000

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BSP display configuration structure
 */
typedef struct {
    uint32_t buffer_size;           /*!< Size of the buffer for the screen in pixels */
    lv_disp_rot_t rotate;           /*!< Rotation configuration for the display */
} bsp_display_cfg_t;

/**
 * @brief Initialize display with configuration
 *
 * @param cfg display configuration
 * @return Pointer to LVGL display or NULL when error occurred
 */
lv_disp_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg);

/**
 * @brief Get pointer to input device (touch)
 *
 * @return Pointer to LVGL input device or NULL when not initialized
 */
lv_indev_t *bsp_display_get_input_dev(void);

/**
 * @brief Set display's brightness
 *
 * @param brightness_percent Brightness in [%]
 * @return ESP_OK on success
 */
esp_err_t bsp_display_brightness_set(int brightness_percent);

/**
 * @brief Turn on display backlight
 *
 * @return ESP_OK on success
 */
esp_err_t bsp_display_backlight_on(void);

/**
 * @brief Turn off display backlight
 *
 * @return ESP_OK on success
 */
esp_err_t bsp_display_backlight_off(void);

/**
 * @brief Take LVGL mutex
 *
 * @param timeout_ms Timeout in [ms]. 0 will block indefinitely.
 * @return true if mutex was taken
 */
bool bsp_display_lock(uint32_t timeout_ms);

/**
 * @brief Give LVGL mutex
 */
void bsp_display_unlock(void);

#ifdef __cplusplus
}
#endif
