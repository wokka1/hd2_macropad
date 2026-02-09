#include "ota_manager.h"
#include "../version.h"
#include "../ble/ble_controller.h"

#include <string.h>
#include <stdlib.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_https_ota.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>

static const char *TAG = "OTA";

// GitHub repository for updates
#ifndef GITHUB_REPO
#define GITHUB_REPO "wokka/hd2_macropad"
#endif

// GitHub API URL for latest release
#define GITHUB_API_URL "https://api.github.com/repos/" GITHUB_REPO "/releases/latest"

// Maximum size for GitHub API response
#define MAX_HTTP_RESPONSE_SIZE 4096

// OTA information
static ota_info_t s_ota_info = {
    .current_version = SW_VER,
    .available_version = "",
    .download_url = "",
    .status = OTA_STATUS_IDLE,
    .progress_percent = 0,
    .error_message = ""
};

// HTTP response buffer
static char *s_http_response_buffer = NULL;
static int s_http_response_len = 0;

// Task management
static TaskHandle_t s_ota_task_handle = NULL;
static ota_status_callback_t s_status_callback = NULL;
static ota_progress_callback_t s_progress_callback = NULL;
static bool s_ota_busy = false;
static bool s_ble_was_disabled = false;  // Track if we disabled BLE for OTA

// Stack size for OTA tasks (TLS requires ~8KB minimum)
#define OTA_TASK_STACK_SIZE (8 * 1024)

// Static task resources (allocated from PSRAM)
static StaticTask_t s_ota_task_buffer;
static StackType_t *s_ota_task_stack = NULL;

// Compare two version strings (e.g., "1.0.0" vs "1.1.0")
// Returns: -1 if v1 < v2, 0 if equal, 1 if v1 > v2
static int version_compare(const char *v1, const char *v2)
{
    int major1 = 0, minor1 = 0, patch1 = 0;
    int major2 = 0, minor2 = 0, patch2 = 0;

    // Skip 'v' prefix if present
    if (v1[0] == 'v' || v1[0] == 'V') v1++;
    if (v2[0] == 'v' || v2[0] == 'V') v2++;

    sscanf(v1, "%d.%d.%d", &major1, &minor1, &patch1);
    sscanf(v2, "%d.%d.%d", &major2, &minor2, &patch2);

    if (major1 != major2) return (major1 < major2) ? -1 : 1;
    if (minor1 != minor2) return (minor1 < minor2) ? -1 : 1;
    if (patch1 != patch2) return (patch1 < patch2) ? -1 : 1;

    return 0;
}

// HTTP event handler for receiving response
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (s_http_response_buffer && evt->data_len > 0) {
                int remaining = MAX_HTTP_RESPONSE_SIZE - s_http_response_len - 1;
                int copy_len = (evt->data_len < remaining) ? evt->data_len : remaining;
                if (copy_len > 0) {
                    memcpy(s_http_response_buffer + s_http_response_len, evt->data, copy_len);
                    s_http_response_len += copy_len;
                    s_http_response_buffer[s_http_response_len] = '\0';
                }
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

// Log detailed memory usage
static void log_memory_report(const char *context)
{
    ESP_LOGI(TAG, "=== Memory Report: %s ===", context);

    // Internal RAM (DRAM)
    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t internal_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t internal_min = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    // PSRAM
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

    // DMA-capable memory (needed for WiFi/BLE)
    size_t dma_free = heap_caps_get_free_size(MALLOC_CAP_DMA);
    size_t dma_largest = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);

    ESP_LOGI(TAG, "Internal RAM: free=%u, largest=%u, min_ever=%u",
             (unsigned)internal_free, (unsigned)internal_largest, (unsigned)internal_min);
    ESP_LOGI(TAG, "PSRAM:        free=%u, largest=%u",
             (unsigned)psram_free, (unsigned)psram_largest);
    ESP_LOGI(TAG, "DMA-capable:  free=%u, largest=%u",
             (unsigned)dma_free, (unsigned)dma_largest);
    ESP_LOGI(TAG, "=====================================");
}

