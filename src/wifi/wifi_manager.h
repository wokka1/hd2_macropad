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
 * @brief Deinitialize WiFi manager and free resources
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_deinit(void);

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

/**
 * @brief Get saved SSID from NVS
 * @param ssid Buffer to store SSID (must be at least 33 bytes)
 * @param max_len Maximum length of buffer
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if no credentials saved
 */
esp_err_t wifi_manager_get_saved_ssid(char *ssid, size_t max_len);

#ifdef __cplusplus
}
#endif
