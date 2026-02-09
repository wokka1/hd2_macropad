# OTA (Over-The-Air) Update Guide

## Overview

This guide explains how to implement OTA updates for the HD2 Macropad, allowing firmware updates to be downloaded from GitHub releases without physical USB connection.

## Architecture

### Current Partition Layout

Your device uses a dual-app partition scheme ([partitions/16MB_app.csv](partitions/16MB_app.csv)):

| Partition | Size | Purpose |
|-----------|------|---------|
| app0 | 7.5 MB | Primary firmware partition |
| app1 | 7.5 MB | Secondary firmware partition for OTA |
| nvs | 20 KB | Settings storage |
| otadata | 8 KB | Tracks which app is active |
| spiffs | ~960 KB | Filesystem for assets |

### OTA Update Flow

```
1. Device connects to WiFi
2. Checks GitHub for new release
3. Downloads firmware binary (.bin) via HTTPS
4. Writes to inactive partition (app0 or app1)
5. Validates downloaded firmware
6. Sets new partition as bootable
7. Reboots into new firmware
8. On failure, automatically rolls back to old firmware
```

## Prerequisites

### 1. WiFi Configuration

You'll need to add WiFi connection capability:
- SSID and password storage (in NVS)
- WiFi station mode initialization
- Connection management

### 2. GitHub Release Setup

Firmware binaries will be hosted as GitHub release assets:
- Create GitHub releases with version tags (e.g., `v1.0.0`, `v1.1.0`)
- Attach compiled `.bin` file to each release
- Use HTTPS URLs for downloads

### 3. Required ESP-IDF Components

Already available in ESP-IDF:
- `esp_https_ota` - HTTPS OTA update library
- `esp_wifi` - WiFi stack (already enabled in sdkconfig)
- `nvs_flash` - Non-volatile storage
- `esp_http_client` - HTTP/HTTPS client

## Implementation Steps

### Step 1: Add WiFi Manager Module

Create `src/wifi/wifi_manager.h`:

```c
#pragma once

#include <esp_wifi.h>
#include <esp_event.h>

#ifdef __cplusplus
extern "C" {
#endif

// WiFi connection status
typedef enum {
    WIFI_STATUS_DISCONNECTED,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_FAILED
} wifi_status_t;

// WiFi AP information structure
typedef struct {
    char ssid[33];           // SSID (max 32 chars + null terminator)
    int8_t rssi;             // Signal strength
    wifi_auth_mode_t authmode; // Security type
} wifi_ap_record_simple_t;

/**
 * @brief Initialize WiFi manager
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Scan for available WiFi networks
 *
 * @param ap_list Pointer to array to store AP records
 * @param max_aps Maximum number of APs to return
 * @param num_found Pointer to store number of APs found
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_scan(wifi_ap_record_simple_t *ap_list, uint16_t max_aps, uint16_t *num_found);

/**
 * @brief Connect to WiFi using stored credentials
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_connect(void);

/**
 * @brief Connect to specific WiFi network
 * @param ssid WiFi SSID
 * @param password WiFi password
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_connect_to(const char *ssid, const char *password);

/**
 * @brief Disconnect from WiFi
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_disconnect(void);

/**
 * @brief Get current WiFi status
 * @return Current WiFi status
 */
wifi_status_t wifi_manager_get_status(void);

/**
 * @brief Store WiFi credentials in NVS
 * @param ssid WiFi SSID
 * @param password WiFi password
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_set_credentials(const char *ssid, const char *password);

#ifdef __cplusplus
}
#endif
```

Create `src/wifi/wifi_manager.c`:

