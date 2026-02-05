#!/usr/bin/env python3
"""
Transform upstream EEZ project for Elecrow 7" display (800x480).

This script takes the upstream HD2 Macropad EEZ project (480x320) and
transforms it for the higher resolution Elecrow 7" display (800x480).

Transformations applied:
- Display resolution: 480x320 → 800x480
- Widget size scaling via SIZE_MAPPINGS
- Widget removals via REMOVE_IDENTIFIERS
- Widget relocations via WIDGET_RELOCATIONS
- Position/size overrides via POSITION_OVERRIDES, SIZE_OVERRIDES, OBJID_OVERRIDES
- Bitmap replacements via BITMAP_REPLACEMENTS

Usage:
    python3 transform_eez_elecrow7.py [input.eez-project] [output.eez-project]

    Defaults:
        input:  eez/HD2 Macropad.eez-project
        output: eez/HD2 Macropad-elecrow7-transformed.eez-project
"""

import json
import sys
import os
import base64

# =============================================================================
# DISPLAY SETTINGS
# =============================================================================

UPSTREAM_DISPLAY = (480, 320)
ELECROW7_DISPLAY = (800, 480)

# =============================================================================
# SIZE TRANSFORMATION MAPPINGS
# =============================================================================

# Exact size mappings (old_w, old_h) -> (new_w, new_h)
SIZE_MAPPINGS = {
    (480, 320): (800, 480),    # Page/display size
    (76, 76): (114, 114),      # Stratagem icons
    (116, 116): (174, 174),    # Button containers
    (480, 242): (800, 358),    # Tab content areas
    (19, 76): (32, 114),       # Tab bar indicator containers
    (76, 100): (114, 150),     # Vertical icon containers
}

# Single dimension mappings for partial matches
WIDTH_MAPPINGS = {480: 800}
HEIGHT_MAPPINGS = {320: 480, 242: 358, 280: 396}

# =============================================================================
# EXCLUSIONS
# =============================================================================

# Widget identifiers excluded from size transformation
EXCLUDE_IDENTIFIERS = {
    "ImgSplash",
    "ManualArrowLeft", "ManualArrowRight", "ManualArrowUp", "ManualArrowDown",
    "manualPreviewItem",
}

# Widget types excluded from page-size (480x320) transformation
EXCLUDE_TYPES_FROM_PAGE_SIZE = {"LVGLImageWidget"}

# Pages to preserve from reference file instead of transforming
# These pages have custom layouts that shouldn't be overwritten
PRESERVE_PAGES = {
    "Mission",  # Keep fuller descriptions that fit on 800x480
}
PRESERVE_REFERENCE = "eez/hd2-wokka.eez-project"

# Mission buttons to inject from upstream if missing in preserved page
# These are new buttons added upstream that need to be merged in
INJECT_MISSION_BUTTONS = {
    "BtnCC",   # Cargo Container
    "BtnCSD",  # Civilian Safe District
}

# =============================================================================
# WIDGET REMOVALS
# =============================================================================

REMOVE_IDENTIFIERS = {
    "BtnSOS",  # Game page - removed
}

# =============================================================================
# WIDGET RELOCATIONS
# Move widgets between containers with optional rename/resize
# =============================================================================

WIDGET_RELOCATIONS = {
    "BtnHellbomb": {
        "new_identifier": "BtnHellbomb_1",
        "target_parent": "4ed7b682-7506-45ec-bdac-e6d860263d3a",
        "insert_after": "BtnRearm",
        "left": -848, "top": -1909, "width": 174, "height": 174,
    },
}

# =============================================================================
# BITMAP REPLACEMENTS
# =============================================================================

BITMAP_REPLACEMENTS = {
    "tabIcons": "eez/assets/tabs_icons_800x38.png",
}

# =============================================================================
# WIDGET OVERRIDES (by identifier)
# =============================================================================