// Disable BLE to free internal RAM for OTA operations
static void ota_disable_ble(void)
{
    ESP_LOGI(TAG, "Disabling BLE to free memory for OTA...");
    log_memory_report("Before BLE Disable");

    esp_err_t ret = ble_controller_deinit();
    if (ret == ESP_OK) {
        s_ble_was_disabled = true;
        ESP_LOGI(TAG, "BLE disabled successfully");
        // Give some time for memory to be freed
        vTaskDelay(pdMS_TO_TICKS(100));
        log_memory_report("After BLE Disable");
    } else {
        ESP_LOGW(TAG, "Failed to disable BLE: %s", esp_err_to_name(ret));
        s_ble_was_disabled = false;
    }
}

// Re-enable BLE after OTA operation fails
static void ota_restore_ble(void)
{
    if (s_ble_was_disabled) {
        ESP_LOGI(TAG, "Restoring BLE after OTA operation...");
        esp_err_t ret = ble_controller_init();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "BLE restored successfully");
        } else {
            ESP_LOGW(TAG, "Failed to restore BLE: %s", esp_err_to_name(ret));
        }
        s_ble_was_disabled = false;
    }
}

esp_err_t ota_manager_init(void)
{
    ESP_LOGI(TAG, "OTA manager initialized, current version: %s", SW_VER);
    strncpy(s_ota_info.current_version, SW_VER, sizeof(s_ota_info.current_version) - 1);
    s_ota_info.status = OTA_STATUS_IDLE;

    // Mark current firmware as valid to prevent rollback
    ota_manager_confirm_update();

    // Log initial memory state
    log_memory_report("OTA Init");

    return ESP_OK;
}

const char* ota_manager_get_version(void)
{
    return SW_VER;
}