```c
#include "wifi_manager.h"
#include <string.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

static const char *TAG = "WiFi";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define MAX_RETRY          5

static EventGroupHandle_t s_wifi_event_group;
static wifi_status_t s_wifi_status = WIFI_STATUS_DISCONNECTED;
static int s_retry_num = 0;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        s_wifi_status = WIFI_STATUS_CONNECTING;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry connecting to WiFi (%d/%d)", s_retry_num, MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            s_wifi_status = WIFI_STATUS_FAILED;
            ESP_LOGE(TAG, "Failed to connect to WiFi");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        s_wifi_status = WIFI_STATUS_CONNECTED;
    }
}

esp_err_t wifi_manager_init(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi manager initialized");
    return ESP_OK;
}

esp_err_t wifi_manager_set_credentials(const char *ssid, const char *password)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) return err;

    err = nvs_set_str(nvs_handle, "wifi_ssid", ssid);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_set_str(nvs_handle, "wifi_pass", password);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "WiFi credentials stored");
    return err;
}

esp_err_t wifi_manager_scan(wifi_ap_record_simple_t *ap_list, uint16_t max_aps, uint16_t *num_found)
{
    if (ap_list == NULL || num_found == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting WiFi scan...");

    // Start scan (blocking mode)
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 300,
    };

    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan failed: %s", esp_err_to_name(err));
        return err;
    }

    // Get scan results
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    ESP_LOGI(TAG, "Found %d access points", ap_count);

    if (ap_count == 0) {
        *num_found = 0;
        return ESP_OK;
    }

    // Limit to max_aps
    uint16_t to_fetch = (ap_count > max_aps) ? max_aps : ap_count;

    wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * to_fetch);
    if (ap_records == NULL) {
        return ESP_ERR_NO_MEM;
    }

    err = esp_wifi_scan_get_ap_records(&to_fetch, ap_records);
    if (err != ESP_OK) {
        free(ap_records);
        return err;
    }

    // Convert to simplified format
    for (uint16_t i = 0; i < to_fetch; i++) {
        strncpy(ap_list[i].ssid, (char *)ap_records[i].ssid, sizeof(ap_list[i].ssid) - 1);
        ap_list[i].ssid[sizeof(ap_list[i].ssid) - 1] = '\0';
        ap_list[i].rssi = ap_records[i].rssi;
        ap_list[i].authmode = ap_records[i].authmode;

        ESP_LOGI(TAG, "  [%d] SSID: %s, RSSI: %d, Auth: %d",
                 i, ap_list[i].ssid, ap_list[i].rssi, ap_list[i].authmode);
    }

    *num_found = to_fetch;
    free(ap_records);

    return ESP_OK;
}

esp_err_t wifi_manager_connect_to(const char *ssid, const char *password)
{
    if (ssid == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Configure and connect
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    if (password != NULL) {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);

    s_retry_num = 0;
    s_wifi_status = WIFI_STATUS_CONNECTING;

    // Wait for connection
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi");
        // Store credentials on successful connection
        wifi_manager_set_credentials(ssid, password);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
        return ESP_FAIL;
    }
}

esp_err_t wifi_manager_connect(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;
    char ssid[32] = {0};
    char password[64] = {0};
    size_t ssid_len = sizeof(ssid);
    size_t pass_len = sizeof(password);

    // Read credentials from NVS
    err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "No WiFi credentials found");
        return err;
    }

    err = nvs_get_str(nvs_handle, "wifi_ssid", ssid, &ssid_len);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_get_str(nvs_handle, "wifi_pass", password, &pass_len);
    nvs_close(nvs_handle);
    if (err != ESP_OK) {
        return err;
    }

    // Use connect_to function
    return wifi_manager_connect_to(ssid, password);
}

esp_err_t wifi_manager_disconnect(void)
{
    s_wifi_status = WIFI_STATUS_DISCONNECTED;
    return esp_wifi_disconnect();
}

wifi_status_t wifi_manager_get_status(void)
{
    return s_wifi_status;
}
```

### Step 2: Add OTA Update Module

Create `src/ota/ota_updater.h`:

