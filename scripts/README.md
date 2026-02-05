# EEZ Transform Script for Elecrow 7" Display

This script transforms the upstream HD2 Macropad EEZ project (480x320) to work with the Elecrow 7" display (800x480).

## Usage

```bash
python3 scripts/transform_eez_elecrow7.py [input.eez-project] [output.eez-project]
```

**Defaults:**
- Input: `eez/HD2 Macropad.eez-project`
- Output: `eez/HD2 Macropad-elecrow7-transformed.eez-project`

## What It Does

The script applies the following transformations:

### Size Scaling
| Element | Original | Elecrow 7" |
|---------|----------|------------|
| Display | 480x320 | 800x480 |
| Stratagem icons | 76x76 | 114x114 |
| Button containers | 116x116 | 174x174 |
| Tab content areas | 480x242 | 800x358 |

### Widget Modifications
- **Removals**: Widgets listed in `REMOVE_IDENTIFIERS` are deleted
- **Relocations**: Widgets can be moved between containers, renamed, and resized
- **Position/Size Overrides**: Specific widgets can have custom positions and sizes

### Bitmap Replacements
Higher resolution bitmap assets are swapped in via `BITMAP_REPLACEMENTS`.

## Configuration

All transformation rules are defined as constants at the top of the script:

| Constant | Purpose |
|----------|---------|
| `SIZE_MAPPINGS` | Exact (width, height) transformations |
| `WIDTH_MAPPINGS` / `HEIGHT_MAPPINGS` | Single dimension transformations |
| `EXCLUDE_IDENTIFIERS` | Widgets to skip size transformation |
| `REMOVE_IDENTIFIERS` | Widgets to delete entirely |
| `WIDGET_RELOCATIONS` | Move widgets between containers |
| `BITMAP_REPLACEMENTS` | Replace bitmap assets |
| `POSITION_OVERRIDES` | Override widget positions by identifier |
| `SIZE_OVERRIDES` | Override widget sizes by identifier |
| `OBJID_OVERRIDES` | Override unnamed widgets by objID |

## Adding New Overrides

### By Identifier (named widgets)
```python
POSITION_OVERRIDES = {
    "MyWidget": {"left": 100, "top": 50},
}

SIZE_OVERRIDES = {
    "MyWidget": {"width": 200, "height": 100},
}
```

### By objID (unnamed widgets)
Find the objID in the EEZ project JSON, then add:
```python
OBJID_OVERRIDES = {
    "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx": {"left": 10, "top": 20, "width": 100, "height": 50},
}
```

### Relocating Widgets
```python
WIDGET_RELOCATIONS = {
    "SourceWidgetName": {
        "new_identifier": "NewName",           # Optional rename
        "target_parent": "container-objid",    # Target container objID
        "insert_after": "SiblingWidget",       # Optional: insert position
        "left": 0, "top": 0,                   # New position
        "width": 100, "height": 100,           # New size
    },
}
```

## Workflow for Upstream Updates

1. Pull latest upstream EEZ project to `eez/HD2 Macropad.eez-project`
2. Run the transform script
3. Open transformed project in EEZ Studio
4. Verify layout, add any new overrides if needed
5. Build in EEZ Studio, then compile with PlatformIO
