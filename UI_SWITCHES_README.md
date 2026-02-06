# UI Configuration Switches

This document describes the three configuration switches added to the Config page for controlling display and debugging features.

## Overview

Three new switches have been added to the Configuration page to give users control over:
1. **Cooldown Timer Display** - Show/hide cooldown countdown timers
2. **Debug Logging** - Enable/disable verbose debug output to serial monitor
3. **Buzzer Mute** - Mute/unmute buzzer beeps when timers expire

All settings are persisted to NVS (Non-Volatile Storage) and restored on device startup.

## Configuration Switches

### 1. Cooldown Display Toggle (ChbCooldowns_1)

**Purpose**: Controls visibility of cooldown countdown timers on the Game screen.

**Behavior**:
- When **enabled** (default): Cooldown timers appear below stratagem buttons and Resupply button, showing remaining time in `M:SS` format
- When **disabled**: All cooldown labels are hidden from the display

**Implementation**:
- Global flag: `bool showCooldowns` in [main.c](src/main.c)
- Config key: `"cooldowns"` (stored as 0=off, 1=on)
- Default value: **ON** (enabled)
- Action handler: `action_action_toggle_cooldowns()` in [actions.c](src/ui/actions.c)
- Setter function: `setShowCooldowns()` in [configuation.c](src/configuation.c)

**Affected UI Elements**:
- Custom stratagem cooldown labels (6 total: `label_cooldown1` through `label_cooldown6`)
- Resupply cooldown label (`label_supplies`)

### 2. Debug Logging Toggle (ChbDebug_1)

**Purpose**: Controls verbose debug output to the serial monitor.

**Behavior**:
- When **enabled**: Outputs detailed logging including:
  - HID command execution details
  - Buzzer beep events with timing and results
  - Cooldown expiration notifications
  - Connection state changes
- When **disabled** (default): Only essential logging (errors and warnings) is shown

**Implementation**:
- Global flag: `bool debugLogging` in [main.c](src/main.c)
- Config key: `"debug"` (stored as 0=off, 1=on)
- Default value: **OFF** (disabled)
- Action handler: `action_action_toggle_debug()` in [actions.c](src/ui/actions.c)
- Setter function: `setDebugLogging()` in [configuation.c](src/configuation.c)

**Debug Output Examples**:
```c
// HID input execution
ESP_LOGI(TAG, "Send command");
ESP_LOGI(TAG, "CMD Index: %c", (char)(cmdIndex + '0'));
ESP_LOGI(TAG, "CMD Value: %d", stratagemCode[cmdIndex]);

// Buzzer events
ESP_LOGI(TAG, "=== BUZZER BEEP START (duration: %d ms) ===", duration_ms);
ESP_LOGI(TAG, ">>> Buzzer beep result: %s (0x%04X)", esp_err_to_name(result), result);

// Cooldown events
ESP_LOGI(TAG, "*** RESUPPLY COOLDOWN EXPIRED ***");
ESP_LOGI(TAG, "*** STRATAGEM %d COOLDOWN EXPIRED ***", c + 1);
```

### 3. Buzzer Mute (ChbMute)

**Purpose**: Mutes the buzzer that sounds when cooldown timers expire.

**Behavior**:
- When **enabled** (muted): Buzzer is silent - no beeps when timers expire
- When **disabled** (default): Buzzer emits 400ms beep when any cooldown timer reaches zero

**Implementation**:
- Global flag: `bool playerMuted` in [main.c](src/main.c)
- Config key: `"muted"` (stored as 0=unmuted, 1=muted)
- Default value: **OFF** (unmuted - buzzer enabled)
- Action handler: `action_mute_sound()` in [actions.c](src/ui/actions.c)
- Setter function: `setMuted()` in [configuation.c](src/configuation.c)

**Buzzer Behavior**:
- **Trigger**: When any cooldown timer (custom stratagem or Resupply) reaches zero
- **Duration**: 400ms beep
- **Hardware**: CrowPanel Advance buzzer via STC8H1K28 I2C chip (address 0x30)
- **Beep prevention**: Each timer tracks if it has already beeped to prevent duplicate beeps

