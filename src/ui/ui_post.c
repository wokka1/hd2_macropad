#include "main.h"
#include "screens.h"
#include "ui.h"
#include "ui_events.h"
#include "../ota/ota_manager.h"
#include <stdio.h>


extern lv_obj_t *cooldownLabels[MAX_USER_STRATAGEMS];
lv_obj_t *labelSupplies = NULL;  // Will be initialized from EEZ Studio


void ui_post()
{
  lv_obj_t *tabsBtnsList[] = {
      lv_tabview_get_tab_btns(objects.tab_view_setup),
      lv_tabview_get_tab_btns(objects.tab_view_config)};

  lv_color_t itemColorActive = lv_color_hex(colorActive);

  for (uint8_t c = 0; c < 2; c++)
  {
    lv_obj_t *tabBtnsItem = tabsBtnsList[c];

    lv_obj_set_style_border_color(tabBtnsItem, itemColorActive, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(tabBtnsItem, 2, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(tabBtnsItem, itemColorActive, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(tabBtnsItem, lv_color_hex(0x999999), LV_PART_ITEMS | LV_STATE_CHECKED);
  }

  lv_obj_t *dropDownList[] = {
      lv_dropdown_get_list(objects.dd_connectivity),
      lv_dropdown_get_list(objects.dd_keymap)};

  for (uint8_t c = 0; c < 2; c++)
  {
    lv_obj_t *dropDownItem = dropDownList[c];

    lv_obj_set_style_text_color(dropDownItem, lv_color_hex(0x000000), LV_PART_SELECTED | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(dropDownItem, itemColorActive, LV_PART_SELECTED | LV_STATE_CHECKED);
  }

  lv_obj_add_flag(objects.msg_box, LV_OBJ_FLAG_HIDDEN);

  cooldownLabels[0] = objects.label_cooldown1;
  cooldownLabels[1] = objects.label_cooldown2;
  cooldownLabels[2] = objects.label_cooldown3;
  cooldownLabels[3] = objects.label_cooldown4;
  cooldownLabels[4] = objects.label_cooldown5;
  cooldownLabels[5] = objects.label_cooldown6;

  // Initialize Resupply cooldown label
  labelSupplies = objects.label_supplies;

  // Position cooldown labels - COMMENTED OUT: Using EEZ Studio positions instead
  // If you want code-based positioning, uncomment these lines:
  // lv_obj_align_to(objects.label_cooldown1, objects.custom_stratagem1, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
  // lv_obj_align_to(objects.label_cooldown2, objects.custom_stratagem2, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
  // lv_obj_align_to(objects.label_cooldown3, objects.custom_stratagem3, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
  // lv_obj_align_to(objects.label_cooldown4, objects.custom_stratagem4, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
  // lv_obj_align_to(objects.label_cooldown5, objects.custom_stratagem5, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
  // lv_obj_align_to(objects.label_cooldown6, objects.custom_stratagem6, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

  // Set cooldown label style
  lv_obj_set_style_text_color(objects.label_cooldown1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(objects.label_cooldown2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(objects.label_cooldown3, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(objects.label_cooldown4, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(objects.label_cooldown5, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(objects.label_cooldown6, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

  resetCooldowns();

  // ============================================================================
  // OTA Update UI Setup
  // ============================================================================

  // Set current version label
  if (objects.lbl_current_version) {
    char version_text[32];
    snprintf(version_text, sizeof(version_text), "Current: v%s", ota_manager_get_version());
    lv_label_set_text(objects.lbl_current_version, version_text);
  }

  // Also set the simple version label if it exists
  if (objects.lbl_version) {
    char version_text[16];
    snprintf(version_text, sizeof(version_text), "v%s", ota_manager_get_version());
    lv_label_set_text(objects.lbl_version, version_text);
  }

  // Initialize OTA UI state - hide elements that should only show after check
  if (objects.lbl_available_version) {
    lv_obj_add_flag(objects.lbl_available_version, LV_OBJ_FLAG_HIDDEN);
  }
  if (objects.btn_install_update) {
    lv_obj_add_flag(objects.btn_install_update, LV_OBJ_FLAG_HIDDEN);
  }
  if (objects.bar_ota_progress) {
    lv_obj_add_flag(objects.bar_ota_progress, LV_OBJ_FLAG_HIDDEN);
  }

  // Set initial status
  if (objects.lbl_ota_status) {
    lv_label_set_text(objects.lbl_ota_status, "Ready to check for updates");
  }

  // Initialize OTA manager (marks current firmware as valid)
  ota_manager_init();
}