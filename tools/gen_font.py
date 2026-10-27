#!/usr/bin/env python3
"""Generate the English and Chinese bitmap font atlases.

English keeps the original fixed 8x14 monospace cells.  Chinese uses 16x16
glyph areas inside padded 18x18 cells, with an 8px ASCII advance for
digits/Latin and a 16px advance for CJK characters.  Only characters used by
the string table (plus printable ASCII) are packed, keeping the atlas small
enough for the PSP.
"""
from __future__ import annotations

import csv
import json
import subprocess
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
SRC_DIR = ROOT / "assets" / "src"
GEN_DIR = ROOT / "assets" / "gen"
CSV_PATH = ROOT / "assets" / "strings.csv"

# English atlas (unchanged from M0).
CELL_W = 8
CELL_H = 14
COLS = 16
ROWS = 8
ATLAS_W = CELL_W * COLS
ATLAS_H = 128
FIRST = 32
LAST = 126
FONT_SIZE = 11

# Chinese atlas.  Each 16x16 glyph sits inside an 18x18 cell with a one-pixel
# transparent gutter.  The gutter keeps linear filtering from bleeding a
# neighbouring glyph into large gold-stamped menu headings; advances and quad
# sizes still use the 16px glyph area.
ZH_CELL = 18
ZH_GLYPH = 16
ZH_ATLAS_W = 512
ZH_COLS = ZH_ATLAS_W // ZH_CELL
ZH_ASCII_SIZE = 11
ZH_CJK_SIZE = 14


def find_mono_font() -> Path:
    candidates = [
        Path("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"),
        Path("/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf"),
    ]
    for path in candidates:
        if path.exists():
            return path
    try:
        out = subprocess.check_output(
            ["fc-match", "-f", "%{file}", "monospace"], text=True
        ).strip()
        if out:
            return Path(out)
    except Exception:
        pass
    raise SystemExit("no monospace font found")


def find_cjk_font() -> tuple[Path, int]:
    candidates = [
        (Path("/usr/share/fonts/opentype/noto/NotoSansCJK-Medium.ttc"), 2),
        (Path("/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc"), 0),
    ]
    for path, index in candidates:
        if path.exists():
            return path, index
    try:
        out = subprocess.check_output(
            ["fc-match", "-f", "%{file}", "sans-serif:lang=zh-cn"], text=True
        ).strip()
        if out:
            return Path(out), 0
    except Exception:
        pass
    raise SystemExit("no Chinese font found")


def collect_chars() -> set[str]:
    # Only pack characters that the UI can actually draw, plus all digits and
    # the few hard-coded punctuation marks used by non-string UI code.  This
    # keeps the Chinese atlas at 512x256 instead of 512x512 on PSP.
    chars: set[str] = set("0123456789,!?>")
    with CSV_PATH.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            for key in ("en", "zh"):
                for ch in row.get(key) or "":
                    if ch not in "\r\n\t":
                        chars.add(ch)
    return chars


