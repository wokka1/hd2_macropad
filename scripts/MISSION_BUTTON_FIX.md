# Mission Page Button Injection - Fix Analysis

## Problem
The transform script was injecting BtnCC and BtnCSD but they weren't positioned correctly in EEZ Studio. The issue was that the script was copying upstream containers (480x320 structure) which lacked the proper FLEX layout used in the 800x480 preserved page.

## Root Cause
**Original Approach (Wrong):**
- Extracted container from upstream HD2 Macropad.eez-project
- Tried to modify dimensions and positions
- Missing critical FLEX layout styles
- Upstream structure fundamentally incompatible with preserved page

**Fixed Approach:**
- Create new containers from scratch matching reference pattern
- Only extract event handlers and style properties from upstream
- Build complete container with proper FLEX layout

## Key Changes in Transform Script

### 1. Container Structure
Created containers with proper FLEX layout:
```json
{
  "width": 32,
  "widthUnit": "%",
  "heightUnit": "content",
  "localStyles": {
    "definition": {
      "MAIN": {
        "DEFAULT": {
          "layout": "FLEX",
          "flex_flow": "ROW_WRAP",
          "flex_cross_place": "CENTER",
          "pad_column": 8
        }
      }
    }
  }
}
```

### 2. Button Positions (from manual fixes)
**BtnCC (Cargo Container):**
- Container: left=0
- Button: left=997, top=1140 (column 3, row 4)

**BtnCSD (Super Destroyer):**
- Container: left=9
- Button: left=699, top=1140 (column 2, row 4)
- Note: NOT 110 (directly below BtnOIF) - positioned between columns

### 3. Label Structure
Labels created with:
- Fixed dimensions: width=72, height=32
- Font: MONTSERRAT_14
- Proper widget flags: locked=false, hiddenInEditor=false
- Text with newlines: "Cargo\nContainer", "Super\nDestroyer"

## Configuration Updated

In `MISSION_CUSTOMIZATIONS.injected_buttons`:
```python
"BtnCC": {
    "container_left": 0,
    "button_left": 997, "button_top": 1140,
    "label": "Cargo\nContainer",
    "label_width": 72, "label_height": 32,
},
"BtnCSD": {
    "container_left": 9,
    "button_left": 699, "button_top": 1140,
    "label": "Super\nDestroyer",
    "label_width": 72, "label_height": 32,
},
```

## Function Changes

### Replaced: `scale_and_position_widget()`
### With: `create_mission_button_container()`

The new function:
1. Extracts event handlers and styles (bg_img_src, border_color) from upstream button
2. Creates new container from scratch with FLEX layout
3. Creates new button widget with extracted properties
4. Creates new label widget with configured text and dimensions
5. Returns complete container ready to append to main container

## Verification
Generated containers now match manually fixed version:
- ✓ Container width: 32% (not px)
- ✓ Container FLEX layout present
- ✓ Button positions: BtnCC (997,1140), BtnCSD (699,1140)
- ✓ Label dimensions: 72x32
- ✓ All widget flags and properties match reference pattern

## Result
The transform script now correctly generates Mission page buttons that display properly in EEZ Studio without manual intervention.
