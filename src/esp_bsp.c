/*
 * ESP32-8048S070 Board Support Package
 * RGB Panel Display Driver for ESP-IDF
 *
 * Based on working bluetooth-testing branch implementation
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/i2c.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_heap_caps.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"

#include "esp_bsp.h"
#include "main.h"

static const char *TAG = "BSP";

// Display panel handle
static esp_lcd_panel_handle_t panel_handle = NULL;
static SemaphoreHandle_t lvgl_mux = NULL;

// LVGL display
static lv_disp_draw_buf_t disp_buf;
static lv_disp_drv_t disp_drv;
static lv_disp_t *disp = NULL;

// Touch input device
static lv_indev_drv_t indev_drv;
static lv_indev_t *indev = NULL;

// Touch state
static int16_t touch_last_x = 0;
static int16_t touch_last_y = 0;

// GT911 Touch Controller I2C Address
#define GT911_I2C_ADDR_1    0x5D
#define GT911_I2C_ADDR_2    0x14
static uint8_t gt911_addr = GT911_I2C_ADDR_1;

#if !BACKLIGHT_USE_I2C
// LEDC channel for backlight (PWM mode for original ESP32-8048S070)
#define LCD_LEDC_CH         1
#endif

/**********************
 * BACKLIGHT CONTROL
 **********************/

#if BACKLIGHT_USE_I2C
/**
 * STC8H1K28 I2C Backlight Controller for CrowPanel Advance
 * Protocol: Write single byte to address 0x30
 *   0 = Maximum brightness
 *   1-244 = Dimming levels (1=brightest dim, 244=dimmest)
 *   245 = Off
 *   246 = Buzzer on
 *   247 = Buzzer off
 */

static bool backlight_i2c_ready = false;

static esp_err_t stc8h_write_brightness(uint8_t value)
{
    if (!backlight_i2c_ready) {
        ESP_LOGW(TAG, "Backlight I2C not ready");
        return ESP_ERR_INVALID_STATE;
    }

    if (debugLogging) {
        ESP_LOGI(TAG, "STC8H1K28: Writing value %d to I2C addr 0x%02X", value, BACKLIGHT_I2C_ADDR);
    }
    esp_err_t ret = i2c_master_write_to_device(BSP_I2C_NUM, BACKLIGHT_I2C_ADDR,
                                                &value, 1, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write to STC8H1K28: %s (0x%04X)", esp_err_to_name(ret), ret);
    } else if (debugLogging) {
        ESP_LOGI(TAG, "STC8H1K28: Write successful");
    }
    return ret;
}

static esp_err_t bsp_display_brightness_init(void)
{
    ESP_LOGI(TAG, "CrowPanel Advance: Using STC8H1K28 I2C backlight controller at 0x%02X", BACKLIGHT_I2C_ADDR);
    // I2C will be initialized by bsp_i2c_init() before touch init
    // Mark as ready - actual I2C init happens later
    backlight_i2c_ready = true;
    return ESP_OK;
}

esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent > 100) {
        brightness_percent = 100;
    }
    if (brightness_percent < 0) {
        brightness_percent = 0;
    }

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness_percent);

    // STC8H1K28: 0 = max brightness, 245 = off
    // Convert percentage: 100% -> 0, 0% -> 245
    uint8_t value;
    if (brightness_percent == 0) {
        value = BACKLIGHT_OFF;  // 245 = off
    } else if (brightness_percent >= 100) {
        value = BACKLIGHT_MAX;  // 0 = max brightness
    } else {
        // Linear scale: 100% = 0, 1% = 244
        // brightness_percent: 1-99 maps to value: 1-244
        value = (uint8_t)(245 - (brightness_percent * 245 / 100));
    }

    return stc8h_write_brightness(value);
}

esp_err_t bsp_display_backlight_off(void)
{
    ESP_LOGI(TAG, "Backlight OFF");
    return stc8h_write_brightness(BACKLIGHT_OFF);
}