```c
#pragma once

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

// OTA update status callback
typedef void (*ota_progress_callback_t)(int progress_percent);

/**
 * @brief Check for and perform OTA update from GitHub
 *
 * @param github_url URL to firmware binary (e.g., GitHub release asset)
 * @param progress_cb Callback for progress updates (0-100%)
 * @return ESP_OK if update successful (device will reboot)
 *         ESP_FAIL if update failed
 */
esp_err_t ota_update_from_url(const char *github_url, ota_progress_callback_t progress_cb);

/**
 * @brief Get current firmware version
 * @return Version string (from app_desc)
 */
const char* ota_get_current_version(void);

#ifdef __cplusplus
}
#endif
```

Create `src/ota/ota_updater.c`:

```c
#include "ota_updater.h"
#include <string.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_https_ota.h>
#include <esp_app_format.h>

static const char *TAG = "OTA";

// GitHub certificate (replace with actual GitHub CA certificate)
extern const uint8_t github_root_cert_pem_start[] asm("_binary_github_root_cert_pem_start");
extern const uint8_t github_root_cert_pem_end[]   asm("_binary_github_root_cert_pem_end");

const char* ota_get_current_version(void)
{
    const esp_app_desc_t *app_desc = esp_app_get_description();
    return app_desc->version;
}

static esp_err_t validate_image_header(esp_app_desc_t *new_app_info)
{
    if (new_app_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_app_desc_t *running_app_info = esp_app_get_description();
    ESP_LOGI(TAG, "Running firmware version: %s", running_app_info->version);
    ESP_LOGI(TAG, "New firmware version: %s", new_app_info->version);

    return ESP_OK;
}

esp_err_t ota_update_from_url(const char *github_url, ota_progress_callback_t progress_cb)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Starting OTA update from: %s", github_url);

    esp_http_client_config_t config = {
        .url = github_url,
        .cert_pem = (char *)github_root_cert_pem_start,
        .timeout_ms = 30000,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    esp_https_ota_handle_t https_ota_handle = NULL;
    ret = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed");
        return ret;
    }

    esp_app_desc_t app_desc;
    ret = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_get_img_desc failed");
        goto ota_end;
    }

    ret = validate_image_header(&app_desc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "image header verification failed");
        goto ota_end;
    }

    // Download and write firmware
    while (1) {
        ret = esp_https_ota_perform(https_ota_handle);
        if (ret != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }

        // Report progress
        if (progress_cb) {
            int total = esp_https_ota_get_image_size(https_ota_handle);
            int downloaded = esp_https_ota_get_image_len_read(https_ota_handle);
            int progress = (downloaded * 100) / total;
            progress_cb(progress);
        }
    }

    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true) {
        ESP_LOGE(TAG, "Complete data was not received");
        ret = ESP_FAIL;
        goto ota_end;
    }

    ret = esp_https_ota_finish(https_ota_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA update successful! Rebooting...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    } else {
        ESP_LOGE(TAG, "ESP HTTPS OTA upgrade failed 0x%x", ret);
        goto ota_end;
    }

    return ESP_OK;

ota_end:
    esp_https_ota_abort(https_ota_handle);
    return ret;
}
```

### Step 3: Add GitHub Certificate

Download GitHub's root certificate and save as `certs/github_root_cert.pem`:

```bash
# Download GitHub's CA certificate
echo -n | openssl s_client -connect github.com:443 2>/dev/null | \
    sed -ne '/-BEGIN CERTIFICATE-/,/-END CERTIFICATE-/p' > certs/github_root_cert.pem
```

Add to CMakeLists.txt (or create component CMakeLists if needed):

```cmake
target_add_binary_data(${COMPONENT_TARGET} "certs/github_root_cert.pem" TEXT)
```

### Step 4: Add OTA UI to EEZ Studio

Add a new Setup page section with **two WiFi connection methods**:

#### Method 1: Scan and Select (Recommended)
- **"Scan WiFi"** button → Triggers WiFi scan
- **Dropdown/List widget** → Shows available networks with signal strength
- **Password text input** → For selected network
- **"Connect"** button → Connects to selected network

#### Method 2: Manual Entry (Fallback for hidden networks)
- **SSID text input** → Manual SSID entry
- **Password text input** → Manual password entry
- **"Connect (Manual)"** button → Connects using typed credentials