def generate_en(font_path: Path) -> None:
    font = ImageFont.truetype(str(font_path), FONT_SIZE)
    image = Image.new("RGBA", (ATLAS_W, ATLAS_H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)

    for code in range(FIRST, LAST + 1):
        idx = code - FIRST
        col = idx % COLS
        row = idx // COLS
        x = col * CELL_W
        y = row * CELL_H
        ch = chr(code)
        if ch == " ":
            continue
        bbox = draw.textbbox((0, 0), ch, font=font, anchor="ls")
        glyph_w = bbox[2] - bbox[0]
        dx = x + (CELL_W - glyph_w) / 2 - bbox[0]
        draw.text((dx, y + 11), ch, font=font, fill=(255, 255, 255, 255),
                  anchor="ls")

    SRC_DIR.mkdir(parents=True, exist_ok=True)
    GEN_DIR.mkdir(parents=True, exist_ok=True)
    png_path = SRC_DIR / "font_en.png"
    json_path = GEN_DIR / "font_en.json"
    image.save(png_path)
    meta = {
        "atlas_w": ATLAS_W,
        "atlas_h": ATLAS_H,
        "cell_w": CELL_W,
        "cell_h": CELL_H,
        "cols": COLS,
        "rows": ROWS,
        "first": FIRST,
        "last": LAST,
        "font": str(font_path),
    }
    json_path.write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
    print(f"generated {png_path.relative_to(ROOT)} and {json_path.relative_to(ROOT)}")


def generate_zh(latin_font_path: Path, cjk_font_path: Path,
                cjk_index: int, chars: set[str]) -> None:
    codepoints = sorted(ord(ch) for ch in chars)
    if len(codepoints) > 1024:
        raise SystemExit(
            f"Chinese atlas needs {len(codepoints)} glyphs, max is 1024"
        )

    rows = (len(codepoints) + ZH_COLS - 1) // ZH_COLS
    atlas_h = 128
    while atlas_h < rows * ZH_CELL:
        atlas_h *= 2
    image = Image.new("RGBA", (ZH_ATLAS_W, atlas_h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    latin_font = ImageFont.truetype(str(latin_font_path), ZH_ASCII_SIZE)
    cjk_font = ImageFont.truetype(
        str(cjk_font_path), ZH_CJK_SIZE, index=cjk_index
    )

    glyphs: list[dict[str, int]] = []
    for idx, cp in enumerate(codepoints):
        col = idx % ZH_COLS
        row = idx // ZH_COLS
        cell_x = col * ZH_CELL
        cell_y = row * ZH_CELL
        x = cell_x + 1
        y = cell_y + 1
        ch = chr(cp)
        advance = 8 if cp < 128 else ZH_GLYPH
        width = 8 if cp < 128 else ZH_GLYPH
        if ch != " ":
            if cp < 128:
                bbox = draw.textbbox((0, 0), ch, font=latin_font, anchor="ls")
                glyph_w = bbox[2] - bbox[0]
                dx = x + (8 - glyph_w) / 2 - bbox[0]
                draw.text((dx, y + 13), ch, font=latin_font,
                          fill=(255, 255, 255, 255), anchor="ls")
            else:
                bbox = draw.textbbox((0, 0), ch, font=cjk_font, anchor="ls")
                glyph_w = bbox[2] - bbox[0]
                dx = x + (ZH_GLYPH - glyph_w) / 2 - bbox[0]
                draw.text((dx, y + 13), ch, font=cjk_font,
                          fill=(255, 255, 255, 255), anchor="ls")
        glyphs.append({
            "cp": cp,
            "u": x,
            "v": y,
            "w": width,
            "advance": advance,
        })

    SRC_DIR.mkdir(parents=True, exist_ok=True)
    GEN_DIR.mkdir(parents=True, exist_ok=True)
    png_path = SRC_DIR / "font_zh.png"
    json_path = GEN_DIR / "font_zh.json"
    image.save(png_path)
    meta = {
        "atlas_w": ZH_ATLAS_W,
        "atlas_h": atlas_h,
        "cell_w": ZH_GLYPH,
        "cell_h": ZH_GLYPH,
        "cols": ZH_COLS,
        "rows": atlas_h // ZH_CELL,
        "glyph_count": len(glyphs),
        "latin_font": str(latin_font_path),
        "cjk_font": str(cjk_font_path),
        "cjk_index": cjk_index,
        "glyphs": glyphs,
    }
    json_path.write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
    print(
        f"generated {png_path.relative_to(ROOT)} and "
        f"{json_path.relative_to(ROOT)} ({len(glyphs)} glyphs, "
        f"{ZH_ATLAS_W}x{atlas_h})"
    )


def main() -> None:
    latin_path = find_mono_font()
    cjk_path, cjk_index = find_cjk_font()
    generate_en(latin_path)
    generate_zh(latin_path, cjk_path, cjk_index, collect_chars())


if __name__ == "__main__":
    main()