POSITION_OVERRIDES = {
    # Intro page
    "ImgSplash": {"left": 160, "top": 80},
    # Setup page
    "MsgBox": {"left": -3, "top": 139},
    "TabViewSetup": {"top": 10},
    "LabelAmount": {"left": 12, "top": -177},
    # Game page
    "BtnRearm": {"left": 387},
    "CustomStratagem1": {"left": -658, "top": 621},
    "ImgConnection2": {"left": -741, "top": -70},
}

SIZE_OVERRIDES = {
    "MsgBox": {"width": 85, "height": 64},
    "MsgLabel": {"width": 65, "height": 44},
    "TabViewConfig": {"width": 100, "height": 280},
}

IMAGE_SIZE_OVERRIDES = {
    "tabIcons": {"width": 800, "height": 38, "left": 0, "top": 8},
}

# =============================================================================
# WIDGET OVERRIDES (by objID - for unnamed widgets)
# =============================================================================

OBJID_OVERRIDES = {
    # --- Setup page ---
    "252c8bf6-4f4f-4528-b082-0ae46e1e9354": {"left": 68, "top": 0, "width": 100, "height": 41},   # button bar container
    "03739a41-3f8d-4ea3-faab-667e98cac915": {"left": 643, "top": -41},                            # button [5]
    "360a6033-6a09-4c23-dd52-3184c401728a": {"width": 76, "height": 100, "left": 336, "top": -248},  # status bar
    "c12fc17a-c1d4-48a0-c486-26f5f04e1a86": {"width": 76, "height": 76},  # container
    "2597bc80-aa79-4b2e-958b-c70f9b847d4e": {"width": 76, "height": 76},  # container
    "99603c57-9006-475b-ca56-ef9f70943cf9": {"width": 76, "height": 76},  # container
    "ab451dbf-4609-42a3-f7f3-1a2b1238c19a": {"width": 76, "height": 76},  # container
    "13a50eac-2594-47ff-c923-a35229839d29": {"width": 19, "height": 76},  # tab indicator
    "6615d191-ca0d-4020-8b2b-fbb9d0df288b": {"width": 76, "height": 76},  # container
    "d279652b-f3d0-4c83-f814-6af54c156e25": {"width": 76, "height": 76},  # container

    # --- Preset page ---
    "581e561e-3eea-47c9-f87a-557665f4f10e": {"width": 76, "height": 100},  # button

    # --- Game page ---
    "54a11e59-23cc-45a4-e624-3925e605d7d3": {"left": -3, "top": -2, "width": 151, "height": 114},  # button

    # --- Config page ---
    "4a783d06-3a89-4aa3-b4dc-58cb60d4c730": {"width": 800, "height": 242},  # tab content
    "69259fd5-546e-4f77-e2d7-a14930deb60f": {"width": 800, "height": 242},  # tab content
    "0df4e875-231b-45a0-9627-3fc55820a123": {"width": 76, "height": 100},   # button
    "71fb1be1-e076-45e9-d8fd-34d7fb411371": {"width": 76, "height": 100},   # button
    "247cb222-9fb2-4e82-e6b0-9e6c71d38870": {"width": 76, "height": 100},   # button
    "99eee6d6-4260-4497-ec39-a10d84b95c70": {"width": 76, "height": 100},   # button
}


# =============================================================================
# TRANSFORMATION FUNCTIONS
# =============================================================================

def apply_override(obj, override):
    """Apply an override dict to a widget object."""
    for key in ("left", "top", "width", "height"):
        if key in override:
            obj[key] = override[key]


def transform_size(width, height):
    """Transform a width/height pair based on mappings."""
    if (width, height) in SIZE_MAPPINGS:
        return SIZE_MAPPINGS[(width, height)]
    return (WIDTH_MAPPINGS.get(width, width), HEIGHT_MAPPINGS.get(height, height))


def should_transform_widget(obj):
    """Check if a widget should be transformed based on exclusion rules."""
    identifier = obj.get("identifier", "")
    widget_type = obj.get("type", "")
    size = (obj.get("width"), obj.get("height"))

    if identifier in EXCLUDE_IDENTIFIERS:
        return False
    if widget_type in EXCLUDE_TYPES_FROM_PAGE_SIZE and size == UPSTREAM_DISPLAY:
        return False
    return True


