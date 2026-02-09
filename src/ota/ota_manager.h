#pragma once

#include <esp_err.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// OTA update status
typedef enum {
    OTA_STATUS_IDLE,
    OTA_STATUS_CHECKING,
    OTA_STATUS_UP_TO_DATE,
    OTA_STATUS_AVAILABLE,
    OTA_STATUS_DOWNLOADING,
    OTA_STATUS_INSTALLING,
    OTA_STATUS_COMPLETE,
    OTA_STATUS_ERROR
} ota_status_t;

// OTA information structure
typedef struct {
    char current_version[16];
    char available_version[16];
    char download_url[256];
    ota_status_t status;
    int progress_percent;
    char error_message[64];
} ota_info_t;

// Callback types
typedef void (*ota_progress_callback_t)(int progress_percent);
typedef void (*ota_status_callback_t)(ota_status_t status, const char *message);

/**
 * @brief Initialize OTA manager
 * @return ESP_OK on success
 */
esp_err_t ota_manager_init(void);

/**
 * @brief Get current firmware version
 * @return Version string
 */
const char* ota_manager_get_version(void);

/**
 * @brief Check for available updates from GitHub
 *
 * This is a blocking call that queries the GitHub API.
 * Updates the internal ota_info structure with results.
 *
 * @return ESP_OK if check completed (regardless of update availability)
 *         ESP_FAIL on network or parse error
 */
esp_err_t ota_manager_check_for_update(void);

/**
 * @brief Get current OTA information
 * @return Pointer to ota_info structure (do not free)
 */
const ota_info_t* ota_manager_get_info(void);

/**
 * @brief Check if an update is available
 * @return true if update available, false otherwise
 */
bool ota_manager_update_available(void);

/**
 * @brief Perform OTA update from previously checked URL
 *
 * Downloads and installs the firmware. On success, the device will reboot.
 *
 * @param progress_cb Callback for progress updates (0-100%)
 * @return ESP_OK if update successful (device will reboot)
 *         ESP_FAIL on error
 */
esp_err_t ota_manager_perform_update(ota_progress_callback_t progress_cb);

/**
 * @brief Mark current firmware as valid (call after successful boot)
 *
 * This prevents automatic rollback to previous firmware.
 *
 * @return ESP_OK on success
 */
esp_err_t ota_manager_confirm_update(void);

/**
 * @brief Get status string for current OTA status
 * @param status OTA status value
 * @return Human-readable status string
 */
const char* ota_manager_status_str(ota_status_t status);

#ifdef __cplusplus
}
#endif
