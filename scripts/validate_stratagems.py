#!/usr/bin/env python3
"""
Stratagem Validator - Scrapes wiki and compares to stratagems.h

Usage: python3 scripts/validate_stratagems.py

Outputs:
- stratagems_wiki.csv: Raw wiki data
- stratagems_comparison.csv: Comparison with local definitions
- Terminal summary report
"""

import re
import csv
import sys
from pathlib import Path
from dataclasses import dataclass, field
from typing import Optional

try:
    import requests
    from bs4 import BeautifulSoup
except ImportError:
    print("Missing dependencies. Install with:")
    print("  pip install requests beautifulsoup4")
    sys.exit(1)

WIKI_URL = "https://helldivers.wiki.gg/wiki/Stratagems"
STRATAGEMS_H = Path(__file__).parent.parent / "src" / "stratagems.h"

# Direction mappings for sequence parsing
DIR_MAP = {
    'INPUT_UP': '↑',
    'INPUT_DOWN': '↓',
    'INPUT_LEFT': '←',
    'INPUT_RIGHT': '→',
}

# Ship module definitions (from configuration.h)
SHIP_MODULES = {
    'SHIP_LVC': {'value': 1, 'cooldown_reduction': 0.50, 'callin': 0.0, 'name': 'Liquid-Ventilated Cockpit'},
    'SHIP_ZBL': {'value': 2, 'cooldown_reduction': 0.10, 'callin': 0.0, 'name': 'Zero-G Breaching Logistics'},
    'SHIP_HC':  {'value': 4, 'cooldown_reduction': 0.10, 'callin': 0.0, 'name': 'Hand Carts'},
    'SHIP_MA':  {'value': 8, 'cooldown_reduction': 0.05, 'callin': 0.0, 'name': 'Morale Augmentation'},
    'SHIP_SRP': {'value': 16, 'cooldown_reduction': 0.10, 'callin': 0.0, 'name': 'Streamlined Request Process'},
    'SHIP_SS':  {'value': 32, 'cooldown_reduction': 0.10, 'callin': 0.0, 'name': 'Synthetic Supplementation'},
    'SHIP_TSU': {'value': 64, 'cooldown_reduction': 0.0, 'callin': 1.0, 'name': 'Targeting Software Upgrade'},
    'SHIP_RLS': {'value': 128, 'cooldown_reduction': 0.0, 'callin': 3.0, 'name': 'Rapid Launch System'},
    'SHIP_DT':  {'value': 256, 'cooldown_reduction': 0.0, 'callin': 3.0, 'name': 'Dynamic Tracking'},
}


@dataclass
class WikiStratagem:
    """Stratagem data scraped from wiki"""
    name: str
    category: str
    cooldown: Optional[int] = None
    callin: Optional[float] = None
    sequence: str = ""
    cost: str = ""
    unlock: str = ""


@dataclass
class LocalStratagem:
    """Stratagem data from stratagems.h"""
    name: str
    index: int
    list_type: str  # 'item' or 'base'
    cooldown: int = 0
    callin: float = 0.0
    ship_modules: int = 0
    sequence: str = ""


def parse_cooldown(text: str) -> Optional[int]:
    """Parse cooldown from wiki text like '480 sec' or '70s'"""
    if not text:
        return None
    text = text.strip().lower()
    # Handle "N/A", "Infinite", etc
    if 'n/a' in text or 'infinite' in text or not text:
        return None
    # Extract number
    match = re.search(r'(\d+)', text)
    if match:
        return int(match.group(1))
    return None


def parse_callin(text: str) -> Optional[float]:
    """Parse call-in time from wiki text"""
    if not text:
        return None
    text = text.strip().lower()
    match = re.search(r'([\d.]+)', text)
    if match:
        return float(match.group(1))
    return None