**Code Implementation**:
```c
// Buzzer only sounds if not muted
if (!playerMuted)
{
    bsp_buzzer_beep(400);  // 400ms beep
}
```

## Configuration Storage (NVS)

All three settings are stored in Non-Volatile Storage under the namespace `"config"`:

| Setting | NVS Key | Type | Default | Values |
|---------|---------|------|---------|--------|
| Cooldown Display | `"cooldowns"` | uint8_t | 1 (ON) | 0=hidden, 1=visible |
| Debug Logging | `"debug"` | uint8_t | 0 (OFF) | 0=disabled, 1=enabled |
| Buzzer Mute | `"muted"` | uint8_t | 0 (OFF) | 0=unmuted, 1=muted |

### NVS Operations

**Initialization**: `initConfig()` in [configuation.c](src/configuation.c)
- Creates async NVS write queue (10 entries)
- Spawns low-priority task (priority 1) to handle NVS writes without blocking BLE

**Loading**: `loadConfig()` called during `app_main()`
- Reads all settings from NVS
- Applies defaults if keys don't exist
- Restores UI checkbox states

**Saving**: Async writes via `setConfig()` function
- Non-blocking - posts to queue instead of direct NVS write
- Prevents BLE disruption during flash writes
- 50ms delay between consecutive writes

## EEZ Studio Integration

### Action Naming Convention

**Important**: EEZ Studio automatically prepends `action_` to action names defined in the project.

**Correct naming**:
- EEZ Studio action name: `action_toggle_debug`
- Generated C function: `action_action_toggle_debug()`

**Configuration in EEZ Studio**:
1. Define native actions in the Actions section:
   - Name: `action_toggle_debug` → Type: Native
   - Name: `action_toggle_cooldowns` → Type: Native
   - Name: `MuteSound` → Type: Native (already exists)

2. Assign actions to checkboxes:
   - `ChbDebug_1` → Event: CHANGED → Action: `action_toggle_debug`
   - `ChbCooldowns_1` → Event: CHANGED → Action: `action_toggle_cooldowns`
   - `ChbMute` → Event: CHANGED → Action: `MuteSound`

### Auto-Generated Files (Do Not Edit)
- `src/ui/screens.c` - Event handler callbacks
- `src/ui/ui.c` - Action table and UI initialization

### User-Editable Files
- `src/ui/actions.c` - Action implementations
- `src/ui/actions.h` - Action declarations
- `src/configuation.c` - Configuration setters/getters
- `src/main.c` - Global flags and UI update logic

## Files Modified

### Core Logic
- [src/main.c](src/main.c)
  - Added `showCooldowns` flag
  - Modified `ui_update_task()` to respect showCooldowns flag
  - Wrapped buzzer beeps with `!playerMuted` checks

- [src/configuation.c](src/configuation.c)
  - Added `setDebugLogging()` function
  - Added `setShowCooldowns()` function
  - Updated `loadConfig()` to load new settings from NVS

- [src/configration.h](src/configration.h)
  - Added function declarations for new setters

### UI Integration
- [src/ui/actions.c](src/ui/actions.c)
  - Added `action_action_toggle_debug()` handler
  - Added `action_action_toggle_cooldowns()` handler

- [src/ui/actions.h](src/ui/actions.h)
  - Added function declarations for new actions

### EEZ Studio Project
- `eez/HD2 Macropad-elecrow7-transformed.eez-project`
  - Added three checkboxes to Config page
  - Defined native actions
  - Wired event handlers

## Usage Instructions

### For Users
1. Navigate to the **Config** page on the device
2. Toggle switches as desired:
   - **Cooldowns**: Show/hide timer displays during gameplay
   - **Debug**: Enable for troubleshooting (increases serial output)
   - **Mute**: Silence buzzer beeps when timers expire
3. Settings are saved automatically and persist across reboots

### For Developers
1. Modify UI in EEZ Studio (`eez/HD2 Macropad-elecrow7-transformed.eez-project`)
2. Export UI from EEZ Studio
3. Run animation patch script: `python3 scripts/patch_eez_animations.py`
4. Build and flash: `pio run -t upload`

