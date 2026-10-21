#!/usr/bin/env python3
"""Validate translation coverage, printf placeholders, glyphs, and line width."""
from __future__ import annotations

import csv
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CSV_PATH = ROOT / "assets" / "strings.csv"
FONT_META = ROOT / "assets" / "gen" / "font_zh.json"
MAX_WIDTH = 464  # 480px screen minus 8px margins

PLACEHOLDER = re.compile(r"%(?:[-+ #0]*\d*(?:\.\d+)?[diouxXfcs])")


def main() -> None:
    rows = list(csv.DictReader(CSV_PATH.open(newline="", encoding="utf-8")))
    meta = json.loads(FONT_META.read_text(encoding="utf-8"))
    advances = {glyph["cp"]: glyph["advance"] for glyph in meta["glyphs"]}
    errors: list[str] = []
    max_width = (0, "")
    for row in rows:
        sid = row["id"]
        en = row["en"]
        zh = row["zh"]
        if not zh:
            errors.append(f"{sid}: missing Chinese translation")
            continue
        if PLACEHOLDER.findall(en) != PLACEHOLDER.findall(zh):
            errors.append(
                f"{sid}: placeholder mismatch: {PLACEHOLDER.findall(en)!r} "
                f"vs {PLACEHOLDER.findall(zh)!r}"
            )
        for ch in zh:
            if ord(ch) not in advances:
                errors.append(f"{sid}: glyph missing for {ch!r} (U+{ord(ch):04X})")
        width = sum(advances.get(ord(ch), 8) for ch in zh)
        if width > max_width[0]:
            max_width = (width, sid)
        if width > MAX_WIDTH:
            errors.append(f"{sid}: Chinese width {width} exceeds {MAX_WIDTH}px")

    if errors:
        for error in errors:
            print("FAIL", error)
        raise SystemExit(1)
    print(
        "PASS: i18n translation coverage, printf placeholders, glyphs, and "
        f"width (max {max_width[0]}px {max_width[1]})"
    )


if __name__ == "__main__":
    main()