def scrape_wiki() -> list[WikiStratagem]:
    """Scrape stratagem data from wiki"""
    print(f"Fetching {WIKI_URL}...")

    headers = {'User-Agent': 'HD2-Macropad-Validator/1.0'}
    resp = requests.get(WIKI_URL, headers=headers, timeout=30)
    resp.raise_for_status()

    soup = BeautifulSoup(resp.text, 'html.parser')
    stratagems = []
    current_category = "Unknown"

    # Find all tables with stratagem data
    tables = soup.find_all('table', class_='wikitable')

    for table in tables:
        # Try to find category from preceding header
        prev = table.find_previous(['h2', 'h3', 'h4'])
        if prev:
            # Get text, stripping any edit links
            cat_text = prev.get_text(strip=True)
            # Remove "[edit]" suffix if present
            cat_text = re.sub(r'\[edit\]$', '', cat_text).strip()
            if cat_text:
                current_category = cat_text

        # Parse table rows
        rows = table.find_all('tr')
        headers = []

        for row in rows:
            # Check for header row
            th_cells = row.find_all('th')
            if th_cells and len(th_cells) > 2:
                headers = [th.get_text(strip=True).lower() for th in th_cells]
                continue

            # Data row
            cells = row.find_all(['td', 'th'])
            if len(cells) < 3:
                continue

            # Try to extract stratagem name from first few cells
            name = None
            cooldown = None
            sequence = ""
            cost = ""
            unlock = ""

            for i, cell in enumerate(cells):
                text = cell.get_text(strip=True)

                # Check for stratagem name (usually in a link, but sometimes plain text)
                link = cell.find('a')
                if link and not name:
                    link_text = link.get_text(strip=True)
                    # Filter out things that aren't stratagem names or section headers
                    section_headers = {'warbonds', 'requisition', 'free', 'medal', 'medals'}
                    if (link_text and len(link_text) > 2 and
                        not link_text.startswith('Level') and
                        link_text.lower() not in section_headers):
                        name = link_text

                # Fallback: Check for stratagem name pattern in cell text (no link)
                # Look for model numbers like TX-41, MG-43, SH-51, etc.
                if not name and i < 3:  # Only check first few columns
                    if re.match(r'^[A-Z]{1,4}[-/][A-Z]?\d+', text) or \
                       re.match(r'^[A-Z]{2,}\s+\d+', text):
                        # Looks like a model number (e.g., TX-41 Sterilizer, MG-43 Machine Gun)
                        name = text

                # Check for cooldown
                if 'sec' in text.lower() or re.match(r'^\d+\s*s$', text.lower()):
                    parsed = parse_cooldown(text)
                    if parsed and parsed > 0:
                        cooldown = parsed

                # Check for sequence (arrows)
                arrows = cell.find_all('img')
                if arrows:
                    seq_parts = []
                    for arrow in arrows:
                        alt = arrow.get('alt', '')
                        if 'up' in alt.lower():
                            seq_parts.append('↑')
                        elif 'down' in alt.lower():
                            seq_parts.append('↓')
                        elif 'left' in alt.lower():
                            seq_parts.append('←')
                        elif 'right' in alt.lower():
                            seq_parts.append('→')
                    if seq_parts:
                        sequence = ''.join(seq_parts)

                # Check for cost
                if 'slip' in text.lower() or 'medal' in text.lower() or 'free' in text.lower():
                    cost = text

                # Check for unlock level
                if text.lower().startswith('level') or re.match(r'^\d+$', text):
                    unlock = text

            if name:
                strat = WikiStratagem(
                    name=name,
                    category=current_category,
                    cooldown=cooldown,
                    sequence=sequence,
                    cost=cost,
                    unlock=unlock
                )
                stratagems.append(strat)

    return stratagems


def parse_stratagems_h() -> list[LocalStratagem]:
    """Parse stratagems from stratagems.h"""
    if not STRATAGEMS_H.exists():
        print(f"Error: {STRATAGEMS_H} not found")
        return []

    content = STRATAGEMS_H.read_text()
    stratagems = []

    # Parse strategemItemList - look for content between { and }};
    item_match = re.search(r'strategemItemList\[.*?\]\s*=\s*\{(.+?)\}\};', content, re.DOTALL)
    if item_match:
        items_text = item_match.group(1)
        stratagems.extend(parse_stratagem_list(items_text, 'item'))

    # Parse strategemBaseList
    base_match = re.search(r'strategemBaseList\[.*?\]\s*=\s*\{(.+?)\}\};', content, re.DOTALL)
    if base_match:
        base_text = base_match.group(1)
        stratagems.extend(parse_stratagem_list(base_text, 'base'))

    return stratagems


