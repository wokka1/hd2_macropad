#include "main.h"
#include "ui.h"
#include "screens.h"
#include "ui_events.h"
#include "configration.h"
#include "esp_log.h"

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