esp_err_t bsp_display_backlight_on(void)
{
    ESP_LOGI(TAG, "Backlight ON (max brightness)");
    return stc8h_write_brightness(BACKLIGHT_MAX);
}

esp_err_t bsp_buzzer_on(void)
{
    if (debugLogging) ESP_LOGI(TAG, "Buzzer ON");
    return stc8h_write_brightness(246);  // 246 = buzzer on
}

esp_err_t bsp_buzzer_off(void)
{
    if (debugLogging) ESP_LOGI(TAG, "Buzzer OFF");
    return stc8h_write_brightness(247);  // 247 = buzzer off
}

esp_err_t bsp_buzzer_beep(uint16_t duration_ms)
{
    if (debugLogging) ESP_LOGI(TAG, "=== BUZZER BEEP START (duration: %d ms) ===", duration_ms);
    esp_err_t ret = bsp_buzzer_on();
    if (ret == ESP_OK) {
        if (debugLogging) ESP_LOGI(TAG, "Buzzer ON successful, delaying %d ms...", duration_ms);
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        if (debugLogging) ESP_LOGI(TAG, "Delay complete, turning buzzer OFF...");
        ret = bsp_buzzer_off();
        if (ret == ESP_OK) {
            if (debugLogging) ESP_LOGI(TAG, "=== BUZZER BEEP COMPLETE ===");
        } else {
            ESP_LOGE(TAG, "=== BUZZER BEEP FAILED (turning off) ===");
        }
    } else {
        ESP_LOGE(TAG, "=== BUZZER BEEP FAILED (turning on) ===");
    }
    return ret;
}

#else
/* PWM Backlight Control for original ESP32-8048S070 */

static esp_err_t bsp_display_brightness_init(void)
{
    ESP_LOGI(TAG, "Initializing backlight PWM on GPIO %d", PIN_NUM_BK_LIGHT);

    const ledc_timer_config_t LCD_backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = 1,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };

    const ledc_channel_config_t LCD_backlight_channel = {
        .gpio_num = PIN_NUM_BK_LIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = 1,
        .duty = 0,
        .hpoint = 0
    };

    ESP_ERROR_CHECK(ledc_timer_config(&LCD_backlight_timer));
    ESP_ERROR_CHECK(ledc_channel_config(&LCD_backlight_channel));

    return ESP_OK;
}

esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent > 100) {
        brightness_percent = 100;
    }
    if (brightness_percent < 0) {
        brightness_percent = 0;
    }

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness_percent);
    uint32_t duty_cycle = (1023 * brightness_percent) / 100;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty_cycle));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH));

    return ESP_OK;
}

esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_brightness_set(0);
}

esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(100);
}

// Buzzer stubs for non-I2C boards (no buzzer hardware)
esp_err_t bsp_buzzer_on(void)
{
    ESP_LOGW(TAG, "Buzzer not available on this board");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_buzzer_off(void)
{
    return ESP_OK;  // No-op
}

esp_err_t bsp_buzzer_beep(uint16_t duration_ms)
{
    ESP_LOGW(TAG, "Buzzer not available on this board");
    return ESP_ERR_NOT_SUPPORTED;
}

#endif // BACKLIGHT_USE_I2C

/**********************
 * LVGL CALLBACKS
 **********************/

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)drv->user_data;
    int offsetx1 = area->x1;
    int offsety1 = area->y1;
    int offsetx2 = area->x2;
    int offsety2 = area->y2;
    esp_lcd_panel_draw_bitmap(panel, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
    lv_disp_flush_ready(drv);
}

static void increase_lvgl_tick(void *arg)
{
    lv_tick_inc(1);
}

/**********************
 * GT911 TOUCH DRIVER
 **********************/

static bool i2c_initialized = false;