def parse_stratagem_list(text: str, list_type: str) -> list[LocalStratagem]:
    """Parse individual stratagem entries from list text"""
    stratagems = []

    # Split by entry blocks - each entry starts with // index or // index - name
    # Format can be:
    #   // 0
    #   // MG-43 Machine Gun
    # OR:
    #   // 0 - Reinforce

    # Find all entry blocks using regex
    # Pattern matches: // number (optionally followed by - name on same line or name on next line)
    pattern = r'//\s*(\d+)\s*(?:-\s*([^\n]+)|[\n\s]*//\s*([^\n]+))'
    matches = list(re.finditer(pattern, text))

    for i, match in enumerate(matches):
        index = int(match.group(1))

        # Get name from either group 2 (same line) or group 3 (next line)
        name = match.group(2) or match.group(3)
        if name:
            name = name.strip()
            # Clean up - remove trailing parenthetical like "(SHIP_MA + SHIP_SS apply)"
            name = re.sub(r'\s*\([^)]*\)\s*$', '', name)

        if not name:
            continue

        # Get the text block for this entry (until next entry or end)
        start = match.end()
        end = matches[i + 1].start() if i + 1 < len(matches) else len(text)
        entry_text = text[start:end]

        # Extract sequence from {INPUT_..., INPUT_..., ...}
        seq_match = re.search(r'\{([^}]*INPUT[^}]*)\}', entry_text)
        sequence = ""
        if seq_match:
            seq_text = seq_match.group(1)
            for direction, arrow in DIR_MAP.items():
                seq_text = seq_text.replace(direction, arrow)
            seq_text = re.sub(r'[,\s0]+', '', seq_text)
            sequence = seq_text

        # Extract numerical values after the sequence
        # Format: {sequence}, cooldown, callin, shipModules, ...
        # The values are typically on separate lines:
        #   {INPUT_DOWN, INPUT_LEFT, ...},
        #   480,
        #   7.75,
        #   SHIP_MA | SHIP_SRP,

        cooldown = 0
        callin = 0.0
        ship_modules = 0

        # Find all lines after the sequence closing brace
        lines_after_seq = entry_text.split('},')
        if len(lines_after_seq) > 1:
            remaining = lines_after_seq[1]

            # Split by commas and newlines to get individual values
            values = re.split(r'[,\n]+', remaining)
            values = [v.strip() for v in values if v.strip()]

            if values:
                # First value is cooldown
                cd_match = re.match(r'^(\d+)$', values[0])
                if cd_match:
                    cooldown = int(cd_match.group(1))

                # Second value is callin (float)
                if len(values) > 1:
                    callin_match = re.match(r'^([\d.]+)$', values[1])
                    if callin_match:
                        callin = float(callin_match.group(1))

                # Third value is ship modules
                if len(values) > 2:
                    ship_modules = parse_ship_modules(values[2])

        strat = LocalStratagem(
            name=name,
            index=index,
            list_type=list_type,
            cooldown=cooldown,
            callin=callin,
            ship_modules=ship_modules,
            sequence=sequence
        )
        stratagems.append(strat)

    return stratagems


def parse_ship_modules(text: str) -> int:
    """Parse ship modules bitmask from text like 'SHIP_MA | SHIP_SS'"""
    if not text or text == '0':
        return 0

    result = 0
    for mod_name, mod_info in SHIP_MODULES.items():
        if mod_name in text:
            result |= mod_info['value']
    return result


def calculate_modified_cooldown(cooldown: int, callin: float, ship_modules: int) -> tuple[float, float]:
    """
    Calculate cooldown with all applicable ship modules enabled.
    Returns (final_cooldown, total_callin_time)
    """
    if cooldown == 0:
        return 0.0, 0.0

    # Calculate reduction factor
    factor = 1.0
    total_callin = callin

    for mod_name, mod_info in SHIP_MODULES.items():
        # Check if this module applies to the stratagem
        if ship_modules & mod_info['value']:
            factor -= mod_info['cooldown_reduction']
            total_callin += mod_info['callin']

    final_cooldown = cooldown * factor + total_callin
    return final_cooldown, total_callin


def normalize_name(name: str) -> str:
    """Normalize stratagem name for comparison - keep more context"""
    name = name.lower().strip()
    # Remove special characters but keep spaces for word matching
    name = re.sub(r'[^a-z0-9\s]', '', name)
    # Normalize whitespace
    name = ' '.join(name.split())
    return name


