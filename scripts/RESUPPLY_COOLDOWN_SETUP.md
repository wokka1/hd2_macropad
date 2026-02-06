# Resupply Cooldown Label Setup

## Overview
This guide explains how to complete the Resupply cooldown timer integration after creating the `LabelSupplies` widget in EEZ Studio.

## Current Status
✅ **Code changes complete:**
- Added `cooldown` field to `stratagemBase` struct
- Resupply cooldown set to 180 seconds (3 minutes)
- Global variable `resupplyCooldownValue` tracking in main.c
- UI update task displaying countdown timer
- Execution code triggers cooldown when Resupply is called

⏳ **EEZ Studio steps needed:**
1. Export your EEZ project with the `LabelSupplies` widget
2. Uncomment one line in `ui_post.c`

## After EEZ Studio Export

### Step 1: Verify Label in screens.h
After exporting from EEZ Studio, check that `src/ui/screens.h` contains:

```c
lv_obj_t *label_supplies;  // or whatever name you used in EEZ
```

### Step 2: Verify Label Creation in screens.c
Check that `src/ui/screens.c` creates the label (should be auto-generated):

```c
{
    // LabelSupplies
    lv_obj_t *obj = lv_label_create(parent_obj);
    objects.label_supplies = obj;
    lv_obj_set_pos(obj, X, Y);  // Position from EEZ Studio
    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_label_set_text(obj, "");
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);  // Initially hidden
}
```

### Step 3: Uncomment Initialization in ui_post.c

In `src/ui/ui_post.c`, find this section (around line 48):

```c
// Initialize Resupply cooldown label (if it exists in EEZ Studio)
// After exporting from EEZ Studio, this should be: labelSupplies = objects.label_supplies;
// For now, it will remain NULL until EEZ export adds the object
// labelSupplies = objects.label_supplies;
```

**Uncomment the last line:**
```c
// Initialize Resupply cooldown label
labelSupplies = objects.label_supplies;
```

### Step 4: Rebuild and Test

```bash
# Run animation patch if needed
python3 scripts/patch_eez_animations.py

# Build and flash
platformio run --target upload
```

## Testing

1. **Deploy the firmware** to the device
2. **Navigate to Game page** (or wherever Resupply button is)
3. **Press Resupply** button (DOWN, DOWN, UP, RIGHT stratagem)
4. **Verify countdown appears** on LabelSupplies
5. **Wait 3 minutes** and confirm label disappears when cooldown expires

## Label Positioning

The label position is controlled by EEZ Studio. Make sure to place it:
- Near the Resupply button on the Game page
- Or wherever makes sense for your UI layout

**Note:** Since we commented out the `lv_obj_align_to()` calls, EEZ Studio positions are used directly.

## Technical Details

### Cooldown Tracking Flow

1. **User presses Resupply button** → `action_trigger_stratagem_base()` called
2. **Execution code checks** if index == 1 (Resupply) and cooldown > 0
3. **Sets global:** `resupplyCooldownValue = getNow() + 180`
4. **UI update task** (runs every 100ms):
   - Calculates remaining time: `diff = resupplyCooldownValue - getNow()`
   - Formats as "M:SS" (e.g., "2:45")
   - Shows/hides label based on remaining time
5. **Label disappears** automatically when countdown reaches 0

### Cooldown Value Source

The 180-second (3-minute) cooldown comes from:
- [Helldivers Wiki - Stratagems](https://helldivers.wiki.gg/wiki/Stratagems)
- Base cooldown without ship module upgrades

### Code Locations

- **Struct definition:** `src/stratagems.h` lines 28-34
- **Resupply entry:** `src/stratagems.h` lines 879-886
- **Global variables:** `src/main.c` lines 64-67
- **UI update logic:** `src/main.c` lines 298-335
- **Execution trigger:** `src/ui/ui_events.c` lines 316-336
- **Label initialization:** `src/ui/ui_post.c` lines 48-52

## Troubleshooting

### Label doesn't appear after pressing Resupply

**Check:**
1. Did you uncomment `labelSupplies = objects.label_supplies;` in ui_post.c?
2. Is the label named exactly `label_supplies` in EEZ Studio?
3. Is the label initially set to HIDDEN in EEZ Studio?
4. Check serial output for "Base stratagem: idx=1" when pressing Resupply

### Label position is wrong

**Solution:**
- Adjust position in EEZ Studio and re-export
- Positions are not overridden by code (we commented out align_to calls)

### Compile error: 'label_supplies' undeclared

**Cause:** EEZ Studio hasn't exported the label yet

**Solution:**
1. Open your EEZ project
2. Verify `LabelSupplies` widget exists
3. Export from EEZ Studio (Build → Export)
4. Check that screens.h now has `lv_obj_t *label_supplies;`

### Label shows wrong time format

**Check:** The format is intentionally "M:SS" (e.g., "3:00", "0:45")
- Minutes: 0-9 (no padding)
- Seconds: 00-59 (padded with leading zero)

This matches the format used for custom stratagem cooldowns.

## Future Enhancements

Potential improvements:
1. **Add cooldowns to other base stratagems** (Reinforce, SOS, etc.)
2. **Visual indicators** when cooldown is almost ready (e.g., change color)
3. **Audio feedback** when cooldown expires
4. **Persistent cooldowns** across page navigation

## Related Documentation

- [COOLDOWN_LABELS_EEZ.md](COOLDOWN_LABELS_EEZ.md) - Guide for custom stratagem cooldown labels
- [EEZ_EXPORT_WORKFLOW.md](EEZ_EXPORT_WORKFLOW.md) - EEZ Studio export workflow
- [README.md](../README.md) - Main project documentation