static esp_err_t bsp_i2c_init(void)
{
    if (i2c_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing I2C for touch controller");

    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_NUM_TOUCH_SDA,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = PIN_NUM_TOUCH_SCL,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = BSP_I2C_CLK_SPEED_HZ
    };

    ESP_ERROR_CHECK(i2c_param_config(BSP_I2C_NUM, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(BSP_I2C_NUM, i2c_conf.mode, 0, 0, 0));

    i2c_initialized = true;

    // Scan I2C bus for devices
    ESP_LOGI(TAG, "Scanning I2C bus...");
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(BSP_I2C_NUM, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "I2C device found at address 0x%02X", addr);
        }
    }
    ESP_LOGI(TAG, "I2C scan complete");

    return ESP_OK;
}

static esp_err_t gt911_i2c_read(uint16_t reg, uint8_t *data, size_t len)
{
    uint8_t reg_buf[2] = {reg >> 8, reg & 0xFF};

    esp_err_t ret = i2c_master_write_read_device(BSP_I2C_NUM, gt911_addr,
                                                  reg_buf, sizeof(reg_buf),
                                                  data, len,
                                                  pdMS_TO_TICKS(100));
    return ret;
}

static esp_err_t gt911_i2c_write(uint16_t reg, uint8_t *data, size_t len)
{
    uint8_t *buf = malloc(2 + len);
    if (!buf) {
        return ESP_ERR_NO_MEM;
    }

    buf[0] = reg >> 8;
    buf[1] = reg & 0xFF;
    if (len > 0 && data) {
        memcpy(buf + 2, data, len);
    }

    esp_err_t ret = i2c_master_write_to_device(BSP_I2C_NUM, gt911_addr,
                                                buf, 2 + len,
                                                pdMS_TO_TICKS(100));
    free(buf);
    return ret;
}

static esp_err_t gt911_init(void)
{
    ESP_LOGI(TAG, "Initializing GT911 touch controller");

    // Reset sequence for GT911
    gpio_config_t rst_conf = {
        .pin_bit_mask = (1ULL << PIN_NUM_TOUCH_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&rst_conf);

    gpio_config_t int_conf = {
        .pin_bit_mask = (1ULL << PIN_NUM_TOUCH_INT),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&int_conf);

    // GT911 reset sequence to set I2C address to 0x5D
    gpio_set_level(PIN_NUM_TOUCH_RST, 0);
    gpio_set_level(PIN_NUM_TOUCH_INT, 0);
    vTaskDelay(pdMS_TO_TICKS(10));

    gpio_set_level(PIN_NUM_TOUCH_INT, 1);  // INT high for addr 0x5D
    vTaskDelay(pdMS_TO_TICKS(1));

    gpio_set_level(PIN_NUM_TOUCH_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Set INT as input after reset
    int_conf.mode = GPIO_MODE_INPUT;
    gpio_config(&int_conf);

    vTaskDelay(pdMS_TO_TICKS(50));

    // Try to read product ID to verify communication
    uint8_t product_id[4] = {0};
    esp_err_t ret = gt911_i2c_read(0x8140, product_id, 4);

    if (ret != ESP_OK) {
        // Try alternate address
        gt911_addr = GT911_I2C_ADDR_2;
        ret = gt911_i2c_read(0x8140, product_id, 4);
    }

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "GT911 Product ID: %c%c%c%c (addr: 0x%02X)",
                 product_id[0], product_id[1], product_id[2], product_id[3], gt911_addr);
    } else {
        ESP_LOGW(TAG, "GT911 not detected, touch disabled");
        return ret;
    }

    return ESP_OK;
}

