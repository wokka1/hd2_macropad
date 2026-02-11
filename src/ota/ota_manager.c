#include "ota_manager.h"
#include "../version.h"
#include "../ble/ble_controller.h"
#include "../esp_bsp.h"

#include <string.h>
#include <stdlib.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_https_ota.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>

static const char *TAG = "OTA";

// GitHub repository for updates
#ifndef GITHUB_REPO
#define GITHUB_REPO "wokka1/hd2_macropad"
#endif

// GitHub API URL for latest release
#define GITHUB_API_URL "https://api.github.com/repos/" GITHUB_REPO "/releases/latest"

// Maximum size for GitHub API response
// We use string searching instead of full JSON parsing, so truncation is OK
// as long as tag_name and firmware.bin asset appear in the first 4KB
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

// Stack size for OTA tasks (TLS requires ~8KB minimum, image verification needs more)
// Increased to 16KB to prevent Double Exception during esp_https_ota_finish()
#define OTA_TASK_STACK_SIZE (16 * 1024)

// Size of pre-allocated buffer to reserve contiguous memory for TLS
// mbedTLS needs ~32KB contiguous internal RAM for SSL context
// Note: 40KB caused WiFi init to fail due to insufficient memory
#define OTA_TLS_RESERVE_SIZE (32 * 1024)

// Static task resources (allocated from internal RAM for flash operation safety)
static StaticTask_t s_ota_task_buffer;
static StackType_t *s_ota_task_stack = NULL;

// Pre-allocated buffer to reserve contiguous memory before BLE fragments heap
// This is allocated at boot and freed just before TLS operations
static void *s_tls_reserve_buffer = NULL;

// Extract a JSON string value after a key (e.g., find "tag_name":"1.2.3" and return "1.2.3")
// Returns pointer to static buffer with extracted value, or NULL if not found
static const char* extract_json_string(const char *json, const char *key, char *out_buf, size_t buf_size)
{
    // Build search pattern like: "tag_name":"
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);

    const char *start = strstr(json, pattern);
    if (!start) {
        return NULL;
    }

    // Move past the pattern to the value
    start += strlen(pattern);

    // Find the closing quote
    const char *end = strchr(start, '"');
    if (!end) {
        return NULL;
    }

    // Copy the value
    size_t len = end - start;
    if (len >= buf_size) {
        len = buf_size - 1;
    }
    memcpy(out_buf, start, len);
    out_buf[len] = '\0';

    return out_buf;
}

// Find firmware.bin asset and extract its download URL
// GitHub JSON structure: "assets":[{...,"name":"firmware.bin",...,"browser_download_url":"..."},...]
static const char* extract_firmware_url(const char *json, char *out_buf, size_t buf_size)
{
    // Find the firmware.bin asset entry
    const char *firmware_pos = strstr(json, "\"name\":\"firmware.bin\"");
    if (!firmware_pos) {
        return NULL;
    }

    // Search backwards for the start of this asset object (the '{' before it)
    // and forwards for browser_download_url within this asset
    // Assets are typically ~200-300 chars, so search within a reasonable range

    // Search forward for browser_download_url (it comes after name in the asset object)
    const char *search_end = firmware_pos + 500;  // Reasonable limit
    const char *url_key = "\"browser_download_url\":\"";
    const char *url_start = strstr(firmware_pos, url_key);

    if (!url_start || url_start > search_end) {
        // Try searching backwards in case URL comes before name in this asset
        // Limit backwards search to avoid crossing into previous asset
        const char *search_start = (firmware_pos - 500 > json) ? firmware_pos - 500 : json;
        for (const char *p = firmware_pos; p >= search_start; p--) {
            if (strncmp(p, url_key, strlen(url_key)) == 0) {
                url_start = p;
                break;
            }
        }
    }

    if (!url_start) {
        return NULL;
    }

    // Extract the URL value
    url_start += strlen(url_key);
    const char *url_end = strchr(url_start, '"');
    if (!url_end) {
        return NULL;
    }

    size_t len = url_end - url_start;
    if (len >= buf_size) {
        len = buf_size - 1;
    }
    memcpy(out_buf, url_start, len);
    out_buf[len] = '\0';

    return out_buf;
}

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

    // Log initial memory state (before any allocations)
    log_memory_report("OTA Init - Before Pre-allocation");

    // PRE-ALLOCATE OTA TASK STACK
    // The task stack must be in internal RAM because during esp_https_ota_finish(),
    // flash cache is disabled which makes PSRAM inaccessible.
    //
    // NOTE: TLS reserve buffer is NO LONGER needed because CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC
    // is enabled in sdkconfig, so mbedTLS allocates from PSRAM instead of internal RAM.
    if (s_ota_task_stack == NULL) {
        s_ota_task_stack = heap_caps_malloc(OTA_TASK_STACK_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (s_ota_task_stack != NULL) {
            ESP_LOGI(TAG, "Pre-allocated OTA task stack: %d bytes from internal RAM", OTA_TASK_STACK_SIZE);
        } else {
            ESP_LOGW(TAG, "Failed to pre-allocate OTA task stack");
        }
    }

    log_memory_report("OTA Init - After Pre-allocation");

    return ESP_OK;
}

