#include "main.h"
#include "ui.h"
#include "screens.h"
#include "ui_events.h"
#include "configration.h"
#include "esp_log.h"
#include <string.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../wifi/wifi_manager.h"
#include "../ota/ota_manager.h"

// Global variables for WiFi scan results
#define MAX_WIFI_APS 20
static wifi_ap_record_simple_t wifi_scan_results[MAX_WIFI_APS];
static uint16_t wifi_scan_count = 0;
static bool wifi_initialized = false;

// Change HID input delay
void action_change_delay(lv_event_t *e)
{
	int32_t delay = lv_slider_get_value(e->current_target);

	setDelay(delay * 10, false);
}

// Change display brightness
void action_change_brightness(lv_event_t *e)
{
	int32_t brightness = lv_slider_get_value(e->current_target);

	setBrightness(brightness * 10, false);
}

// Change sound mute (on/off)
void action_mute_sound(lv_event_t *e)
{
	bool muted = lv_obj_get_state(e->current_target) & LV_STATE_CHECKED ? true : false;

	setMuted(muted, false);
}

// Toggle debug logging
void action_action_toggle_debug(lv_event_t *e)
{
	bool debug = lv_obj_get_state(e->current_target) & LV_STATE_CHECKED ? true : false;

	setDebugLogging(debug, false);

	// Immediate feedback to user
	if (debug)
	{
		ESP_LOGI("Config", "Debug logging ENABLED");
	}
	else
	{
		ESP_LOGI("Config", "Debug logging DISABLED");
	}
}

// Toggle cooldown display (switch is labeled "Disable", so invert logic)
void action_action_toggle_cooldowns(lv_event_t *e)
{
	bool disable = lv_obj_get_state(e->current_target) & LV_STATE_CHECKED ? true : false;

	setShowCooldowns(!disable, false);  // Invert: checked = disable = hide cooldowns

	// Immediate feedback to user
	ESP_LOGI("Config", "Cooldown display %s", disable ? "DISABLED (hidden)" : "ENABLED (visible)");
}

// Trigger when tab navigation has changed
void action_tab_changed(lv_event_t *e)
{
	playbackSound(SND_SWIPE);
}

// Change screen orientation
void action_flip_screen(lv_event_t *e)
{
	bool flip = lv_obj_get_state(e->current_target) & LV_STATE_CHECKED ? true : false;

	setRotation(flip ? LV_DISP_ROT_270 : LV_DISP_ROT_90, false);

	esp_restart();
}

void action_restart_device(lv_event_t *e)
{
	esp_restart();
}

void action_reset_confirm(lv_event_t *e)
{
	resetConfig();
}

void action_assign_stratagems(lv_event_t *e)
{
	assignStratagems();
}

void action_enable_image_mode(lv_event_t *e)
{
	enableImageMode();
}

void action_reset_presets(lv_event_t *e)
{
	resetPresets();
}

void action_disable_image_mode(lv_event_t *e)
{
	disableImageMode();
}

void action_finalize_manual_execution(lv_event_t *e)
{
	finalizeManualExecution();
}

void action_config_2_setup(lv_event_t *e)
{
	lv_scr_load_anim(objects.setup, LV_SCR_LOAD_ANIM_MOVE_LEFT, 1000, 0, false);
}

void action_game_2_setup(lv_event_t *e)
{
	resetCooldowns();

	lv_scr_load_anim(objects.setup, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 1000, 0, false);
}