def find_wiki_match(local: LocalStratagem, wiki_list: list[WikiStratagem]) -> Optional[WikiStratagem]:
    """Find matching wiki stratagem for a local one"""
    local_name = local.name.lower().strip()
    local_norm = normalize_name(local.name)

    # Priority 1: Exact name match (case-insensitive)
    for wiki in wiki_list:
        if wiki.name.lower().strip() == local_name:
            return wiki

    # Priority 2: Exact normalized match
    for wiki in wiki_list:
        if normalize_name(wiki.name) == local_norm:
            return wiki

    # Priority 3: Match key identifying words (e.g., "Orbital Smoke" vs "Eagle Smoke")
    # Split into words and require all significant words to match
    local_words = set(local_norm.split())
    # Remove common words that appear in many stratagems
    common_words = {'strike', 'barrage', 'sentry', 'gun', 'machine', 'pack', 'launcher'}
    local_key_words = local_words - common_words

    best_match = None
    best_score = 0

    for wiki in wiki_list:
        wiki_norm = normalize_name(wiki.name)
        wiki_words = set(wiki_norm.split())
        wiki_key_words = wiki_words - common_words

        # Check if all key words match
        if local_key_words and local_key_words == wiki_key_words:
            return wiki

        # Count matching words as score
        matching = len(local_words & wiki_words)
        total = max(len(local_words), len(wiki_words))
        score = matching / total if total > 0 else 0

        # Require high similarity (>70%) for partial matches
        if score > 0.7 and score > best_score:
            best_score = score
            best_match = wiki

    return best_match


