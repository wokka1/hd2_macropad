#ifndef EEZ_LVGL_UI_EVENTS_H
#define EEZ_LVGL_UI_EVENTS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void action_reset_stratagems(lv_event_t * e);
extern void action_reset_confirm(lv_event_t * e);
extern void action_get_preset(lv_event_t * e);
extern void action_set_preset(lv_event_t * e);
extern void action_change_brightness(lv_event_t * e);
extern void action_flip_screen(lv_event_t * e);
extern void action_mute_sound(lv_event_t * e);
extern void action_change_delay(lv_event_t * e);
extern void action_change_connectivity(lv_event_t * e);
extern void action_change_keymap(lv_event_t * e);
extern void action_keyboard_demo(lv_event_t * e);
extern void action_select_stratagem(lv_event_t * e);
extern void action_deselect_stratagem(lv_event_t * e);
extern void action_tab_changed(lv_event_t * e);
extern void action_trigger_stratagem_base(lv_event_t * e);
extern void action_trigger_stratagem_user(lv_event_t * e);
extern void action_restart_device(lv_event_t * e);
extern void action_assign_stratagems(lv_event_t * e);
extern void action_goto_game(lv_event_t * e);
extern void action_enable_image_mode(lv_event_t * e);
extern void action_assign_preset_image(lv_event_t * e);
extern void action_clear_preset_image(lv_event_t * e);
extern void action_reset_presets(lv_event_t * e);
extern void action_disable_image_mode(lv_event_t * e);
extern void action_manual_execute(lv_event_t * e);
extern void action_finalize_manual_execution(lv_event_t * e);
extern void action_action_toggle_debug(lv_event_t * e);
extern void action_action_toggle_cooldowns(lv_event_t * e);
extern void action_action_clear_cooldowns(lv_event_t * e);
extern void action_action_wifi_scan(lv_event_t * e);
extern void action_action_wifi_connect_selected(lv_event_t * e);
extern void action_action_wifi_connect_manual(lv_event_t * e);
extern void action_action_enable_wifi(lv_event_t * e);
extern void action_hide_keyboard(lv_event_t * e);
extern void action_show_keyboard(lv_event_t * e);
extern void action_action_ota_check(lv_event_t * e);
extern void action_action_ota_install(lv_event_t * e);
extern const char* action_get_current_version(void);

// OTA UI element pointers (defined in actions.c, set in ui_post.c)
extern lv_obj_t *ota_status_label;
extern lv_obj_t *ota_progress_bar;
extern lv_obj_t *ota_available_label;
extern lv_obj_t *ota_install_btn;
extern lv_obj_t *ota_check_btn;


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_EVENTS_H*/