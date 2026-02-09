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

static EventGroupHandle_t s_wifi_event_group = NULL;
static wifi_status_t s_wifi_status = WIFI_STATUS_DISCONNECTED;
static int s_retry_num = 0;
static esp_event_handler_instance_t s_instance_any_id = NULL;
static esp_event_handler_instance_t s_instance_got_ip = NULL;
static esp_netif_t *s_sta_netif = NULL;
static bool s_wifi_initialized = false;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        // Don't auto-connect on start - wait for explicit connect request
        ESP_LOGI(TAG, "WiFi started, ready for scan/connect");
        s_wifi_status = WIFI_STATUS_DISCONNECTED;
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
    if (s_wifi_initialized) {
        ESP_LOGW(TAG, "WiFi already initialized");
        return ESP_OK;
    }

    // Create event group if not already created
    if (s_wifi_event_group == NULL) {
        s_wifi_event_group = xEventGroupCreate();
        if (s_wifi_event_group == NULL) {
            ESP_LOGE(TAG, "Failed to create event group");
            return ESP_FAIL;
        }
    }

    // Initialize network interface (safe to call multiple times)
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to init netif: %s", esp_err_to_name(ret));
        return ret;
    }

    // Create default event loop (safe to call multiple times)
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        return ret;
    }

    // Create default WiFi station netif (only if not already created)
    if (s_sta_netif == NULL) {
        s_sta_netif = esp_netif_create_default_wifi_sta();
        if (s_sta_netif == NULL) {
            ESP_LOGE(TAG, "Failed to create WiFi STA netif");
            return ESP_FAIL;
        }

        // Set custom hostname
        ret = esp_netif_set_hostname(s_sta_netif, "hd2-macropad");
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to set hostname: %s", esp_err_to_name(ret));
            // Non-fatal, continue anyway
        } else {
            ESP_LOGI(TAG, "Hostname set to: hd2-macropad");
        }
    }

    // Initialize WiFi with reduced buffer counts for memory-constrained environment
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    // Reduce buffer counts to save memory (defaults are too large)
    cfg.dynamic_rx_buf_num = 12;      // Default: 32
    cfg.static_rx_buf_num = 6;        // Default: 10
    cfg.static_tx_buf_num = 6;        // Default: 16
    cfg.tx_buf_type = 0;              // Static TX buffer (type 1 = dynamic needs dynamic_tx_buf_num set)
    cfg.cache_tx_buf_num = 12;        // Default: 32
    cfg.ampdu_rx_enable = 0;          // Disable AMPDU RX to save memory
    cfg.ampdu_tx_enable = 0;          // Disable AMPDU TX to save memory

    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register event handlers
    ret = esp_event_handler_instance_register(WIFI_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               &wifi_event_handler,
                                               NULL,
                                               &s_instance_any_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(ret));
        esp_wifi_deinit();
        return ret;
    }

    ret = esp_event_handler_instance_register(IP_EVENT,
                                               IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler,
                                               NULL,
                                               &s_instance_got_ip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(ret));
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_instance_any_id);
        esp_wifi_deinit();
        return ret;
    }

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    s_wifi_initialized = true;
    ESP_LOGI(TAG, "WiFi manager initialized");
    return ESP_OK;
}

esp_err_t wifi_manager_deinit(void)
{
    if (!s_wifi_initialized) {
        ESP_LOGW(TAG, "WiFi not initialized");
        return ESP_OK;
    }

    // Disconnect if connected
    wifi_manager_disconnect();

    // Stop WiFi
    esp_wifi_stop();

    // Unregister event handlers
    if (s_instance_any_id) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_instance_any_id);
        s_instance_any_id = NULL;
    }
    if (s_instance_got_ip) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_instance_got_ip);
        s_instance_got_ip = NULL;
    }

    // Deinitialize WiFi
    esp_wifi_deinit();

    // Destroy netif (only on complete deinit)
    if (s_sta_netif) {
        esp_netif_destroy(s_sta_netif);
        s_sta_netif = NULL;
    }

    s_wifi_initialized = false;
    s_wifi_status = WIFI_STATUS_DISCONNECTED;

    ESP_LOGI(TAG, "WiFi manager deinitialized");
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
            // Set authmode based on whether password is provided
            .threshold.authmode = (password == NULL || strlen(password) == 0) ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK,
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

    // Clear any previous event bits
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    s_retry_num = 0;
    s_wifi_status = WIFI_STATUS_CONNECTING;

    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);

    // Actually initiate the connection
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start connection: %s", esp_err_to_name(ret));
        s_wifi_status = WIFI_STATUS_FAILED;
        return ret;
    }

    // Wait for connection with 30 second timeout
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdTRUE,  // Clear bits on exit
                                           pdFALSE,
                                           pdMS_TO_TICKS(30000));  // 30 second timeout

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi");
        // Store credentials on successful connection
        wifi_manager_set_credentials(ssid, password);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
        return ESP_FAIL;
    } else {
        // Timeout occurred
        ESP_LOGE(TAG, "Connection timeout after 30 seconds");
        s_wifi_status = WIFI_STATUS_FAILED;
        esp_wifi_disconnect();  // Cancel the connection attempt
        return ESP_ERR_TIMEOUT;
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

esp_err_t wifi_manager_get_saved_ssid(char *ssid, size_t max_len)
{
    if (ssid == NULL || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }

    err = nvs_get_str(nvs_handle, "wifi_ssid", ssid, &max_len);
    nvs_close(nvs_handle);

    return err;
}
