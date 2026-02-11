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
 * @brief Check for available updates from GitHub (async, runs in separate task)
 *
 * Spawns a FreeRTOS task with adequate stack for TLS operations.
 * Call ota_manager_is_busy() to check if operation is in progress.
 * Call ota_manager_get_info() to get results after completion.
 *
 * @param status_cb Callback for status updates (called from task context)
 * @return ESP_OK if task started successfully
 */
esp_err_t ota_manager_check_for_update_async(ota_status_callback_t status_cb);

/**
 * @brief Check if an OTA operation is currently in progress
 * @return true if busy, false if idle
 */
bool ota_manager_is_busy(void);

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
 * @brief Perform OTA update from previously checked URL (async, runs in separate task)
 *
 * Spawns a FreeRTOS task with adequate stack for TLS operations.
 * On success, the device will reboot automatically.
 *
 * @param progress_cb Callback for progress updates (0-100%)
 * @param status_cb Callback for status updates
 * @return ESP_OK if task started successfully
 */
esp_err_t ota_manager_perform_update_async(ota_progress_callback_t progress_cb, ota_status_callback_t status_cb);

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

/**
 * @brief Check if a previous firmware version is available for rollback
 * @return true if previous version exists in inactive partition
 */
bool ota_manager_has_previous_version(void);

/**
 * @brief Get the version of the previous firmware (from inactive partition)
 * @return Version string, or NULL if no previous version available
 */
const char* ota_manager_get_previous_version(void);

/**
 * @brief Rollback to the previous firmware version
 *
 * Sets the inactive partition as the boot partition and reboots.
 * This function does not return on success (device reboots).
 *
 * @return ESP_FAIL if rollback failed, does not return on success
 */
esp_err_t ota_manager_rollback(void);

#ifdef __cplusplus
}
#endif