static bool gt911_read_touch(int16_t *x, int16_t *y)
{
    uint8_t status;

    // Read touch status register
    if (gt911_i2c_read(0x814E, &status, 1) != ESP_OK) {
        return false;
    }

    // Check if touch data is ready and there's at least one touch point
    if ((status & 0x80) == 0 || (status & 0x0F) == 0) {
        // Clear status
        uint8_t clear = 0;
        gt911_i2c_write(0x814E, &clear, 1);
        return false;
    }

    // Read first touch point
    uint8_t touch_data[4];
    if (gt911_i2c_read(0x8150, touch_data, 4) != ESP_OK) {
        return false;
    }

    *x = (touch_data[1] << 8) | touch_data[0];
    *y = (touch_data[3] << 8) | touch_data[2];

    // Clear status
    uint8_t clear = 0;
    gt911_i2c_write(0x814E, &clear, 1);

    return true;
}

static void lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    int16_t x, y;

    if (gt911_read_touch(&x, &y)) {
        touch_last_x = x;
        touch_last_y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }

    data->point.x = touch_last_x;
    data->point.y = touch_last_y;
}

/**********************
 * RGB PANEL INIT
 **********************/

static esp_err_t init_lcd_panel(void)
{
    ESP_LOGI(TAG, "Initializing RGB LCD panel");

#ifdef PCLK_ACTIVE_NEG
    // CrowPanel Advance timing per ESPHome community
    ESP_LOGI(TAG, "Using CrowPanel Advance timing (pclk: %d Hz, pclk_active_neg: %d)",
             LCD_PIXEL_CLOCK_HZ, PCLK_ACTIVE_NEG);
#endif

    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_PLL160M,
        .timings = {
            .pclk_hz = LCD_PIXEL_CLOCK_HZ,
            .h_res = LCD_WIDTH,
            .v_res = LCD_HEIGHT,
#ifdef CROWPANEL_ADVANCE
            // CrowPanel Advance V1.2 timing from official Elecrow factory code
            .hsync_front_porch = 8,
            .hsync_pulse_width = 4,
            .hsync_back_porch = 8,
            .vsync_front_porch = 8,
            .vsync_pulse_width = 4,
            .vsync_back_porch = 8,
#else
            // Original ESP32-8048S070 timing
            .hsync_pulse_width = 1,
            .hsync_back_porch = 40,
            .hsync_front_porch = 48,
            .vsync_pulse_width = 1,
            .vsync_back_porch = 40,
            .vsync_front_porch = 13,
#endif
            .flags = {
#ifdef PCLK_ACTIVE_NEG
                .pclk_active_neg = true,
#else
                .pclk_active_neg = false,
#endif
            },
        },
        .data_width = 16,
        .psram_trans_align = 64,
        .hsync_gpio_num = PIN_NUM_HSYNC,
        .vsync_gpio_num = PIN_NUM_VSYNC,
        .de_gpio_num = PIN_NUM_DE,
        .pclk_gpio_num = PIN_NUM_PCLK,
        .data_gpio_nums = {
            PIN_NUM_DATA0, PIN_NUM_DATA1, PIN_NUM_DATA2, PIN_NUM_DATA3,
            PIN_NUM_DATA4, PIN_NUM_DATA5, PIN_NUM_DATA6, PIN_NUM_DATA7,
            PIN_NUM_DATA8, PIN_NUM_DATA9, PIN_NUM_DATA10, PIN_NUM_DATA11,
            PIN_NUM_DATA12, PIN_NUM_DATA13, PIN_NUM_DATA14, PIN_NUM_DATA15,
        },
        .disp_gpio_num = GPIO_NUM_NC,
#ifdef CROWPANEL_ADVANCE
        // Use bounce buffer to prevent PSRAM access from causing display offset
        // 10 lines * 800 pixels * 2 bytes = 16000 bytes from internal SRAM
        .bounce_buffer_size_px = 800 * 10,
#endif
        .flags = {
            .fb_in_psram = 1,
        },
    };

    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    ESP_LOGI(TAG, "RGB LCD panel initialized");
    return ESP_OK;
}

/**********************
 * LVGL INIT
 **********************/