// Internal blocking check function
static esp_err_t ota_check_internal(void)
{
    esp_err_t ret = ESP_FAIL;

    s_ota_info.status = OTA_STATUS_CHECKING;
    s_ota_info.available_version[0] = '\0';
    s_ota_info.download_url[0] = '\0';
    s_ota_info.error_message[0] = '\0';

    ESP_LOGI(TAG, "Checking for updates from: %s", GITHUB_API_URL);

    // Allocate response buffer
    s_http_response_buffer = malloc(MAX_HTTP_RESPONSE_SIZE);
    if (!s_http_response_buffer) {
        ESP_LOGE(TAG, "Failed to allocate response buffer");
        strncpy(s_ota_info.error_message, "Memory allocation failed", sizeof(s_ota_info.error_message) - 1);
        s_ota_info.status = OTA_STATUS_ERROR;
        return ESP_ERR_NO_MEM;
    }
    s_http_response_len = 0;
    s_http_response_buffer[0] = '\0';

    // Configure HTTP client for GitHub API
    esp_http_client_config_t config = {
        .url = GITHUB_API_URL,
        .event_handler = http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
        .buffer_size = 1024,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        strncpy(s_ota_info.error_message, "HTTP client init failed", sizeof(s_ota_info.error_message) - 1);
        s_ota_info.status = OTA_STATUS_ERROR;
        free(s_http_response_buffer);
        s_http_response_buffer = NULL;
        return ESP_FAIL;
    }

    // GitHub API requires User-Agent header
    esp_http_client_set_header(client, "User-Agent", "HD2-Macropad-OTA/1.0");
    esp_http_client_set_header(client, "Accept", "application/vnd.github.v3+json");

    ret = esp_http_client_perform(client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(ret));
        strncpy(s_ota_info.error_message, "Network error", sizeof(s_ota_info.error_message) - 1);
        s_ota_info.status = OTA_STATUS_ERROR;
        goto cleanup;
    }

    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP status: %d, response length: %d", status_code, s_http_response_len);

    if (status_code == 404) {
        ESP_LOGI(TAG, "No releases found");
        strncpy(s_ota_info.error_message, "No releases found", sizeof(s_ota_info.error_message) - 1);
        s_ota_info.status = OTA_STATUS_UP_TO_DATE;
        ret = ESP_OK;
        goto cleanup;
    }

    if (status_code != 200) {
        ESP_LOGE(TAG, "GitHub API returned status %d", status_code);
        snprintf(s_ota_info.error_message, sizeof(s_ota_info.error_message), "HTTP error %d", status_code);
        s_ota_info.status = OTA_STATUS_ERROR;
        ret = ESP_FAIL;
        goto cleanup;
    }

    // Parse JSON response
    cJSON *json = cJSON_Parse(s_http_response_buffer);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        strncpy(s_ota_info.error_message, "JSON parse error", sizeof(s_ota_info.error_message) - 1);
        s_ota_info.status = OTA_STATUS_ERROR;
        ret = ESP_FAIL;
        goto cleanup;
    }

    // Extract tag_name (version)
    cJSON *tag_name = cJSON_GetObjectItem(json, "tag_name");
    if (!tag_name || !cJSON_IsString(tag_name)) {
        ESP_LOGE(TAG, "No tag_name in response");
        strncpy(s_ota_info.error_message, "Invalid release format", sizeof(s_ota_info.error_message) - 1);
        s_ota_info.status = OTA_STATUS_ERROR;
        cJSON_Delete(json);
        ret = ESP_FAIL;
        goto cleanup;
    }

    strncpy(s_ota_info.available_version, tag_name->valuestring, sizeof(s_ota_info.available_version) - 1);
    ESP_LOGI(TAG, "Latest release: %s (current: %s)", s_ota_info.available_version, SW_VER);

    // Find firmware.bin asset
    cJSON *assets = cJSON_GetObjectItem(json, "assets");
    if (assets && cJSON_IsArray(assets)) {
        cJSON *asset = NULL;
        cJSON_ArrayForEach(asset, assets) {
            cJSON *name = cJSON_GetObjectItem(asset, "name");
            if (name && cJSON_IsString(name)) {
                if (strcmp(name->valuestring, "firmware.bin") == 0) {
                    cJSON *download_url = cJSON_GetObjectItem(asset, "browser_download_url");
                    if (download_url && cJSON_IsString(download_url)) {
                        strncpy(s_ota_info.download_url, download_url->valuestring, sizeof(s_ota_info.download_url) - 1);
                        ESP_LOGI(TAG, "Firmware URL: %s", s_ota_info.download_url);
                    }
                    break;
                }
            }
        }
    }

    cJSON_Delete(json);

    // Compare versions
    int cmp = version_compare(SW_VER, s_ota_info.available_version);
    if (cmp < 0 && s_ota_info.download_url[0] != '\0') {
        ESP_LOGI(TAG, "Update available: %s -> %s", SW_VER, s_ota_info.available_version);
        s_ota_info.status = OTA_STATUS_AVAILABLE;
    } else {
        ESP_LOGI(TAG, "Already up to date");
        s_ota_info.status = OTA_STATUS_UP_TO_DATE;
    }

    ret = ESP_OK;

cleanup:
    esp_http_client_cleanup(client);
    free(s_http_response_buffer);
    s_http_response_buffer = NULL;
    return ret;
}

