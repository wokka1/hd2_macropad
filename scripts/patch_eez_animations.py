#!/usr/bin/env python3
"""
Post-processing script to patch EEZ-generated eez-flow.cpp with animation overrides.

This script should be run after exporting UI from EEZ Studio to restore the
animation fix that eliminates jerky slide/fade animations on the 800x480 RGB display.

Usage:
    python scripts/patch_eez_animations.py

The script:
1. Reads src/ui/eez-flow.cpp
2. Finds the replacePageHook function
3. Patches lv_scr_load_anim call to force instant transitions
4. Writes the patched file back
"""

import os
import re
from pathlib import Path

# Path to the auto-generated file
EEZ_FLOW_FILE = Path(__file__).parent.parent / "src" / "ui" / "eez-flow.cpp"

# Original pattern to find (the unmodified auto-generated code)
ORIGINAL_PATTERN = re.compile(
    r'(void replacePageHook\(int16_t pageId, uint32_t animType, uint32_t speed, uint32_t delay\) \{.*?'
    r'g_currentScreen = screenIndex;)\s*'
    r'lv_scr_load_anim\(screen, \(lv_scr_load_anim_t\)animType, speed, delay, false\);',
    re.DOTALL
)

# Replacement code with animation override
PATCHED_CODE = r'''\1
    // Override all animations - use instant transitions for responsive UI on 800x480 display
    // This patch is applied by scripts/patch_eez_animations.py after EEZ export
    lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);'''


def patch_animations():
    """Patch the eez-flow.cpp file to override animations."""

    if not EEZ_FLOW_FILE.exists():
        print(f"Error: {EEZ_FLOW_FILE} not found!")
        return False

    print(f"Reading {EEZ_FLOW_FILE}...")
    content = EEZ_FLOW_FILE.read_text()

    # Check if already patched
    if "// This patch is applied by scripts/patch_eez_animations.py" in content:
        print("✓ File already patched - animations already overridden")
        return True

    # Apply the patch
    patched_content, num_replacements = ORIGINAL_PATTERN.subn(PATCHED_CODE, content)

    if num_replacements == 0:
        print("Warning: Could not find the expected pattern in eez-flow.cpp")
        print("The file may have already been modified or the EEZ generator output changed.")
        return False

    # Write back the patched file
    print(f"Patching {EEZ_FLOW_FILE}...")
    EEZ_FLOW_FILE.write_text(patched_content)

    print(f"✓ Successfully patched eez-flow.cpp ({num_replacements} replacement(s))")
    print("  All screen transitions will now use instant animations for responsive UI")
    return True


if __name__ == "__main__":
    import sys
    success = patch_animations()
    sys.exit(0 if success else 1)