def transform_widget(obj):
    """Recursively transform widget sizes in the EEZ project."""
    if not isinstance(obj, dict):
        return obj

    identifier = obj.get("identifier", "")
    objID = obj.get("objID", "")
    size_overridden = False

    # Apply objID overrides (unnamed widgets)
    if objID in OBJID_OVERRIDES:
        override = OBJID_OVERRIDES[objID]
        apply_override(obj, override)
        size_overridden = "width" in override or "height" in override

    # Apply identifier-based position overrides
    if identifier in POSITION_OVERRIDES:
        apply_override(obj, POSITION_OVERRIDES[identifier])

    # Apply identifier-based size overrides
    if identifier in SIZE_OVERRIDES:
        apply_override(obj, SIZE_OVERRIDES[identifier])
        size_overridden = True
    # Apply image-based overrides
    elif obj.get("image") in IMAGE_SIZE_OVERRIDES:
        apply_override(obj, IMAGE_SIZE_OVERRIDES[obj["image"]])
        size_overridden = True
    # Apply size transformation if no override and widget qualifies
    elif not size_overridden and "width" in obj and "height" in obj:
        if should_transform_widget(obj):
            old_size = (obj["width"], obj["height"])
            new_size = transform_size(*old_size)
            if old_size != new_size:
                obj["width"], obj["height"] = new_size

    # Recurse into children, filtering out removed widgets
    for key, value in obj.items():
        if isinstance(value, dict):
            transform_widget(value)
        elif isinstance(value, list):
            if key in ("children", "widgets", "components"):
                obj[key] = [
                    item for item in value
                    if not (isinstance(item, dict) and item.get("identifier") in REMOVE_IDENTIFIERS)
                ]
                value = obj[key]
            for item in value:
                if isinstance(item, dict):
                    transform_widget(item)

    return obj


def relocate_widgets(project):
    """Relocate widgets to different containers with optional rename/resize."""
    if not WIDGET_RELOCATIONS:
        return 0

    def find_and_extract(obj, identifier):
        for key in ("children", "widgets", "components"):
            if key in obj and isinstance(obj[key], list):
                for i, item in enumerate(obj[key]):
                    if isinstance(item, dict):
                        if item.get("identifier") == identifier:
                            return obj[key].pop(i)
                        result = find_and_extract(item, identifier)
                        if result:
                            return result
        return None

    def find_by_objid(obj, target_objid):
        if obj.get("objID") == target_objid:
            return obj
        for key in ("children", "widgets", "components"):
            if key in obj and isinstance(obj[key], list):
                for item in obj[key]:
                    if isinstance(item, dict):
                        result = find_by_objid(item, target_objid)
                        if result:
                            return result
        return None

    relocated = 0
    for page in project.get("userPages", []):
        for source_id, config in WIDGET_RELOCATIONS.items():
            widget = find_and_extract(page, source_id)
            if widget:
                if "new_identifier" in config:
                    widget["identifier"] = config["new_identifier"]
                apply_override(widget, config)

                target = find_by_objid(page, config["target_parent"])
                if target:
                    for key in ("children", "widgets"):
                        if key in target:
                            idx = len(target[key])
                            if "insert_after" in config:
                                for i, item in enumerate(target[key]):
                                    if item.get("identifier") == config["insert_after"]:
                                        idx = i + 1
                                        break
                            target[key].insert(idx, widget)
                            relocated += 1
                            print(f"  Relocated: {source_id} -> {config.get('new_identifier', source_id)}")
                            break
    return relocated


def transform_display_settings(settings):
    """Transform display settings to Elecrow 7" resolution."""
    if "general" in settings:
        general = settings["general"]
        if general.get("displayWidth") == UPSTREAM_DISPLAY[0]:
            general["displayWidth"] = ELECROW7_DISPLAY[0]
        if general.get("displayHeight") == UPSTREAM_DISPLAY[1]:
            general["displayHeight"] = ELECROW7_DISPLAY[1]
    return settings