// Task function for checking updates
static void ota_check_task(void *pvParameters)
{
    ESP_LOGI(TAG, "OTA check task started");

    esp_err_t ret = ota_check_internal();

    // Notify via callback
    if (s_status_callback) {
        if (ret == ESP_OK) {
            s_status_callback(s_ota_info.status,
                s_ota_info.status == OTA_STATUS_AVAILABLE ? s_ota_info.available_version : "Up to date");
        } else {
            s_status_callback(OTA_STATUS_ERROR, s_ota_info.error_message);
        }
    }

    // Restore BLE if we disabled it and no update is available (or error occurred)
    // Keep BLE disabled if update is available so user can proceed with install
    if (s_ota_info.status != OTA_STATUS_AVAILABLE) {
        ota_restore_ble();
    }

    s_ota_busy = false;
    s_ota_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t ota_manager_check_for_update_async(ota_status_callback_t status_cb)
{
    if (s_ota_busy) {
        ESP_LOGW(TAG, "OTA operation already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    // Log detailed memory before OTA
    log_memory_report("Before OTA Check");

    // Disable BLE to free internal RAM for HTTPS/TLS operations
    ota_disable_ble();

    size_t internal_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    // Need at least 8KB internal RAM for LWIP semaphores, TLS buffers, and HTTP client
    if (internal_largest < 8192) {
        ESP_LOGE(TAG, "Not enough internal RAM (need 8KB, have %u)", (unsigned)internal_largest);
        strncpy(s_ota_info.error_message, "Low memory", sizeof(s_ota_info.error_message) - 1);
        s_ota_info.status = OTA_STATUS_ERROR;
        ota_restore_ble();  // Restore BLE since we're not proceeding
        return ESP_ERR_NO_MEM;
    }

    // Allocate stack from PSRAM if not already allocated
    if (s_ota_task_stack == NULL) {
        s_ota_task_stack = heap_caps_malloc(OTA_TASK_STACK_SIZE, MALLOC_CAP_SPIRAM);
        if (s_ota_task_stack == NULL) {
            ESP_LOGE(TAG, "Failed to allocate task stack from PSRAM");
            ota_restore_ble();  // Restore BLE since we're not proceeding
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "Allocated %d bytes for OTA task stack from PSRAM", OTA_TASK_STACK_SIZE);
    }

    s_status_callback = status_cb;
    s_ota_busy = true;

    // Use static task creation with PSRAM stack
    s_ota_task_handle = xTaskCreateStatic(
        ota_check_task,
        "ota_check",
        OTA_TASK_STACK_SIZE,
        NULL,
        5,  // Priority
        s_ota_task_stack,
        &s_ota_task_buffer
    );

    if (s_ota_task_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create OTA check task");
        s_ota_busy = false;
        ota_restore_ble();  // Restore BLE since we're not proceeding
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA check task created");
    return ESP_OK;
}

bool ota_manager_is_busy(void)
{
    return s_ota_busy;
}

const ota_info_t* ota_manager_get_info(void)
{
    return &s_ota_info;
}

bool ota_manager_update_available(void)
{
    return s_ota_info.status == OTA_STATUS_AVAILABLE;
}

// Internal blocking update function
static esp_err_t ota_perform_internal(void)
{
    ota_progress_callback_t progress_cb = s_progress_callback;
    if (s_ota_info.download_url[0] == '\0') {
        ESP_LOGE(TAG, "No download URL available");
        strncpy(s_ota_info.error_message, "No update URL", sizeof(s_ota_info.error_message) - 1);
        s_ota_info.status = OTA_STATUS_ERROR;
        return ESP_FAIL;
    }

    s_ota_info.status = OTA_STATUS_DOWNLOADING;
    s_ota_info.progress_percent = 0;
    s_ota_info.error_message[0] = '\0';

    ESP_LOGI(TAG, "Starting OTA update from: %s", s_ota_info.download_url);

    esp_http_client_config_t http_config = {
        .url = s_ota_info.download_url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 60000,
        .keep_alive_enable = true,
        .buffer_size = 1024,
        .buffer_size_tx = 1024,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    esp_https_ota_handle_t ota_handle = NULL;
    esp_err_t ret = esp_https_ota_begin(&ota_config, &ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(ret));
        strncpy(s_ota_info.error_message, "OTA begin failed", sizeof(s_ota_info.error_message) - 1);
        s_ota_info.status = OTA_STATUS_ERROR;
        return ret;
    }

    // Get firmware image info
    esp_app_desc_t app_desc;
    ret = esp_https_ota_get_img_desc(ota_handle, &app_desc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get image description");
        strncpy(s_ota_info.error_message, "Invalid firmware", sizeof(s_ota_info.error_message) - 1);
        s_ota_info.status = OTA_STATUS_ERROR;
        esp_https_ota_abort(ota_handle);
        return ret;
    }

    ESP_LOGI(TAG, "Downloading firmware version: %s", app_desc.version);

    // Download and write firmware
    int total_size = esp_https_ota_get_image_size(ota_handle);
    s_ota_info.status = OTA_STATUS_DOWNLOADING;

    while (1) {
        ret = esp_https_ota_perform(ota_handle);
        if (ret != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }

        // Calculate and report progress
        int downloaded = esp_https_ota_get_image_len_read(ota_handle);
        if (total_size > 0) {
            s_ota_info.progress_percent = (downloaded * 100) / total_size;
            if (progress_cb) {
                progress_cb(s_ota_info.progress_percent);
            }
        }

        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (!esp_https_ota_is_complete_data_received(ota_handle)) {
        ESP_LOGE(TAG, "Incomplete data received");
        strncpy(s_ota_info.error_message, "Download incomplete", sizeof(s_ota_info.error_message) - 1);
        s_ota_info.status = OTA_STATUS_ERROR;
        esp_https_ota_abort(ota_handle);
        return ESP_FAIL;
    }

    s_ota_info.status = OTA_STATUS_INSTALLING;
    s_ota_info.progress_percent = 100;
    if (progress_cb) {
        progress_cb(100);
    }

    ret = esp_https_ota_finish(ota_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA update successful! Rebooting in 2 seconds...");
        s_ota_info.status = OTA_STATUS_COMPLETE;
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    } else {
        if (ret == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Firmware validation failed");
            strncpy(s_ota_info.error_message, "Validation failed", sizeof(s_ota_info.error_message) - 1);
        } else {
            ESP_LOGE(TAG, "OTA finish failed: %s", esp_err_to_name(ret));
            strncpy(s_ota_info.error_message, "Update failed", sizeof(s_ota_info.error_message) - 1);
        }
        s_ota_info.status = OTA_STATUS_ERROR;
    }

    return ret;
}

// Task function for performing update
static void ota_perform_task(void *pvParameters)
{
    ESP_LOGI(TAG, "OTA perform task started");

    (void)ota_perform_internal();  // Result handled internally, success causes reboot

    // If we get here, update failed (success causes reboot)
    if (s_status_callback) {
        s_status_callback(OTA_STATUS_ERROR, s_ota_info.error_message);
    }

    // Restore BLE since update failed
    ota_restore_ble();

    s_ota_busy = false;
    s_ota_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t ota_manager_perform_update_async(ota_progress_callback_t progress_cb, ota_status_callback_t status_cb)
{
    if (s_ota_busy) {
        ESP_LOGW(TAG, "OTA operation already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_ota_info.download_url[0] == '\0') {
        ESP_LOGE(TAG, "No download URL available");
        return ESP_FAIL;
    }

    // Ensure BLE is disabled (should already be disabled from check, but just in case)
    if (!s_ble_was_disabled) {
        ota_disable_ble();
    }

    // Allocate stack from PSRAM if not already allocated
    if (s_ota_task_stack == NULL) {
        s_ota_task_stack = heap_caps_malloc(OTA_TASK_STACK_SIZE, MALLOC_CAP_SPIRAM);
        if (s_ota_task_stack == NULL) {
            ESP_LOGE(TAG, "Failed to allocate task stack from PSRAM");
            ota_restore_ble();
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "Allocated %d bytes for OTA task stack from PSRAM", OTA_TASK_STACK_SIZE);
    }

    s_progress_callback = progress_cb;
    s_status_callback = status_cb;
    s_ota_busy = true;

    // Use static task creation with PSRAM stack
    s_ota_task_handle = xTaskCreateStatic(
        ota_perform_task,
        "ota_update",
        OTA_TASK_STACK_SIZE,
        NULL,
        5,  // Priority
        s_ota_task_stack,
        &s_ota_task_buffer
    );

    if (s_ota_task_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create OTA perform task");
        s_ota_busy = false;
        ota_restore_ble();
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA perform task created");
    return ESP_OK;
}

esp_err_t ota_manager_confirm_update(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;

    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG, "Marking firmware as valid");
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }

    return ESP_OK;
}

const char* ota_manager_status_str(ota_status_t status)
{
    switch (status) {
        case OTA_STATUS_IDLE:        return "Ready";
        case OTA_STATUS_CHECKING:    return "Checking...";
        case OTA_STATUS_UP_TO_DATE:  return "Up to date";
        case OTA_STATUS_AVAILABLE:   return "Update available";
        case OTA_STATUS_DOWNLOADING: return "Downloading...";
        case OTA_STATUS_INSTALLING:  return "Installing...";
        case OTA_STATUS_COMPLETE:    return "Complete";
        case OTA_STATUS_ERROR:       return "Error";
        default:                     return "Unknown";
    }
}