def main():
    print("=" * 60)
    print("HD2 Macropad Stratagem Validator")
    print("=" * 60)
    print()

    # Scrape wiki
    wiki_stratagems = scrape_wiki()
    print(f"Found {len(wiki_stratagems)} stratagems on wiki")

    # Parse local stratagems.h
    local_stratagems = parse_stratagems_h()
    print(f"Found {len(local_stratagems)} stratagems in stratagems.h")
    print()

    # Export wiki data to CSV
    wiki_csv = Path(__file__).parent / "stratagems_wiki.csv"
    with open(wiki_csv, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Name', 'Category', 'Cooldown', 'Sequence', 'Cost', 'Unlock'])
        for s in wiki_stratagems:
            writer.writerow([s.name, s.category, s.cooldown or '', s.sequence, s.cost, s.unlock])
    print(f"Wiki data exported to: {wiki_csv}")

    # Compare and create comparison CSV with cooldown calculations
    comparison_csv = Path(__file__).parent / "stratagems_comparison.csv"

    matched = []
    mismatched_cooldowns = []
    missing_from_local = []
    local_only = []

    # Track which wiki stratagems have been matched
    matched_wiki = set()

    with open(comparison_csv, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([
            'Local Name', 'Type', 'Index', 'Local Cooldown', 'Wiki Cooldown',
            'Cooldown Match', 'Base CallIn', 'Ship Modules',
            'Modified Cooldown (All Mods)', 'Total CallIn (All Mods)',
            'Wiki Name', 'Category'
        ])

        for local in local_stratagems:
            wiki_match = find_wiki_match(local, wiki_stratagems)

            if wiki_match:
                matched_wiki.add(wiki_match.name)
                cooldown_match = "TRUE" if wiki_match.cooldown == local.cooldown else "FALSE"

                # Calculate modified cooldown with all ship modules
                mod_cooldown, mod_callin = calculate_modified_cooldown(
                    local.cooldown, local.callin, local.ship_modules
                )

                # Format ship modules
                ship_mod_str = format_ship_modules(local.ship_modules)

                writer.writerow([
                    local.name, local.list_type, local.index,
                    local.cooldown, wiki_match.cooldown or 'N/A',
                    cooldown_match, local.callin, ship_mod_str,
                    f"{mod_cooldown:.1f}" if mod_cooldown else "0",
                    f"{mod_callin:.1f}" if mod_callin else "0",
                    wiki_match.name, wiki_match.category
                ])

                if wiki_match.cooldown and wiki_match.cooldown != local.cooldown:
                    mismatched_cooldowns.append((local, wiki_match))
                else:
                    matched.append((local, wiki_match))
            else:
                # No wiki match found
                mod_cooldown, mod_callin = calculate_modified_cooldown(
                    local.cooldown, local.callin, local.ship_modules
                )
                ship_mod_str = format_ship_modules(local.ship_modules)

                writer.writerow([
                    local.name, local.list_type, local.index,
                    local.cooldown, 'NOT FOUND',
                    'N/A', local.callin, ship_mod_str,
                    f"{mod_cooldown:.1f}" if mod_cooldown else "0",
                    f"{mod_callin:.1f}" if mod_callin else "0",
                    '', ''
                ])
                local_only.append(local)

    # Find wiki stratagems not in local
    for wiki in wiki_stratagems:
        if wiki.name not in matched_wiki:
            missing_from_local.append(wiki)

    print(f"Comparison exported to: {comparison_csv}")
    print()

    # Print summary report
    print("=" * 60)
    print("SUMMARY REPORT")
    print("=" * 60)
    print()

    print(f"Total wiki stratagems:   {len(wiki_stratagems)}")
    print(f"Total local stratagems:  {len(local_stratagems)}")
    print(f"  - Item list entries:   {len([s for s in local_stratagems if s.list_type == 'item'])}")
    print(f"  - Base list entries:   {len([s for s in local_stratagems if s.list_type == 'base'])}")
    print()

    print(f"Matched (cooldown OK):   {len(matched)}")
    print(f"Cooldown mismatches:     {len(mismatched_cooldowns)}")
    print(f"Local-only (not on wiki): {len(local_only)}")
    print(f"Wiki-only (not in local): {len(missing_from_local)}")
    print()

    if mismatched_cooldowns:
        print("-" * 60)
        print("COOLDOWN MISMATCHES:")
        print("-" * 60)
        for local, wiki in mismatched_cooldowns:
            print(f"  {local.name}")
            print(f"    Local: {local.cooldown}s  |  Wiki: {wiki.cooldown}s")
        print()

    if missing_from_local:
        print("-" * 60)
        print("MISSING FROM LOCAL (on wiki but not in stratagems.h):")
        print("-" * 60)
        for wiki in missing_from_local[:20]:  # Limit output
            cd_str = f"{wiki.cooldown}s" if wiki.cooldown else "N/A"
            print(f"  [{wiki.category}] {wiki.name} (cooldown: {cd_str})")
        if len(missing_from_local) > 20:
            print(f"  ... and {len(missing_from_local) - 20} more")
        print()

    if local_only:
        print("-" * 60)
        print("LOCAL-ONLY (in stratagems.h but not matched on wiki):")
        print("-" * 60)
        for local in local_only:
            print(f"  [{local.list_type}:{local.index}] {local.name}")
        print()

    # Ship module cooldown calculation summary
    print("=" * 60)
    print("COOLDOWN CALCULATIONS (All Ship Modules Enabled)")
    print("=" * 60)
    print()
    print("Modules applied when all enabled:")
    for mod_name, mod_info in SHIP_MODULES.items():
        effect = []
        if mod_info['cooldown_reduction'] > 0:
            effect.append(f"-{int(mod_info['cooldown_reduction']*100)}% cooldown")
        if mod_info['callin'] > 0:
            effect.append(f"+{mod_info['callin']}s call-in")
        print(f"  {mod_name}: {', '.join(effect)}")
    print()

    print("Stratagems with cooldowns and ship module effects:")
    print("-" * 60)
    print(f"{'Name':<40} {'Base':>6} {'Modified':>10} {'Modules':<20}")
    print("-" * 60)

    for local in sorted(local_stratagems, key=lambda x: x.cooldown, reverse=True):
        if local.cooldown > 0:
            mod_cooldown, _ = calculate_modified_cooldown(
                local.cooldown, local.callin, local.ship_modules
            )
            ship_mod_str = format_ship_modules(local.ship_modules) or "None"
            reduction = ((local.cooldown - mod_cooldown) / local.cooldown * 100) if mod_cooldown != local.cooldown else 0

            print(f"{local.name:<40} {local.cooldown:>5}s {mod_cooldown:>9.1f}s {ship_mod_str:<20}")

    print()
    print("Done!")


def format_ship_modules(bitmask: int) -> str:
    """Format ship modules bitmask as readable string"""
    if bitmask == 0:
        return ""

    modules = []
    for mod_name, mod_info in SHIP_MODULES.items():
        if bitmask & mod_info['value']:
            # Use short name
            short = mod_name.replace('SHIP_', '')
            modules.append(short)

    return '|'.join(modules)


if __name__ == '__main__':
    main()