## Testing Checklist

- [ ] **Cooldown Display Toggle**
  - [ ] Enable switch → cooldown timers visible during gameplay
  - [ ] Disable switch → cooldown timers hidden immediately
  - [ ] Setting persists after reboot

- [ ] **Debug Logging Toggle**
  - [ ] Enable switch → verbose output appears in serial monitor
  - [ ] Disable switch → only essential logging shown
  - [ ] Setting persists after reboot

- [ ] **Buzzer Mute Toggle**
  - [ ] Unmuted → buzzer beeps when timer expires
  - [ ] Muted → no buzzer sound when timer expires
  - [ ] Cooldown beep prevention works (no duplicate beeps)
  - [ ] Setting persists after reboot

- [ ] **General**
  - [ ] All switches work independently
  - [ ] UI checkboxes reflect current state on startup
  - [ ] NVS writes don't disrupt BLE connection

## Technical Notes

### Buzzer Hardware
- **Board**: CrowPanel Advance 7" (ESP32-S3)
- **Controller**: STC8H1K28 I2C chip at address 0x30
- **Commands**:
  - `246` = Buzzer ON
  - `247` = Buzzer OFF
  - `0-244` = Backlight brightness (245=off)

### Cooldown Beep Logic
Each timer has a beep flag to prevent duplicate beeps:
```c
// Custom stratagems (6 total)
bool cooldownBeepTriggered[MAX_USER_STRATAGEMS] = {false};

// Resupply timer
bool resupplyBeepTriggered = false;
```

**Beep triggering**:
1. Timer expires (diff <= 0)
2. Check if beep hasn't been triggered yet
3. If `!playerMuted`, call `bsp_buzzer_beep(400)`
4. Set beep flag to prevent re-triggering
5. Reset beep flag when new cooldown starts

### Memory Usage
- **RAM**: 15.7% (51,520 / 327,680 bytes)
- **Flash**: 53.5% (4,208,088 / 7,864,320 bytes)
- Impact: Minimal (~100 bytes for flags and NVS keys)

## Future Enhancements

Potential improvements for consideration:
1. **Volume Control**: Add slider for buzzer volume (would require PWM control)
2. **Cooldown Color Coding**: Change timer color as expiration approaches
3. **Custom Beep Patterns**: Different sounds for different timer types
4. **Log Level Selection**: Fine-grained debug levels (ERROR, WARN, INFO, DEBUG)
5. **Export/Import Settings**: Backup configuration to SD card

## Related Documentation
- [EEZ Export Workflow](scripts/EEZ_EXPORT_WORKFLOW.md)
- [Transform Script Changes](scripts/TRANSFORM_CHANGES_SUMMARY.md)
- [Project Memory](~/.claude/projects/-Users-wokka-hd2-macropad-hd2-macropad/memory/MEMORY.md)
- [Sound Research](SOUND_RESEARCH.md)

## Bug Fixes and Improvements

### Fix: Inverted Cooldown Toggle Logic (2024-02-06)
**Issue**: The "Disable Cooldowns" switch behavior was inverted - checking it would show cooldowns instead of hiding them.

**Solution**: Inverted the boolean logic in both the action handler and restore function:
- [actions.c](src/ui/actions.c): `setShowCooldowns(!disable, false)` - inverts checkbox state
- [configuation.c](src/configuation.c): Inverted checkbox restore logic - show=true means unchecked
- Added immediate feedback log message

### Improvement: Debug Logging Immediate Feedback (2024-02-06)
**Issue**: Debug toggle appeared to do nothing because debug messages only appear during specific events.

**Solution**: Added immediate feedback messages when toggling:
- [actions.c](src/ui/actions.c): Added ESP_LOGI messages on toggle
- Added `#include "esp_log.h"` for logging support
- User now sees "Debug logging ENABLED/DISABLED" immediately in serial monitor

## Commit History
- Initial implementation: 27fd46b
- Buzzer beep for all timers (400ms): cece586
- Debug logging and UI switches: 27fd46b
- Bug fixes: inverted cooldown logic and debug feedback: [current commit]