static void lvgl_timer_task(void *arg)
{
    ESP_LOGI(TAG, "LVGL timer task started");

    while (1) {
        if (pdTRUE == xSemaphoreTake(lvgl_mux, portMAX_DELAY)) {
            lv_timer_handler();
            xSemaphoreGive(lvgl_mux);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static esp_err_t init_lvgl(const bsp_display_cfg_t *cfg)
{
    ESP_LOGI(TAG, "Initializing LVGL");

    lv_init();

    // Create mutex for LVGL
    lvgl_mux = xSemaphoreCreateMutex();
    if (!lvgl_mux) {
        ESP_LOGE(TAG, "Failed to create LVGL mutex");
        return ESP_ERR_NO_MEM;
    }

    // Allocate LVGL draw buffers in PSRAM
    size_t buf_size = LCD_WIDTH * 50;  // 50 lines buffer
    void *buf1 = heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    void *buf2 = heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);

    if (!buf1 || !buf2) {
        ESP_LOGE(TAG, "Failed to allocate LVGL buffers");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "LVGL buffers allocated: %d bytes each in PSRAM", buf_size * sizeof(lv_color_t));

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, buf_size);

    // Initialize display driver
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_WIDTH;
    disp_drv.ver_res = LCD_HEIGHT;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;

    // Apply rotation if specified
    if (cfg && cfg->rotate != LV_DISP_ROT_NONE) {
        disp_drv.sw_rotate = 1;
        disp_drv.rotated = cfg->rotate;
    }

    disp = lv_disp_drv_register(&disp_drv);

    // Setup periodic timer for LVGL tick
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1000));  // 1ms tick

    // Create LVGL task
    xTaskCreatePinnedToCore(lvgl_timer_task, "lvgl", 8192, NULL, 4, NULL, 0);

    ESP_LOGI(TAG, "LVGL initialized");
    return ESP_OK;
}

static esp_err_t init_touch(void)
{
    // Initialize I2C
    esp_err_t ret = bsp_i2c_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "I2C init failed, touch disabled");
        return ret;
    }

    // Initialize GT911
    ret = gt911_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "GT911 init failed, touch disabled");
        return ret;
    }

    // Register touch input device with LVGL
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_cb;
    indev = lv_indev_drv_register(&indev_drv);

    ESP_LOGI(TAG, "Touch input initialized");
    return ESP_OK;
}

/**********************
 * PUBLIC API
 **********************/

lv_disp_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg)
{
    ESP_LOGI(TAG, "Starting display initialization");

#if BACKLIGHT_USE_I2C
    // For CrowPanel Advance: Initialize I2C first (needed for backlight control)
    ESP_LOGI(TAG, "CrowPanel Advance: Initializing I2C bus first for backlight control");
    if (bsp_i2c_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2C init failed");
        return NULL;
    }
#endif

    // Initialize backlight
    if (bsp_display_brightness_init() != ESP_OK) {
        ESP_LOGE(TAG, "Backlight init failed");
        return NULL;
    }

    // Initialize RGB panel
    if (init_lcd_panel() != ESP_OK) {
        ESP_LOGE(TAG, "LCD panel init failed");
        return NULL;
    }

    // Turn on backlight after panel is ready
    bsp_display_backlight_on();

    // Initialize LVGL
    if (init_lvgl(cfg) != ESP_OK) {
        ESP_LOGE(TAG, "LVGL init failed");
        return NULL;
    }

    // Initialize touch (non-fatal if it fails)
    init_touch();

    ESP_LOGI(TAG, "Display initialization complete");
    return disp;
}

lv_indev_t *bsp_display_get_input_dev(void)
{
    return indev;
}

bool bsp_display_lock(uint32_t timeout_ms)
{
    if (!lvgl_mux) {
        return false;
    }

    TickType_t ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(lvgl_mux, ticks) == pdTRUE;
}

void bsp_display_unlock(void)
{
    if (lvgl_mux) {
        xSemaphoreGive(lvgl_mux);
    }
}
