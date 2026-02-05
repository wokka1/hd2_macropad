# Transform Script Changes Summary

## Changes Made

### 1. Mission Page Button Injection
**Issue:** Script was copying upstream 480x320 containers which lacked proper FLEX layout.

**Solution:**
- Created `create_mission_button_container()` function to build containers from scratch
- Extracts only event handlers and styles from upstream
- Creates proper FLEX layout with ROW_WRAP, CENTER alignment

**Configuration:**
```python
MISSION_CUSTOMIZATIONS = {
    "font_replacement": ("MONTSERRAT_20", "MONTSERRAT_14"),
    "icon_size": 100,  # 114→100 for all Mission icons
    "injected_buttons": {
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
    },
}
```

### 2. Setup Page Container Removals
**Issue:** Upstream has extra empty spacer containers that should be removed for 800x480 layout.

**Solution:** Added `REMOVE_CONTAINERS` set with objIDs to remove.

**Removed Containers:**
- **TabSpecial**: 4th container (was 4, now 3)
- **TabBackpack**: 4th container (was 4, now 3)
- **TabSentry**: 2nd container (was 2, now 1)

**Configuration:**
```python
REMOVE_CONTAINERS = {
    "b4fd079e-8f4d-43e2-9651-fdb842521d6a",  # TabSpecial container 4
    "99603c57-9006-475b-ca56-ef9f70943cf9",  # TabBackpack container 4
    "dc11de2d-804a-46e5-bdef-da02158d5fe9",  # TabSentry container 2
}
```

## Transform Process

1. **Load upstream project** (480x320)
2. **Transform sizes** (480x320 → 800x480)
3. **Remove containers** by objID
4. **Relocate widgets** (e.g., BtnHellbomb)
5. **Replace bitmaps** (tab icons)
6. **Preserve pages** (Mission from hd2-wokka.eez-project)
7. **Customize Mission page**:
   - Replace font (MONTSERRAT_20 → MONTSERRAT_14)
   - Resize icons (114x114 → 100x100)
   - Inject missing buttons (BtnCC, BtnCSD)

## Verification

All changes verified:
- ✓ Mission font: MONTSERRAT_14
- ✓ Mission icons: 100x100 (11 buttons)
- ✓ BtnCC positioned: (997, 1140) with label
- ✓ BtnCSD positioned: (699, 1140) with label
- ✓ TabSpecial: 3 containers
- ✓ TabBackpack: 3 containers
- ✓ TabSentry: 1 container

## Important Notes

**Don't manually edit the transformed output file!**
- The script overwrites `HD2 Macropad-elecrow7-transformed.eez-project` each run
- Instead, edit the transform script configuration
- For Mission page layout changes, edit `eez/hd2-wokka.eez-project` (it gets preserved)

**To add more changes:**
1. Identify what needs to change
2. Add configuration to transform script
3. Run script to verify
4. Commit script changes
