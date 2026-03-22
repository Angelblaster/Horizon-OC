#!/usr/bin/env python3
"""
Merge manual and machine translation files.
Uses manual translation if available, otherwise falls back to machine.

Usage:
    python merge_translations.py <manual_file> <machine_file> <output_file>

Examples:
    python merge_translations.py lang/fr_manual.json lang/fr_machine.json lang/fr.json
    python merge_translations.py manual/de.json auto/de.json lang/de.json
"""

import json
import os
import re
import sys


def load_json(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()
    content = re.sub(r",\s*}", "}", content)
    return json.loads(content)


def save_json(data: dict, path: str):
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write("{\n")
        items = list(data.items())
        for i, (key, val) in enumerate(items):
            k = json.dumps(key, ensure_ascii=False)
            v = json.dumps(val, ensure_ascii=False)
            comma = "," if i < len(items) - 1 else ""
            f.write(f"    {k}: {v}{comma}\n")
        f.write("}\n")


def main():
    if len(sys.argv) < 4:
        print("Usage: python merge_translations.py <manual_file> <machine_file> <output_file>")
        sys.exit(1)

    manual_path = sys.argv[1]
    machine_path = sys.argv[2]
    output_path = sys.argv[3]

    if not os.path.isfile(machine_path):
        print(f"Error: machine file '{machine_path}' not found.")
        sys.exit(1)

    machine = load_json(machine_path)
    manual = load_json(manual_path) if os.path.isfile(manual_path) else {}

    merged = {}
    manual_count = 0
    machine_count = 0

    for key in machine:
        if key in manual and manual[key] and manual[key] != key:
            merged[key] = manual[key]
            manual_count += 1
        else:
            merged[key] = machine[key]
            machine_count += 1

    # Include any manual-only keys not in machine
    for key in manual:
        if key not in merged and manual[key]:
            merged[key] = manual[key]
            manual_count += 1

    save_json(merged, output_path)

    total = len(merged)
    print(f"Merged {total} strings -> {output_path}")
    print(f"  Manual: {manual_count}")
    print(f"  Machine: {machine_count}")


if __name__ == "__main__":
    main()