def replace_bitmaps(project, project_dir):
    """Replace bitmap assets with higher resolution versions."""
    if "bitmaps" not in project:
        return 0

    replaced = 0
    for bitmap in project["bitmaps"]:
        name = bitmap.get("name", "")
        if name in BITMAP_REPLACEMENTS:
            path = os.path.join(project_dir, BITMAP_REPLACEMENTS[name])
            if os.path.exists(path):
                with open(path, "rb") as f:
                    data = base64.b64encode(f.read()).decode("utf-8")
                bitmap["image"] = f"data:image/png;base64,{data}"
                replaced += 1
                print(f"  Replaced bitmap: {name}")
            else:
                print(f"  Warning: Bitmap not found: {path}")
    return replaced


# =============================================================================
# MISSION PAGE BUTTON INJECTION
# =============================================================================

def inject_missing_mission_buttons(preserved_page, upstream_page):
    """Inject missing buttons from upstream into preserved Mission page."""
    if not INJECT_MISSION_BUTTONS:
        return 0

    def find_identifiers(obj, found=None):
        """Recursively find all widget identifiers."""
        if found is None:
            found = set()
        if isinstance(obj, dict):
            if "identifier" in obj:
                found.add(obj["identifier"])
            for key in ("children", "widgets", "components"):
                if key in obj:
                    for item in obj[key]:
                        find_identifiers(item, found)
        return found

    def find_main_container(page):
        """Find the main button container on Mission page."""
        for comp in page.get("components", []):
            for child in comp.get("children", []):
                if child.get("type") == "LVGLContainerWidget":
                    # Check if it contains mission buttons
                    idents = find_identifiers(child)
                    if any(i.startswith("Btn") for i in idents):
                        return child
        return None

    def extract_button_container(page, btn_ident):
        """Extract the container for a button from the page."""
        for comp in page.get("components", []):
            for child in comp.get("children", []):
                if child.get("type") == "LVGLContainerWidget":
                    for container in child.get("children", []):
                        for btn_child in container.get("children", []):
                            if btn_child.get("identifier") == btn_ident:
                                return container
        return None

    def scale_widget_for_elecrow7(widget):
        """Scale a widget and its children from 480x320 to 800x480 sizes."""
        import copy
        widget = copy.deepcopy(widget)

        def scale(obj):
            if isinstance(obj, dict):
                # Scale container/button sizes (76x76 -> 114x114)
                if obj.get("width") == 76 and obj.get("height") == 76:
                    obj["width"] = 114
                    obj["height"] = 114
                for key in ("children", "widgets"):
                    if key in obj:
                        for item in obj[key]:
                            scale(item)
        scale(widget)
        return widget

    # Find what buttons exist in preserved page
    preserved_buttons = find_identifiers(preserved_page)

    # Find main container in preserved page
    main_container = find_main_container(preserved_page)
    if not main_container:
        print("  Warning: Could not find main container in preserved Mission page")
        return 0

    injected = 0
    for btn_ident in INJECT_MISSION_BUTTONS:
        if btn_ident not in preserved_buttons:
            # Extract from upstream
            container = extract_button_container(upstream_page, btn_ident)
            if container:
                # Scale for 800x480
                scaled_container = scale_widget_for_elecrow7(container)
                # Add to preserved page
                if "children" in main_container:
                    main_container["children"].append(scaled_container)
                    injected += 1
                    print(f"  Injected missing button: {btn_ident}")
            else:
                print(f"  Warning: Could not find {btn_ident} in upstream")

    return injected


# =============================================================================
# MAIN
# =============================================================================

