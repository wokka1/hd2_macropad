# Adding Cooldown Labels in EEZ Studio

## Overview
The cooldown timer feature displays countdown timers on custom stratagem buttons. Currently, stub labels exist in the code but need proper positioning in EEZ Studio.

## Current Status
✅ **Code-level**: Cooldown tracking logic fully implemented
✅ **Build**: Compiles successfully with stub labels (hidden by default)
⚠️ **UI**: Labels exist but positioned at (0,0) - need EEZ Studio positioning

## Steps to Add in EEZ Studio

### 1. Open Project
Open `eez/HD2 Macropad-elecrow7-transformed.eez-project` in EEZ Studio

### 2. Navigate to Game Page
- Find the Game page in the pages list
- Locate the 6 custom stratagem buttons (CustomStratagem1-6)

### 3. Add Cooldown Labels
For each custom stratagem button (1-6), add a label widget:

**Properties:**
- **Object name**: `LabelCooldown1` through `LabelCooldown6`
- **Position**: Overlay on corresponding stratagem button (e.g., bottom-center or top-right)
- **Size**: Content-based (will show "M:SS" format like "2:30")
- **Style**: Use LabelCooldown style (or create with white text, small font)
- **Initial state**: Hidden (flag: HIDDEN)
- **Text**: Empty string initially
- **Font**: MONTSERRAT_12 or similar (small enough to fit on button)
- **Alignment**: CENTER or BOTTOM_MID

### 4. Suggested Positioning
Position labels over the corresponding buttons:
```
label_cooldown1 → over custom_stratagem1 (e.g., bottom-center, y+80)
label_cooldown2 → over custom_stratagem2 (e.g., bottom-center, y+80)
...and so on
```

### 5. Export from EEZ Studio
After adding labels:
```bash
# Export from EEZ Studio to generate UI code
# Then run animation patch
python3 scripts/patch_eez_animations.py
```

### 6. Rebuild
```bash
platformio run
```

## Technical Details

### Code Integration
The cooldown logic in `src/main.c` (ui_update_task) will:
1. Calculate remaining time for each stratagem
2. Format as "M:SS" (e.g., "3:45")
3. Update label text and show/hide automatically

### Object References
```c
// In src/ui/ui_post.c
cooldownLabels[0] = objects.label_cooldown1;
cooldownLabels[1] = objects.label_cooldown2;
// ...etc
```

### Current Stub Implementation
```c
// In src/ui/screens.c (auto-generated)
lv_obj_t *obj = lv_label_create(parent_obj);
objects.label_cooldown1 = obj;
lv_obj_set_pos(obj, 0, 0);  // ← Needs proper positioning in EEZ
lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
lv_label_set_text(obj, "");
lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);  // Initially hidden
```

## Alternative: Script-based Positioning
If you don't want to use EEZ Studio, labels can be repositioned in `ui_post.c`:

```c
// Example: Position cooldown labels at bottom of stratagem buttons
lv_obj_align_to(objects.label_cooldown1, objects.custom_stratagem1, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
lv_obj_align_to(objects.label_cooldown2, objects.custom_stratagem2, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
// etc...
```

## Testing
1. Build and flash firmware
2. Assign stratagems to custom slots
3. Activate a stratagem
4. Cooldown timer should appear and count down
5. Timer should hide when reaching zero

## Notes
- Cooldown values from: https://helldivers.wiki.gg/wiki/Stratagems
- Base cooldown times (no ship module upgrades)
- Format: seconds (e.g., 480 = 8:00 minutes)