const char* ota_manager_get_version(void)
{
    return SW_VER;
}

// Free TLS reserve buffer to make contiguous memory available for TLS
static void free_tls_reserve(void)
{
    if (s_tls_reserve_buffer != NULL) {
        ESP_LOGI(TAG, "Freeing TLS reserve buffer to make %d bytes available", OTA_TLS_RESERVE_SIZE);
        heap_caps_free(s_tls_reserve_buffer);
        s_tls_reserve_buffer = NULL;
        log_memory_report("After TLS Reserve Free");
    }
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

    // Free the pre-allocated TLS reserve buffer to create contiguous space for TLS
    // This must happen AFTER BLE is disabled but BEFORE HTTP client init
    free_tls_reserve();

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

    // Retry logic for transient TLS/network failures
    #define OTA_CHECK_MAX_RETRIES 3
    #define OTA_CHECK_RETRY_DELAY_MS 2000

    for (int attempt = 1; attempt <= OTA_CHECK_MAX_RETRIES; attempt++) {
        ret = esp_http_client_perform(client);
        if (ret == ESP_OK) {
            break;  // Success
        }

        ESP_LOGW(TAG, "HTTP request failed (attempt %d/%d): %s",
                 attempt, OTA_CHECK_MAX_RETRIES, esp_err_to_name(ret));

        if (attempt < OTA_CHECK_MAX_RETRIES) {
            ESP_LOGI(TAG, "Retrying in %d ms...", OTA_CHECK_RETRY_DELAY_MS);
            vTaskDelay(pdMS_TO_TICKS(OTA_CHECK_RETRY_DELAY_MS));

            // Reinitialize client for retry (some TLS errors require this)
            esp_http_client_cleanup(client);
            client = esp_http_client_init(&config);
            if (!client) {
                ESP_LOGE(TAG, "Failed to reinitialize HTTP client for retry");
                strncpy(s_ota_info.error_message, "HTTP client init failed", sizeof(s_ota_info.error_message) - 1);
                s_ota_info.status = OTA_STATUS_ERROR;
                free(s_http_response_buffer);
                s_http_response_buffer = NULL;
                return ESP_FAIL;
            }
            esp_http_client_set_header(client, "User-Agent", "HD2-Macropad-OTA/1.0");
            esp_http_client_set_header(client, "Accept", "application/vnd.github.v3+json");

            // Reset response buffer for retry
            s_http_response_len = 0;
            s_http_response_buffer[0] = '\0';
        }
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed after %d attempts: %s", OTA_CHECK_MAX_RETRIES, esp_err_to_name(ret));
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

    // Extract version and firmware URL using string searching
    // This works even if the JSON is truncated, as long as tag_name and
    // the firmware.bin asset appear in the first 4KB of the response
    // (GitHub puts these fields before the release body/notes)

    char version_buf[32];
    if (!extract_json_string(s_http_response_buffer, "tag_name", version_buf, sizeof(version_buf))) {
        ESP_LOGE(TAG, "No tag_name found in response");
        strncpy(s_ota_info.error_message, "Invalid release format", sizeof(s_ota_info.error_message) - 1);
        s_ota_info.status = OTA_STATUS_ERROR;
        ret = ESP_FAIL;
        goto cleanup;
    }

    strncpy(s_ota_info.available_version, version_buf, sizeof(s_ota_info.available_version) - 1);
    ESP_LOGI(TAG, "Latest release: %s (current: %s)", s_ota_info.available_version, SW_VER);

    // Find firmware.bin download URL
    char url_buf[256];
    if (extract_firmware_url(s_http_response_buffer, url_buf, sizeof(url_buf))) {
        strncpy(s_ota_info.download_url, url_buf, sizeof(s_ota_info.download_url) - 1);
        ESP_LOGI(TAG, "Firmware URL: %s", s_ota_info.download_url);
    } else {
        ESP_LOGW(TAG, "firmware.bin asset not found in response (may be truncated or missing)");
    }

    // Compare versions
    int cmp = version_compare(SW_VER, s_ota_info.available_version);
    if (cmp < 0) {
        // Newer version exists
        if (s_ota_info.download_url[0] != '\0') {
            ESP_LOGI(TAG, "Update available: %s -> %s", SW_VER, s_ota_info.available_version);
            s_ota_info.status = OTA_STATUS_AVAILABLE;
        } else {
            ESP_LOGW(TAG, "Newer version %s exists but no firmware.bin asset found in release!", s_ota_info.available_version);
            strncpy(s_ota_info.error_message, "No firmware.bin in release", sizeof(s_ota_info.error_message) - 1);
            s_ota_info.status = OTA_STATUS_UP_TO_DATE;
        }
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

    // IMPORTANT: Resume LVGL and restore BLE BEFORE calling callback
    // The callback may try to update UI, which requires LVGL to be running
    ESP_LOGI(TAG, "Resuming LVGL after OTA check");
    bsp_lvgl_resume();
    ota_restore_ble();

    // Small delay to let LVGL task resume before callback updates UI
    vTaskDelay(pdMS_TO_TICKS(100));

    // Notify via callback (UI updates now safe since LVGL is resumed)
    if (s_status_callback) {
        if (ret == ESP_OK) {
            s_status_callback(s_ota_info.status,
                s_ota_info.status == OTA_STATUS_AVAILABLE ? s_ota_info.available_version : "Up to date");
        } else {
            s_status_callback(OTA_STATUS_ERROR, s_ota_info.error_message);
        }
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

    // Check that pre-allocated resources are available
    // These should have been allocated at ota_manager_init() before BLE fragmented memory
    if (s_ota_task_stack == NULL) {
        ESP_LOGE(TAG, "OTA task stack not pre-allocated - OTA unavailable");
        strncpy(s_ota_info.error_message, "OTA not initialized", sizeof(s_ota_info.error_message) - 1);
        s_ota_info.status = OTA_STATUS_ERROR;
        return ESP_ERR_INVALID_STATE;
    }

    if (s_tls_reserve_buffer == NULL) {
        ESP_LOGW(TAG, "TLS reserve buffer not available - may fail if memory fragmented");
    }

    // Log detailed memory before OTA
    log_memory_report("Before OTA Check");

    // Suspend LVGL to free internal RAM (8KB task stack + reduce fragmentation)
    // LVGL uses PSRAM buffers but its task stack is in internal RAM
    ESP_LOGI(TAG, "Suspending LVGL for OTA...");
    bsp_lvgl_suspend();
    vTaskDelay(pdMS_TO_TICKS(50));
    log_memory_report("After LVGL Suspend");

    // Disable BLE to free internal RAM for HTTPS/TLS operations
    ota_disable_ble();

    s_status_callback = status_cb;
    s_ota_busy = true;

    // Use static task creation with internal RAM stack
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
        bsp_lvgl_resume();  // Resume LVGL since we're not proceeding
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

// Maximum retries for connection errors (504, timeout, etc.)
#define OTA_MAX_RETRIES 3
#define OTA_RETRY_DELAY_MS 3000

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

    // Free the pre-allocated TLS reserve buffer if still allocated
    // (should already be freed from check, but just in case)
    free_tls_reserve();

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
    esp_err_t ret = ESP_FAIL;

    // Retry loop for connection errors (504 server errors, timeouts, etc.)
    for (int attempt = 1; attempt <= OTA_MAX_RETRIES; attempt++) {
        ESP_LOGI(TAG, "OTA connection attempt %d/%d", attempt, OTA_MAX_RETRIES);

        ret = esp_https_ota_begin(&ota_config, &ota_handle);
        if (ret == ESP_OK) {
            break;  // Success, proceed with download
        }

        ESP_LOGW(TAG, "OTA begin failed (attempt %d): %s", attempt, esp_err_to_name(ret));

        if (attempt < OTA_MAX_RETRIES) {
            ESP_LOGI(TAG, "Retrying in %d seconds...", OTA_RETRY_DELAY_MS / 1000);
            vTaskDelay(pdMS_TO_TICKS(OTA_RETRY_DELAY_MS));
        }
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OTA begin failed after %d attempts", OTA_MAX_RETRIES);
        strncpy(s_ota_info.error_message, "Connection failed", sizeof(s_ota_info.error_message) - 1);
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

        // Calculate and report progress (only when percentage changes)
        int downloaded = esp_https_ota_get_image_len_read(ota_handle);
        if (total_size > 0) {
            int new_percent = (downloaded * 100) / total_size;
            if (new_percent != s_ota_info.progress_percent) {
                s_ota_info.progress_percent = new_percent;
                if (progress_cb) {
                    progress_cb(new_percent);
                }
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

    // Skip progress callback here - device is about to reboot anyway
    // and calling lv_async_call during esp_https_ota_finish can cause crashes
    ESP_LOGI(TAG, "Download complete, finalizing update...");
    log_memory_report("Before OTA Finish");

    // CRITICAL: Suspend LVGL before esp_https_ota_finish()
    // During flash operations, cache is disabled making PSRAM inaccessible.
    // LVGL uses PSRAM buffers, so it MUST be suspended to prevent crashes.
    bsp_lvgl_suspend();

    // Give time for LVGL to fully suspend
    vTaskDelay(pdMS_TO_TICKS(100));

    log_memory_report("After LVGL Suspend");

    ret = esp_https_ota_finish(ota_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA update successful! Rebooting in 2 seconds...");
        s_ota_info.status = OTA_STATUS_COMPLETE;
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    } else {
        // Resume LVGL on failure
        bsp_lvgl_resume();

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

    // Check that pre-allocated task stack is available
    if (s_ota_task_stack == NULL) {
        ESP_LOGE(TAG, "OTA task stack not pre-allocated - OTA unavailable");
        return ESP_ERR_INVALID_STATE;
    }

    // NOTE: We do NOT suspend LVGL here - keep it running during download
    // so progress bar updates work. LVGL is only suspended right before
    // esp_https_ota_finish() in ota_perform_internal() when flash cache
    // is disabled and PSRAM becomes inaccessible.

    // Disable BLE to free memory for TLS operations
    ota_disable_ble();

    s_progress_callback = progress_cb;
    s_status_callback = status_cb;
    s_ota_busy = true;

    // Use static task creation with internal RAM stack
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

// Static buffer to hold previous version string
static char s_previous_version[32] = {0};

bool ota_manager_has_previous_version(void)
{
    // Get the partition that would be used for the next OTA update
    // This is the "other" partition - the inactive one
    const esp_partition_t *inactive = esp_ota_get_next_update_partition(NULL);
    if (inactive == NULL) {
        return false;
    }

    // Try to read the app description from the inactive partition
    esp_app_desc_t app_desc;
    esp_err_t ret = esp_ota_get_partition_description(inactive, &app_desc);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "No valid app in inactive partition: %s", esp_err_to_name(ret));
        return false;
    }

    // Check if it looks like a valid version (not empty or all 0xFF)
    if (app_desc.version[0] == '\0' || app_desc.version[0] == 0xFF) {
        return false;
    }

    // Cache the version string
    strncpy(s_previous_version, app_desc.version, sizeof(s_previous_version) - 1);
    s_previous_version[sizeof(s_previous_version) - 1] = '\0';

    ESP_LOGI(TAG, "Previous version available: %s (partition: %s at 0x%lx)",
             s_previous_version, inactive->label, inactive->address);
    return true;
}

const char* ota_manager_get_previous_version(void)
{
    // Check if we already have the version cached
    if (s_previous_version[0] != '\0') {
        return s_previous_version;
    }

    // Try to get it
    if (ota_manager_has_previous_version()) {
        return s_previous_version;
    }

    return NULL;
}

esp_err_t ota_manager_rollback(void)
{
    // Get the inactive partition
    const esp_partition_t *inactive = esp_ota_get_next_update_partition(NULL);
    if (inactive == NULL) {
        ESP_LOGE(TAG, "No inactive partition found for rollback");
        return ESP_FAIL;
    }

    // Verify there's a valid app in the inactive partition
    esp_app_desc_t app_desc;
    esp_err_t ret = esp_ota_get_partition_description(inactive, &app_desc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "No valid app in inactive partition: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Rolling back to version %s (partition: %s at 0x%lx)",
             app_desc.version, inactive->label, inactive->address);

    // Set the inactive partition as the boot partition
    ret = esp_ota_set_boot_partition(inactive);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set boot partition: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Boot partition set, rebooting in 2 seconds...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    // Should not reach here
    return ESP_OK;
}