#### OTA Update Controls
- **"Check for Updates"** button
- **Progress bar** for OTA download (0-100%)
- **Status label** → Shows connection status, update progress, errors
- **Current version label** → Displays running firmware version

#### Suggested EEZ Studio Layout

```
Setup Page
├─ WiFi Section (Container)
│  ├─ Header: "WiFi Network"
│  ├─ Method 1: Scan & Select
│  │  ├─ Button: "Scan Networks" → action_wifi_scan
│  │  ├─ Dropdown: wifi_network_dropdown (initially hidden)
│  │  ├─ Text Input: wifi_password_input (placeholder: "Password")
│  │  └─ Button: "Connect" → action_wifi_connect_selected
│  │
│  ├─ Divider or Spacer
│  │
│  ├─ Method 2: Manual Entry
│  │  ├─ Text Input: wifi_ssid_manual_input (placeholder: "SSID (for hidden)")
│  │  ├─ Text Input: wifi_password_manual_input (placeholder: "Password")
│  │  └─ Button: "Connect (Manual)" → action_wifi_connect_manual
│  │
│  └─ Label: wifi_status_label (shows status messages)
│
├─ OTA Update Section (Container)
│  ├─ Header: "Firmware Update"
│  ├─ Label: "Current Version: v1.0.0"
│  ├─ Button: "Check for Updates" → action_check_for_updates
│  ├─ Progress Bar: ota_progress_bar (0-100%)
│  └─ Label: ota_status_label
```

**Widget Properties:**
- **Dropdown**: Set `lv_dropdown` widget, add placeholder option "Select network..."
- **Text Inputs**: Use `lv_textarea` with one-line mode and password mode for password fields
- **Status Labels**: Small font, centered, update dynamically
- **Buttons**: Adequate tap target size (min 60px height)

### Step 5: Create OTA Action Handlers

Add to `src/ui/actions.c`:

```c
// Global variables for WiFi scan results
#define MAX_WIFI_APS 20
static wifi_ap_record_simple_t wifi_scan_results[MAX_WIFI_APS];
static uint16_t wifi_scan_count = 0;

// Scan for WiFi networks and populate dropdown
void action_wifi_scan(lv_event_t *e)
{
    lv_label_set_text(objects.wifi_status_label, "Scanning...");

    // Perform scan
    esp_err_t ret = wifi_manager_scan(wifi_scan_results, MAX_WIFI_APS, &wifi_scan_count);

    if (ret == ESP_OK && wifi_scan_count > 0) {
        // Clear dropdown options
        lv_dropdown_clear_options(objects.wifi_network_dropdown);

        // Add each network to dropdown with signal strength indicator
        for (uint16_t i = 0; i < wifi_scan_count; i++) {
            char display_text[64];
            const char *lock_icon = (wifi_scan_results[i].authmode != WIFI_AUTH_OPEN) ? "🔒" : "";
            int signal_bars = (wifi_scan_results[i].rssi + 100) / 10; // Rough signal bars

            snprintf(display_text, sizeof(display_text), "%s %s (%d%%)",
                     lock_icon,
                     wifi_scan_results[i].ssid,
                     signal_bars * 10);

            lv_dropdown_add_option(objects.wifi_network_dropdown, display_text, i);
        }

        // Update status
        char status[32];
        snprintf(status, sizeof(status), "Found %d networks", wifi_scan_count);
        lv_label_set_text(objects.wifi_status_label, status);

        // Show dropdown
        lv_obj_clear_flag(objects.wifi_network_dropdown, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(objects.wifi_status_label, "No networks found");
    }
}

// Connect to selected network from dropdown
void action_wifi_connect_selected(lv_event_t *e)
{
    // Get selected network index
    uint16_t selected = lv_dropdown_get_selected(objects.wifi_network_dropdown);

    if (selected >= wifi_scan_count) {
        lv_label_set_text(objects.wifi_status_label, "Invalid selection");
        return;
    }

    // Get password from input
    const char *password = lv_textarea_get_text(objects.wifi_password_input);

    // Check if network requires password
    if (wifi_scan_results[selected].authmode != WIFI_AUTH_OPEN && strlen(password) == 0) {
        lv_label_set_text(objects.wifi_status_label, "Password required");
        return;
    }

    // Connect to selected network
    lv_label_set_text(objects.wifi_status_label, "Connecting...");
    esp_err_t ret = wifi_manager_connect_to(wifi_scan_results[selected].ssid, password);

    if (ret == ESP_OK) {
        lv_label_set_text(objects.wifi_status_label, "Connected!");
    } else {
        lv_label_set_text(objects.wifi_status_label, "Connection failed");
    }
}

// Manual WiFi connection (for hidden networks)
void action_wifi_connect_manual(lv_event_t *e)
{
    // Get SSID and password from UI text inputs
    const char *ssid = lv_textarea_get_text(objects.wifi_ssid_manual_input);
    const char *password = lv_textarea_get_text(objects.wifi_password_manual_input);

    if (strlen(ssid) == 0) {
        lv_label_set_text(objects.wifi_status_label, "SSID required");
        return;
    }

    // Connect
    lv_label_set_text(objects.wifi_status_label, "Connecting...");
    esp_err_t ret = wifi_manager_connect_to(ssid, password);

    if (ret == ESP_OK) {
        lv_label_set_text(objects.wifi_status_label, "Connected!");
    } else {
        lv_label_set_text(objects.wifi_status_label, "Connection failed");
    }
}

void action_check_for_updates(lv_event_t *e)
{
    // GitHub release URL (update with your repo)
    const char *firmware_url =
        "https://github.com/YOUR_USERNAME/hd2_macropad/releases/latest/download/firmware.bin";

    // Start OTA update
    esp_err_t ret = ota_update_from_url(firmware_url, ota_progress_callback);

    if (ret != ESP_OK) {
        lv_label_set_text(objects.ota_status_label, "Update failed");
    }
}

static void ota_progress_callback(int progress_percent)
{
    // Update progress bar in UI
    lv_bar_set_value(objects.ota_progress_bar, progress_percent, LV_ANIM_OFF);

    char buf[32];
    snprintf(buf, sizeof(buf), "Updating... %d%%", progress_percent);
    lv_label_set_text(objects.ota_status_label, buf);
}
```

### Step 6: Update Build Configuration

Add to `src/CMakeLists.txt` or main component:

```cmake
idf_component_register(
    SRCS
        "main.c"
        "wifi/wifi_manager.c"
        "ota/ota_updater.c"
        # ... other sources
    INCLUDE_DIRS
        "."
        "wifi"
        "ota"
    EMBED_TXTFILES
        "certs/github_root_cert.pem"
    REQUIRES
        esp_wifi
        esp_https_ota
        nvs_flash
        esp_netif
)
```

## GitHub Release Workflow

### 1. Build Firmware Binary

```bash
# Build the project
pio run -e crowpanel_advance_16mb

# The firmware binary will be at:
# .pio/build/crowpanel_advance_16mb/firmware.bin
```

### 2. Create GitHub Release

Via GitHub Web UI:
1. Go to your repository → Releases → "Draft a new release"
2. Create a new tag (e.g., `v1.0.0`)
3. Add release notes describing changes
4. **Attach** the `firmware.bin` file
5. Publish release

Via GitHub CLI:
```bash
# Create release and upload binary
gh release create v1.0.0 \
    .pio/build/crowpanel_advance_16mb/firmware.bin \
    --title "HD2 Macropad v1.0.0" \
    --notes "Release notes here"
```

### 3. Get Release Asset URL

The firmware URL will be:
```
https://github.com/YOUR_USERNAME/hd2_macropad/releases/download/v1.0.0/firmware.bin
```

Or use "latest" to always get newest:
```
https://github.com/YOUR_USERNAME/hd2_macropad/releases/latest/download/firmware.bin
```