def transform_eez_project(input_path, output_path, project_dir=None):
    """Main transformation function."""
    print(f"Loading: {input_path}")

    if project_dir is None:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        project_dir = os.path.dirname(script_dir)

    with open(input_path, "r", encoding="utf-8") as f:
        project = json.load(f)

    # Transform display settings
    if "settings" in project:
        transform_display_settings(project["settings"])
        print(f"  Display: {UPSTREAM_DISPLAY[0]}x{UPSTREAM_DISPLAY[1]} -> {ELECROW7_DISPLAY[0]}x{ELECROW7_DISPLAY[1]}")

    # Transform pages
    if "userPages" in project:
        print(f"  Pages: {len(project['userPages'])}")
        for page in project["userPages"]:
            transform_widget(page)

    # Transform user widgets
    if "userWidgets" in project:
        print(f"  User widgets: {len(project['userWidgets'])}")
        for widget in project["userWidgets"]:
            transform_widget(widget)

    # Relocate widgets
    relocated = relocate_widgets(project)
    if relocated:
        print(f"  Widgets relocated: {relocated}")

    # Replace bitmaps
    replaced = replace_bitmaps(project, project_dir)
    if replaced:
        print(f"  Bitmaps replaced: {replaced}")

    # Preserve pages from reference file and inject missing buttons
    if PRESERVE_PAGES:
        ref_path = os.path.join(project_dir, PRESERVE_REFERENCE)
        if os.path.exists(ref_path):
            with open(ref_path, "r", encoding="utf-8") as f:
                ref_project = json.load(f)
            ref_pages = {p.get("name"): p for p in ref_project.get("userPages", [])}

            # Keep original upstream pages for injection
            upstream_pages = {p.get("name"): p for p in project.get("userPages", [])}

            for i, page in enumerate(project.get("userPages", [])):
                page_name = page.get("name")
                if page_name in PRESERVE_PAGES and page_name in ref_pages:
                    # Replace with preserved page
                    project["userPages"][i] = ref_pages[page_name]
                    print(f"  Preserved page: {page_name}")

                    # Inject missing buttons from upstream
                    if page_name == "Mission" and page_name in upstream_pages:
                        injected = inject_missing_mission_buttons(
                            project["userPages"][i],
                            upstream_pages[page_name]
                        )
                        if injected:
                            print(f"  Injected {injected} mission button(s)")
        else:
            print(f"  Warning: Reference file not found: {ref_path}")

    # Save
    print(f"Saving: {output_path}")
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(project, f, indent=2)

    print("Done!")
    return project


def compare_projects(original_path, transformed_path):
    """Compare size distributions between original and transformed."""
    from collections import Counter

    def collect_sizes(obj, sizes=None):
        if sizes is None:
            sizes = []
        if isinstance(obj, dict):
            w, h = obj.get("width"), obj.get("height")
            if w is not None and h is not None:
                sizes.append((w, h))
            for v in obj.values():
                collect_sizes(v, sizes)
        elif isinstance(obj, list):
            for item in obj:
                collect_sizes(item, sizes)
        return sizes

    with open(original_path, "r") as f:
        original = json.load(f)
    with open(transformed_path, "r") as f:
        transformed = json.load(f)

    orig_sizes = Counter(collect_sizes(original))
    trans_sizes = Counter(collect_sizes(transformed))

    print("\n=== Size Comparison ===")
    print("\nOriginal top sizes:")
    for (w, h), count in orig_sizes.most_common(10):
        print(f"  {w}x{h}: {count}")
    print("\nTransformed top sizes:")
    for (w, h), count in trans_sizes.most_common(10):
        print(f"  {w}x{h}: {count}")


if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.dirname(script_dir)

    default_input = os.path.join(project_dir, "eez/HD2 Macropad.eez-project")
    default_output = os.path.join(project_dir, "eez/HD2 Macropad-elecrow7-transformed.eez-project")

    input_path = sys.argv[1] if len(sys.argv) > 1 else default_input
    output_path = sys.argv[2] if len(sys.argv) > 2 else default_output

    transform_eez_project(input_path, output_path)
    compare_projects(input_path, output_path)
