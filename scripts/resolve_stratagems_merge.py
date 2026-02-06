#!/usr/bin/env python3
"""
Resolve stratagems.h merge conflicts by:
1. Keeping sequence[9] from HEAD (Bastion MK XVI needs 9 inputs)
2. Adding cooldown field from upstream
3. For each stratagem: keep 9-element sequence, add cooldown value
"""

import re

def resolve_stratagems_merge(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    # First, resolve the struct definition conflict
    struct_pattern = r'<<<<<<< HEAD\s+uint8_t sequence\[9\];\s+=======\s+uint8_t sequence\[8\];\s+uint16_t cooldown;\s+>>>>>>> upstream/ui-cooldown'

    struct_replacement = '''uint8_t sequence[9];
    uint16_t cooldown;'''

    content = re.sub(struct_pattern, struct_replacement, content, flags=re.MULTILINE)

    # Now resolve each stratagem entry conflict
    # Pattern: HEAD has {sequence with 9 elements, ...}, upstream has {sequence with 8 elements, cooldown value, ...}
    stratagem_pattern = r'<<<<<<< HEAD\s+\{([^}]+)\},\s+=======\s+\{([^}]+)\},\s+(\d+),\s+>>>>>>> upstream/ui-cooldown'

    def replace_stratagem(match):
        head_sequence = match.group(1).strip()
        # upstream_sequence = match.group(2).strip()  # We ignore this, keep HEAD's
        cooldown_value = match.group(3).strip()

        # Keep HEAD sequence (9 elements) and add cooldown from upstream
        return f'{{{head_sequence}}},\n        {cooldown_value},'

    content = re.sub(stratagem_pattern, replace_stratagem, content, flags=re.MULTILINE | re.DOTALL)

    # Write back
    with open(filepath, 'w') as f:
        f.write(content)

    # Count remaining conflicts
    remaining = content.count('<<<<<<< HEAD')
    return remaining

if __name__ == '__main__':
    filepath = 'src/stratagems.h'
    remaining = resolve_stratagems_merge(filepath)

    if remaining == 0:
        print(f"✓ Successfully resolved all conflicts in {filepath}")
    else:
        print(f"⚠ {remaining} conflicts remaining in {filepath}")
        print("  These may need manual resolution")