## Testing OTA Updates

### Test Procedure

1. **Flash Initial Firmware** (v1.0.0):
   ```bash
   pio run -e crowpanel_advance_16mb -t upload
   ```

2. **Configure WiFi**:
   - Enter SSID and password in Setup page
   - Tap "Connect WiFi"
   - Verify connection status

3. **Create New Release** (v1.1.0):
   - Make code changes (increment version)
   - Build firmware
   - Create GitHub release with binary

4. **Trigger OTA Update**:
   - Tap "Check for Updates" button
   - Watch progress bar
   - Device will reboot automatically on success

5. **Verify Update**:
   - Check firmware version in Setup page
   - Test that new features work
   - Verify rollback works if update fails

### Version Management

Set firmware version in `src/CMakeLists.txt`:

```cmake
# Set project version
set(COMPONENT_VERSION "1.0.0")
```

Or in main CMakeLists:
```cmake
project(hd2_macropad VERSION 1.0.0)
```

## Security Considerations

1. **HTTPS Only**: Always use HTTPS URLs for downloads
2. **Certificate Validation**: Keep GitHub certificate up to date
3. **Signature Verification**: Consider adding firmware signature verification
4. **Version Checks**: Prevent downgrade attacks by checking version numbers
5. **Secure Storage**: Store WiFi credentials securely in NVS

## Troubleshooting

### OTA Update Fails

**Check WiFi connection**:
```c
if (wifi_manager_get_status() != WIFI_STATUS_CONNECTED) {
    ESP_LOGE(TAG, "WiFi not connected");
    return ESP_FAIL;
}
```

**Check partition space**:
- Ensure firmware binary < 7.5MB
- Check `pio run` output for binary size

**Check GitHub URL**:
- Test URL in browser first
- Ensure file is publicly accessible
- Check for redirects

### Device Won't Boot After Update

The OTA system automatically rolls back to the previous firmware if:
- New firmware fails to boot
- New firmware crashes repeatedly
- Watchdog timeout occurs

To manually trigger rollback:
```c
const esp_partition_t *running = esp_ota_get_running_partition();
esp_ota_set_boot_partition(running);
esp_restart();
```

## Advanced Features

### Automatic Update Check

Add to main loop:
```c
static void check_for_updates_task(void *pvParameters)
{
    while (1) {
        // Check once per day
        vTaskDelay(pdMS_TO_TICKS(24 * 60 * 60 * 1000));

        if (wifi_manager_get_status() == WIFI_STATUS_CONNECTED) {
            // Fetch version info and compare
            // If newer version available, notify user
        }
    }
}
```

### Version Comparison API

Fetch version from GitHub API:
```c
GET https://api.github.com/repos/YOUR_USERNAME/hd2_macropad/releases/latest
```

Parse JSON response to check version number.

## WiFi Setup: Why Both Methods?

### Scan & Select (Primary Method)
**Pros:**
- ✅ User-friendly - no typing needed
- ✅ Shows signal strength to pick best AP
- ✅ Shows security type (open/locked)
- ✅ Prevents typos in SSID
- ✅ Faster for most users

**Cons:**
- ❌ Can't connect to hidden networks
- ❌ Requires scan each time (a few seconds delay)

### Manual Entry (Fallback Method)
**Pros:**
- ✅ Works with hidden networks
- ✅ Can enter SSID exactly as configured
- ✅ No scan required (if you know SSID)

**Cons:**
- ❌ Requires typing on touchscreen keyboard
- ❌ Easy to make typos
- ❌ Slower for casual users

### Best Practice
Provide both methods! Most users will use scan & select, but power users or those with hidden networks need manual entry.

## References

- [ESP-IDF OTA Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html)
- [ESP-IDF WiFi Scan Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi.html#scan)
- [ESP HTTPS OTA Example](https://github.com/espressif/esp-idf/tree/master/examples/system/ota/simple_ota_example)
- [GitHub Releases Documentation](https://docs.github.com/en/repositories/releasing-projects-on-github)