void action_game_2_mission(lv_event_t *e)
{
	lv_scr_load_anim(objects.mission, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}

void action_mission_2_game(lv_event_t *e)
{
	lv_scr_load_anim(objects.game, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}

// Clear all cooldowns without triggering buzzer
void action_action_clear_cooldowns(lv_event_t *e)
{
	extern lv_obj_t *cooldownLabels[MAX_USER_STRATAGEMS];
	extern uint64_t cooldownValues[MAX_USER_STRATAGEMS];
	extern bool cooldownBeepTriggered[MAX_USER_STRATAGEMS];
	extern uint64_t resupplyCooldownValue;
	extern bool resupplyBeepTriggered;
	extern lv_obj_t *labelSupplies;

	// Clear all custom stratagem cooldowns
	for (uint8_t c = 0; c < MAX_USER_STRATAGEMS; c++)
	{
		cooldownValues[c] = 0;
		cooldownBeepTriggered[c] = false;

		// Hide cooldown label if visible
		if (cooldownLabels[c] && !lv_obj_has_flag(cooldownLabels[c], LV_OBJ_FLAG_HIDDEN))
		{
			lv_obj_add_flag(cooldownLabels[c], LV_OBJ_FLAG_HIDDEN);
		}
	}

	// Clear resupply cooldown
	resupplyCooldownValue = 0;
	resupplyBeepTriggered = false;

	// Hide resupply label if visible
	if (labelSupplies && !lv_obj_has_flag(labelSupplies, LV_OBJ_FLAG_HIDDEN))
	{
		lv_obj_add_flag(labelSupplies, LV_OBJ_FLAG_HIDDEN);
	}

	ESP_LOGI("Actions", "All cooldowns cleared (no buzzer)");
}

// Show on-screen keyboard and link to focused textarea
void action_show_keyboard(lv_event_t *e)
{
	lv_obj_t *textarea = lv_event_get_target(e);

	// Link keyboard to the textarea that triggered this event
	lv_keyboard_set_textarea(objects.keyboard, textarea);

	// Show the keyboard
	lv_obj_clear_flag(objects.keyboard, LV_OBJ_FLAG_HIDDEN);
}

// Hide on-screen keyboard
void action_hide_keyboard(lv_event_t *e)
{
	// Unlink keyboard from any textarea
	lv_keyboard_set_textarea(objects.keyboard, NULL);

	// Hide the keyboard
	lv_obj_add_flag(objects.keyboard, LV_OBJ_FLAG_HIDDEN);
}

// Enable/disable WiFi based on switch state
void action_action_enable_wifi(lv_event_t *e)
{
	lv_obj_t *switch_obj = lv_event_get_target(e);
	// Note: Switch state is inverted in UI
	bool is_checked = !lv_obj_has_state(switch_obj, LV_STATE_CHECKED);

	if (is_checked && !wifi_initialized) {
		// Enable WiFi
		lv_label_set_text(objects.wifi_status_label, "Initializing WiFi...");
		esp_err_t ret = wifi_manager_init();

		if (ret == ESP_OK) {
			wifi_initialized = true;
			// Brief delay to ensure WiFi is fully started
			vTaskDelay(pdMS_TO_TICKS(500));

			// Try to auto-connect with stored credentials
			lv_label_set_text(objects.wifi_status_label, "Connecting...");
			ESP_LOGI("Actions", "WiFi initialized, attempting auto-connect...");

			ret = wifi_manager_connect();
			if (ret == ESP_OK) {
				// Show which network we connected to
				char ssid[33];
				if (wifi_manager_get_saved_ssid(ssid, sizeof(ssid)) == ESP_OK) {
					char status[64];
					snprintf(status, sizeof(status), "Connected to %s", ssid);
					lv_label_set_text(objects.wifi_status_label, status);
					ESP_LOGI("Actions", "Auto-connected to %s", ssid);
				} else {
					lv_label_set_text(objects.wifi_status_label, "Connected!");
					ESP_LOGI("Actions", "Auto-connected to saved network");
				}
			} else {
				// No saved credentials or connection failed - that's OK
				lv_label_set_text(objects.wifi_status_label, "WiFi Ready - Scan for networks");
				ESP_LOGI("Actions", "No saved credentials or auto-connect failed, ready for manual connect");
			}
		} else {
			lv_label_set_text(objects.wifi_status_label, "WiFi init failed!");
			// Uncheck the switch on failure
			lv_obj_clear_state(switch_obj, LV_STATE_CHECKED);
			ESP_LOGE("Actions", "WiFi initialization failed: %s", esp_err_to_name(ret));
		}
	} else if (!is_checked && wifi_initialized) {
		// Disable WiFi
		lv_label_set_text(objects.wifi_status_label, "Disabling WiFi...");
		esp_err_t ret = wifi_manager_deinit();
		if (ret == ESP_OK) {
			wifi_initialized = false;
			lv_label_set_text(objects.wifi_status_label, "WiFi Disabled");
			ESP_LOGI("Actions", "WiFi disabled");
		} else {
			lv_label_set_text(objects.wifi_status_label, "WiFi disable failed!");
			ESP_LOGE("Actions", "WiFi deinit failed: %s", esp_err_to_name(ret));
		}
	}
}

// Scan for WiFi networks and populate dropdown
void action_action_wifi_scan(lv_event_t *e)
{
	// Check if WiFi is initialized
	if (!wifi_initialized) {
		lv_label_set_text(objects.wifi_status_label, "Enable WiFi first!");
		return;
	}

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
void action_action_wifi_connect_selected(lv_event_t *e)
{
	// Check if WiFi is initialized
	if (!wifi_initialized) {
		lv_label_set_text(objects.wifi_status_label, "Enable WiFi first!");
		return;
	}

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
	const char *ssid = wifi_scan_results[selected].ssid;
	lv_label_set_text(objects.wifi_status_label, "Connecting...");
	esp_err_t ret = wifi_manager_connect_to(ssid, password);

	if (ret == ESP_OK) {
		char status[64];
		snprintf(status, sizeof(status), "Connected to %s", ssid);
		lv_label_set_text(objects.wifi_status_label, status);
	} else {
		lv_label_set_text(objects.wifi_status_label, "Connection failed");
	}
}

// Manual WiFi connection (for hidden networks)
void action_action_wifi_connect_manual(lv_event_t *e)
{
	// Check if WiFi is initialized
	if (!wifi_initialized) {
		lv_label_set_text(objects.wifi_status_label, "Enable WiFi first!");
		return;
	}

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
		char status[64];
		snprintf(status, sizeof(status), "Connected to %s", ssid);
		lv_label_set_text(objects.wifi_status_label, status);
	} else {
		lv_label_set_text(objects.wifi_status_label, "Connection failed");
	}
}

// ============================================================================
// OTA Update Actions
// ============================================================================

// Progress callback to update UI during OTA download (called from OTA task)
static void ota_progress_callback(int progress_percent)
{
	// Update progress bar
	if (objects.bar_ota_progress) {
		lv_bar_set_value(objects.bar_ota_progress, progress_percent, LV_ANIM_OFF);
	}

	// Update status label with progress
	if (objects.lbl_ota_status) {
		char buf[32];
		snprintf(buf, sizeof(buf), "Downloading... %d%%", progress_percent);
		lv_label_set_text(objects.lbl_ota_status, buf);
	}

	ESP_LOGI("OTA", "Progress: %d%%", progress_percent);
}

// Status callback for OTA check (called from OTA task when complete)
static void ota_check_status_callback(ota_status_t status, const char *message)
{
	ESP_LOGI("OTA", "Check complete, status: %d, message: %s", status, message ? message : "none");

	// Re-enable check button
	if (objects.btn_check_update) {
		lv_obj_clear_state(objects.btn_check_update, LV_STATE_DISABLED);
	}

	const ota_info_t *info = ota_manager_get_info();

	if (status == OTA_STATUS_ERROR) {
		if (objects.lbl_ota_status) {
			lv_label_set_text(objects.lbl_ota_status, message ? message : "Check failed");
		}
		return;
	}

	if (status == OTA_STATUS_AVAILABLE) {
		// Update available
		if (objects.lbl_ota_status) {
			char buf[64];
			snprintf(buf, sizeof(buf), "Update available: %s", info->available_version);
			lv_label_set_text(objects.lbl_ota_status, buf);
		}

		// Show available version label
		if (objects.lbl_available_version) {
			char buf[32];
			snprintf(buf, sizeof(buf), "Available: %s", info->available_version);
			lv_label_set_text(objects.lbl_available_version, buf);
			lv_obj_clear_flag(objects.lbl_available_version, LV_OBJ_FLAG_HIDDEN);
		}

		// Show install button
		if (objects.btn_install_update) {
			lv_obj_clear_flag(objects.btn_install_update, LV_OBJ_FLAG_HIDDEN);
		}
	} else {
		// Already up to date
		if (objects.lbl_ota_status) {
			lv_label_set_text(objects.lbl_ota_status, "Already up to date!");
		}

		// Hide install button
		if (objects.btn_install_update) {
			lv_obj_add_flag(objects.btn_install_update, LV_OBJ_FLAG_HIDDEN);
		}
	}
}

// Status callback for OTA update (called from OTA task on failure)
static void ota_update_status_callback(ota_status_t status, const char *message)
{
	ESP_LOGI("OTA", "Update status: %d, message: %s", status, message ? message : "none");

	// If we get here, update failed (success causes reboot)
	if (objects.lbl_ota_status) {
		lv_label_set_text(objects.lbl_ota_status, message ? message : "Update failed!");
	}

	// Re-enable buttons
	if (objects.btn_install_update) {
		lv_obj_clear_state(objects.btn_install_update, LV_STATE_DISABLED);
	}
	if (objects.btn_check_update) {
		lv_obj_clear_state(objects.btn_check_update, LV_STATE_DISABLED);
	}

	// Hide progress bar
	if (objects.bar_ota_progress) {
		lv_obj_add_flag(objects.bar_ota_progress, LV_OBJ_FLAG_HIDDEN);
	}
}

// Check for firmware updates from GitHub
void action_action_ota_check(lv_event_t *e)
{
	// Check if WiFi is connected
	if (wifi_manager_get_status() != WIFI_STATUS_CONNECTED) {
		if (objects.lbl_ota_status) {
			lv_label_set_text(objects.lbl_ota_status, "Connect to WiFi first!");
		}
		ESP_LOGW("OTA", "Cannot check for updates: WiFi not connected");
		return;
	}

	// Check if OTA operation already in progress
	if (ota_manager_is_busy()) {
		if (objects.lbl_ota_status) {
			lv_label_set_text(objects.lbl_ota_status, "OTA operation in progress...");
		}
		return;
	}

	// Update UI to show checking status
	if (objects.lbl_ota_status) {
		lv_label_set_text(objects.lbl_ota_status, "Checking for updates...");
	}

	// Disable check button during check
	if (objects.btn_check_update) {
		lv_obj_add_state(objects.btn_check_update, LV_STATE_DISABLED);
	}

	ESP_LOGI("OTA", "Starting async update check...");

	// Start async check - callback will update UI when complete
	esp_err_t ret = ota_manager_check_for_update_async(ota_check_status_callback);
	if (ret != ESP_OK) {
		if (objects.lbl_ota_status) {
			lv_label_set_text(objects.lbl_ota_status, "Failed to start check");
		}
		if (objects.btn_check_update) {
			lv_obj_clear_state(objects.btn_check_update, LV_STATE_DISABLED);
		}
		ESP_LOGE("OTA", "Failed to start async check: %s", esp_err_to_name(ret));
	}
}

// Install the available update
void action_action_ota_install(lv_event_t *e)
{
	const ota_info_t *info = ota_manager_get_info();

	// Verify update is available
	if (info->status != OTA_STATUS_AVAILABLE) {
		if (objects.lbl_ota_status) {
			lv_label_set_text(objects.lbl_ota_status, "No update available");
		}
		ESP_LOGW("OTA", "No update available to install");
		return;
	}

	// Check WiFi connection
	if (wifi_manager_get_status() != WIFI_STATUS_CONNECTED) {
		if (objects.lbl_ota_status) {
			lv_label_set_text(objects.lbl_ota_status, "WiFi disconnected!");
		}
		ESP_LOGW("OTA", "WiFi disconnected, cannot install update");
		return;
	}

	// Check if OTA operation already in progress
	if (ota_manager_is_busy()) {
		if (objects.lbl_ota_status) {
			lv_label_set_text(objects.lbl_ota_status, "OTA operation in progress...");
		}
		return;
	}

	// Show progress bar
	if (objects.bar_ota_progress) {
		lv_bar_set_value(objects.bar_ota_progress, 0, LV_ANIM_OFF);
		lv_obj_clear_flag(objects.bar_ota_progress, LV_OBJ_FLAG_HIDDEN);
	}

	// Disable buttons during update
	if (objects.btn_install_update) {
		lv_obj_add_state(objects.btn_install_update, LV_STATE_DISABLED);
	}
	if (objects.btn_check_update) {
		lv_obj_add_state(objects.btn_check_update, LV_STATE_DISABLED);
	}

	if (objects.lbl_ota_status) {
		lv_label_set_text(objects.lbl_ota_status, "Starting update...");
	}

	ESP_LOGI("OTA", "Starting async firmware update to v%s...", info->available_version);

	// Start async update - callback will handle failure (success causes reboot)
	esp_err_t ret = ota_manager_perform_update_async(ota_progress_callback, ota_update_status_callback);
	if (ret != ESP_OK) {
		if (objects.lbl_ota_status) {
			lv_label_set_text(objects.lbl_ota_status, "Failed to start update");
		}
		// Re-enable buttons
		if (objects.btn_install_update) {
			lv_obj_clear_state(objects.btn_install_update, LV_STATE_DISABLED);
		}
		if (objects.btn_check_update) {
			lv_obj_clear_state(objects.btn_check_update, LV_STATE_DISABLED);
		}
		// Hide progress bar
		if (objects.bar_ota_progress) {
			lv_obj_add_flag(objects.bar_ota_progress, LV_OBJ_FLAG_HIDDEN);
		}
		ESP_LOGE("OTA", "Failed to start update task: %s", esp_err_to_name(ret));
	}
}

// Get current version string for display
const char* action_get_current_version(void)
{
	return ota_manager_get_version();
}