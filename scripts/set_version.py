#!/usr/bin/env python3
"""
PlatformIO pre-build script to set PROJECT_VER from version.h
This ensures the ESP-IDF app version matches version.h
"""

import re
from SCons.Script import Import

Import("env")

def get_version_from_header():
    """Extract SW_VER from version.h"""
    version_file = "src/version.h"
    try:
        with open(version_file, "r") as f:
            content = f.read()
            match = re.search(r'#define\s+SW_VER\s+"([^"]+)"', content)
            if match:
                return match.group(1)
    except FileNotFoundError:
        print(f"Warning: {version_file} not found")
    return None

version = get_version_from_header()
if version:
    print(f"Setting PROJECT_VER to {version} (from version.h)")
    env.Append(CPPDEFINES=[
        ("PROJECT_VER", f'\\"{version}\\"')
    ])
else:
    print("Warning: Could not extract version from version.h")
