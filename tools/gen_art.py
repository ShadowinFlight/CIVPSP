#!/usr/bin/env python3
"""Generate XMB artwork and the M1 terrain atlas for the PSP build."""
from __future__ import annotations

import json
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
SRC_DIR = ROOT / "assets" / "src"
GEN_DIR = ROOT / "assets" / "gen"
BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
MONO = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
CJK = "/usr/share/fonts/opentype/noto/NotoSansCJK-Medium.ttc"

TERRAIN_TILE = 24
TERRAIN_ATLAS_W = 128
# Rows: 0-2 terrain, 3 resources, 4-8 one unit/city palette per civilization.
TERRAIN_ATLAS_H = 256
UNIT_OWNER_ROWS = 5
# Keep in sync with World_CivColor() in src/world.c.
CIV_COLORS = [
    (74, 144, 217, 255),    # player / Aurelia - blue
    (208, 96, 80, 255),     # Borea - red
    (216, 178, 92, 255),    # Cyrene - gold
    (160, 112, 208, 255),   # Draven - purple
    (159, 176, 168, 255),   # barbarians - grey
]
TERRAIN_NAMES = [
    "grassland",
    "plains",
    "forest",
    "hills",
    "mountain",
    "desert",
    "tundra",
    "coast",
    "ocean",
]
RESOURCE_NAMES = ["horses", "iron", "stone", "gems"]
UNIT_NAMES = ["settler", "warrior", "scout", "city", "worker"]


def font(path: str, size: int) -> ImageFont.FreeTypeFont:
    try:
        return ImageFont.truetype(path, size)
    except OSError:
        return ImageFont.load_default()


def cjk_font(size: int) -> ImageFont.FreeTypeFont:
    try:
        return ImageFont.truetype(CJK, size, index=2)
    except OSError:
        return font(BOLD, size)


def draw_tile_field(draw: ImageDraw.ImageDraw, w: int, h: int, size: int) -> None:
    colors = [
        (45, 74, 83, 255),
        (58, 92, 76, 255),
        (78, 104, 72, 255),
        (92, 112, 84, 255),
        (62, 84, 104, 255),
    ]
    for y in range(0, h, size):
        for x in range(0, w, size):
            color = colors[((x // size) * 3 + (y // size) * 5) % len(colors)]
            draw.rectangle([x, y, x + size - 1, y + size - 1], fill=color)
            draw.line([x, y, x + size - 1, y], fill=(30, 46, 54, 90))
            draw.line([x, y, x, y + size - 1], fill=(30, 46, 54, 90))


def vertical_gradient(w: int, h: int, top: tuple[int, int, int],
                      bottom: tuple[int, int, int]) -> Image.Image:
    img = Image.new("RGBA", (w, h))
    draw = ImageDraw.Draw(img)
    for y in range(h):
        t = y / max(1, h - 1)
        color = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        draw.line([0, y, w, y], fill=(*color, 255))
    return img


def draw_skyline(draw: ImageDraw.ImageDraw, w: int, base_y: int,
                 color: tuple[int, int, int, int],
                 window: tuple[int, int, int, int]) -> None:
    """Small stylized city wall and towers for the XMB artwork."""
    draw.rectangle([0, base_y, w, base_y + 34], fill=(5, 11, 15, 255))
    towers = [
        (18, 44, 34, "dome"), (58, 30, 22, "flat"), (94, 54, 46, "spire"),
        (152, 34, 26, "flat"), (192, 48, 38, "dome"), (244, 28, 20, "flat"),
        (276, 58, 44, "spire"), (342, 36, 24, "flat"), (384, 50, 34, "dome"),
        (426, 28, 20, "flat"),
    ]
    for x, width, height, roof in towers:
        top = base_y - height
        draw.rectangle([x, top, x + width, base_y], fill=color)
        if roof == "dome":
            draw.ellipse([x + 3, top - 10, x + width - 3, top + 9], fill=color)
        elif roof == "spire":
            draw.polygon([(x + width // 2, top - 16),
                          (x + 2, top + 1), (x + width - 2, top + 1)],
                         fill=color)
        else:
            draw.rectangle([x + 4, top - 4, x + width - 4, top + 1], fill=color)
        for wy in range(top + 8, base_y - 4, 11):
            for wx in range(x + 6, x + width - 4, 10):
                draw.rectangle([wx, wy, wx + 2, wy + 3], fill=window)
    draw.line([0, base_y - 1, w, base_y - 1],
              fill=(216, 178, 92, 90), width=1)


def generate_icon() -> None:
    w, h = 144, 80
    img = vertical_gradient(w, h, (18, 35, 43), (5, 11, 15))
    draw = ImageDraw.Draw(img)

    # Faint compass ring behind the wordmark.
    draw.ellipse([44, 4, 100, 60], outline=(216, 178, 92, 45), width=1)
    draw.ellipse([49, 9, 95, 55], outline=(216, 178, 92, 22), width=1)
    draw_skyline(draw, w, 68, (6, 14, 18, 255), (216, 178, 92, 150))

    draw.rectangle([1, 1, w - 2, h - 2], outline=(216, 178, 92, 255), width=2)
    draw.rectangle([5, 5, w - 6, h - 6], outline=(216, 178, 92, 90), width=1)
    for x, y, sx, sy in ((8, 8, 1, 1), (w - 14, 8, -1, 1),
                          (8, h - 14, 1, -1), (w - 14, h - 14, -1, -1)):
        draw.line([x, y, x + sx * 10, y], fill=(248, 240, 218, 220), width=1)
        draw.line([x, y, x, y + sy * 7], fill=(248, 240, 218, 220), width=1)

    title_font = font(BOLD, 24)
    sub_font = cjk_font(10)
    draw.text((w // 2, 28), "CIVITAS", font=title_font,
              fill=(248, 240, 218, 255), anchor="mm")
    draw.text((w // 2, 47), "4X 策略", font=sub_font,
              fill=(216, 178, 92, 255), anchor="mm")

    SRC_DIR.mkdir(parents=True, exist_ok=True)
    img.save(SRC_DIR / "icon0.png")
    img.save(ROOT / "ICON0.PNG")


def generate_pic1() -> None:
    w, h = 480, 272
    img = vertical_gradient(w, h, (13, 28, 37), (4, 8, 11))
    draw = ImageDraw.Draw(img)

    # Subtle map-like grid and constellation.
    for x in range(0, w, 32):
        draw.line([x, 0, x, h], fill=(216, 178, 92, 12), width=1)
    for y in range(0, h, 32):
        draw.line([0, y, w, y], fill=(216, 178, 92, 12), width=1)
    rng = random.Random(0xC1A17A5)
    for _ in range(48):
        x = rng.randrange(24, w - 24)
        y = rng.randrange(24, 174)
        draw.point((x, y), fill=(248, 240, 218, rng.randrange(35, 110)))

    # Gold sun/halo behind the title.
    draw.ellipse([168, 46, 312, 190], outline=(216, 178, 92, 28), width=1)
    draw.ellipse([184, 62, 296, 174], outline=(216, 178, 92, 36), width=1)
    draw.rectangle([0, 0, w, 4], fill=(216, 178, 92, 90))
    draw.rectangle([0, h - 4, w, h], fill=(216, 178, 92, 40))

    draw_skyline(draw, w, 239, (6, 14, 18, 255), (216, 178, 92, 170))

    # Double frame with corner ticks.
    draw.rectangle([14, 14, w - 15, h - 15],
                   outline=(216, 178, 92, 230), width=2)
    draw.rectangle([22, 22, w - 23, h - 23],
                   outline=(216, 178, 92, 80), width=1)
    for x, y, sx, sy in ((14, 14, 1, 1), (w - 15, 14, -1, 1),
                          (14, h - 15, 1, -1), (w - 15, h - 15, -1, -1)):
        draw.line([x, y, x + sx * 28, y], fill=(248, 240, 218, 230), width=2)
        draw.line([x, y, x, y + sy * 20], fill=(248, 240, 218, 230), width=2)

    title_font = font(BOLD, 58)
    sub_font = cjk_font(18)
    tag_font = cjk_font(13)
    small_font = font(MONO, 9)

    draw.text((w // 2, 94), "CIVITAS", font=title_font,
              fill=(248, 240, 218, 255), anchor="mm")
    draw.text((w // 2, 143), "回合制 4X 策略游戏", font=sub_font,
              fill=(216, 178, 92, 255), anchor="mm")
    draw.text((w // 2, 164), "A TURN-BASED 4X STRATEGY GAME",
              font=small_font, fill=(174, 190, 180, 210), anchor="mm")
    draw.line([164, 180, w - 164, 180], fill=(216, 178, 92, 150), width=1)
    draw.text((w // 2, 198), "探索 · 建设 · 外交", font=tag_font,
              fill=(210, 218, 206, 230), anchor="mm")
    draw.text((30, h - 28), "V0.2.2 DEV", font=small_font,
              fill=(174, 190, 180, 180), anchor="ls")
    draw.text((w - 30, h - 28), "V0.2.2 BUILD", font=small_font,
              fill=(174, 190, 180, 180), anchor="rs")

    img.save(SRC_DIR / "pic1.png")
    img.save(ROOT / "PIC1.PNG")


def speckle(
    draw: ImageDraw.ImageDraw,
    x0: int,
    y0: int,
    rng: random.Random,
    colors: list[tuple[int, int, int, int]],
    count: int = 30,
) -> None:
    for _ in range(count):
        x = x0 + 1 + rng.randrange(TERRAIN_TILE - 2)
        y = y0 + 1 + rng.randrange(TERRAIN_TILE - 2)
        draw.point((x, y), fill=rng.choice(colors))


def draw_terrain_tile(
    draw: ImageDraw.ImageDraw,
    x0: int,
    y0: int,
    terrain: str,
    rng: random.Random,
) -> None:
    border = (38, 54, 49, 255)
    if terrain == "grassland":
        draw.rectangle([x0, y0, x0 + 23, y0 + 23], fill=(82, 128, 71, 255))
        speckle(draw, x0, y0, rng, [(91, 141, 77, 255), (69, 112, 63, 255), (108, 153, 82, 255)], 34)
        for i in range(5):
            tx = x0 + 3 + rng.randrange(18)
            ty = y0 + 6 + rng.randrange(14)
            draw.line([tx, ty, tx, ty - 3], fill=(130, 170, 91, 255))
            draw.line([tx, ty - 1, tx + 1, ty - 4], fill=(130, 170, 91, 255))
    elif terrain == "plains":
        draw.rectangle([x0, y0, x0 + 23, y0 + 23], fill=(148, 143, 82, 255))
        speckle(draw, x0, y0, rng, [(166, 158, 94, 255), (126, 123, 69, 255), (184, 174, 106, 255)], 32)
        for row in (6, 12, 18):
            draw.line([x0 + 2, y0 + row, x0 + 9, y0 + row], fill=(184, 174, 106, 255))
            draw.line([x0 + 13, y0 + row + 1, x0 + 21, y0 + row + 1], fill=(125, 120, 67, 255))
    elif terrain == "forest":
        draw.rectangle([x0, y0, x0 + 23, y0 + 23], fill=(50, 93, 60, 255))
        speckle(draw, x0, y0, rng, [(43, 81, 53, 255), (62, 110, 66, 255), (38, 72, 48, 255)], 28)
        for tx, ty in ((5, 16), (12, 13), (18, 17)):
            draw.rectangle([x0 + tx, y0 + ty, x0 + tx + 1, y0 + ty + 4], fill=(83, 62, 40, 255))
            draw.polygon(
                [(x0 + tx - 4, y0 + ty), (x0 + tx + 1, y0 + ty - 8), (x0 + tx + 5, y0 + ty)],
                fill=(35, 75, 48, 255),
                outline=(31, 64, 43, 255),
            )
            draw.polygon(
                [(x0 + tx - 3, y0 + ty - 4), (x0 + tx + 1, y0 + ty - 9), (x0 + tx + 4, y0 + ty - 4)],
                fill=(64, 112, 68, 255),
            )
    elif terrain == "hills":
        draw.rectangle([x0, y0, x0 + 23, y0 + 23], fill=(112, 121, 76, 255))
        speckle(draw, x0, y0, rng, [(126, 134, 83, 255), (96, 106, 68, 255)], 26)
        draw.ellipse([x0 + 1, y0 + 11, x0 + 15, y0 + 24], fill=(132, 139, 85, 255), outline=(88, 98, 63, 255))
        draw.ellipse([x0 + 10, y0 + 7, x0 + 23, y0 + 22], fill=(144, 148, 90, 255), outline=(88, 98, 63, 255))
        draw.line([x0 + 5, y0 + 15, x0 + 11, y0 + 15], fill=(159, 160, 98, 255))
        draw.line([x0 + 14, y0 + 11, x0 + 19, y0 + 11], fill=(166, 166, 102, 255))
    elif terrain == "mountain":
        draw.rectangle([x0, y0, x0 + 23, y0 + 23], fill=(104, 108, 104, 255))
        speckle(draw, x0, y0, rng, [(120, 124, 120, 255), (88, 93, 91, 255)], 24)
        draw.polygon(
            [(x0 + 2, y0 + 22), (x0 + 9, y0 + 5), (x0 + 17, y0 + 22)],
            fill=(145, 148, 145, 255),
            outline=(77, 82, 81, 255),
        )
        draw.polygon(
            [(x0 + 6, y0 + 13), (x0 + 9, y0 + 5), (x0 + 13, y0 + 13)],
            fill=(222, 226, 220, 255),
        )
        draw.polygon(
            [(x0 + 12, y0 + 22), (x0 + 18, y0 + 10), (x0 + 23, y0 + 22)],
            fill=(126, 131, 128, 255),
            outline=(77, 82, 81, 255),
        )
        draw.polygon(
            [(x0 + 16, y0 + 15), (x0 + 18, y0 + 10), (x0 + 20, y0 + 15)],
            fill=(205, 211, 207, 255),
        )
    elif terrain == "desert":
        draw.rectangle([x0, y0, x0 + 23, y0 + 23], fill=(196, 165, 98, 255))
        speckle(draw, x0, y0, rng, [(214, 184, 116, 255), (176, 146, 84, 255)], 34)
        for row in (8, 15, 21):
            draw.arc([x0 + 1, y0 + row - 5, x0 + 22, y0 + row + 5], 200, 340, fill=(222, 193, 129, 255))
            draw.arc([x0 + 5, y0 + row - 2, x0 + 23, y0 + row + 7], 200, 340, fill=(174, 143, 82, 255))
    elif terrain == "tundra":
        draw.rectangle([x0, y0, x0 + 23, y0 + 23], fill=(143, 157, 151, 255))
        speckle(draw, x0, y0, rng, [(162, 176, 170, 255), (126, 141, 137, 255), (210, 220, 215, 255)], 32)
        draw.polygon(
            [(x0 + 2, y0 + 17), (x0 + 8, y0 + 13), (x0 + 15, y0 + 17), (x0 + 11, y0 + 22), (x0 + 4, y0 + 21)],
            fill=(180, 195, 190, 255),
        )
        draw.polygon(
            [(x0 + 13, y0 + 7), (x0 + 20, y0 + 5), (x0 + 22, y0 + 11), (x0 + 16, y0 + 13)],
            fill=(197, 209, 204, 255),
        )
        for _ in range(5):
            tx = x0 + 3 + rng.randrange(18)
            ty = y0 + 4 + rng.randrange(16)
            draw.point((tx, ty), fill=(238, 244, 240, 255))
    elif terrain == "coast":
        draw.rectangle([x0, y0, x0 + 23, y0 + 23], fill=(72, 138, 154, 255))
        speckle(draw, x0, y0, rng, [(88, 158, 171, 255), (56, 119, 137, 255), (116, 181, 190, 255)], 28)
        for row in (6, 13, 20):
            draw.arc([x0 + 1, y0 + row - 3, x0 + 12, y0 + row + 3], 200, 340, fill=(177, 220, 222, 255))
            draw.arc([x0 + 10, y0 + row - 1, x0 + 23, y0 + row + 5], 200, 340, fill=(203, 234, 232, 255))
    elif terrain == "ocean":
        draw.rectangle([x0, y0, x0 + 23, y0 + 23], fill=(38, 82, 115, 255))
        speckle(draw, x0, y0, rng, [(47, 95, 130, 255), (29, 69, 100, 255), (60, 112, 145, 255)], 30)
        for row in (7, 14, 21):
            draw.arc([x0 + 1, y0 + row - 4, x0 + 12, y0 + row + 3], 200, 340, fill=(70, 124, 151, 255))
            draw.arc([x0 + 11, y0 + row - 2, x0 + 23, y0 + row + 4], 200, 340, fill=(91, 149, 172, 255))
    else:
        raise ValueError(f"unknown terrain {terrain}")

    draw.rectangle([x0, y0, x0 + 23, y0 + 23], outline=border)


def draw_resource_icon(
    draw: ImageDraw.ImageDraw,
    x0: int,
    y0: int,
    resource: str,
    rng: random.Random,
) -> None:
    cx = x0 + 12
    cy = y0 + 12
    if resource == "horses":
        draw.arc([cx - 8, cy - 9, cx + 8, cy + 9], 200, 340, fill=(145, 94, 47, 255), width=3)
        draw.line([cx - 8, cy + 1, cx - 8, cy + 8], fill=(145, 94, 47, 255), width=3)
        draw.line([cx + 8, cy + 1, cx + 8, cy + 8], fill=(145, 94, 47, 255), width=3)
        draw.line([cx - 6, cy - 2, cx - 4, cy - 4], fill=(201, 151, 94, 255))
        draw.line([cx + 6, cy - 2, cx + 4, cy - 4], fill=(201, 151, 94, 255))
    elif resource == "iron":
        draw.polygon(
            [(cx - 8, cy + 3), (cx - 5, cy - 6), (cx + 6, cy - 7), (cx + 9, cy + 1), (cx + 4, cy + 8), (cx - 5, cy + 7)],
            fill=(133, 139, 145, 255),
            outline=(72, 78, 84, 255),
        )
        draw.polygon(
            [(cx - 4, cy - 4), (cx + 5, cy - 5), (cx + 1, cy + 1), (cx - 6, cy + 1)],
            fill=(174, 179, 183, 255),
        )
        for _ in range(4):
            draw.point((cx - 4 + rng.randrange(10), cy - 3 + rng.randrange(9)), fill=(177, 99, 53, 255))
    elif resource == "stone":
        draw.ellipse([cx - 9, cy + 1, cx + 1, cy + 9], fill=(131, 134, 130, 255), outline=(78, 81, 78, 255))
        draw.ellipse([cx - 2, cy - 5, cx + 9, cy + 8], fill=(151, 153, 147, 255), outline=(78, 81, 78, 255))
        draw.ellipse([cx - 5, cy - 8, cx + 4, cy + 1], fill=(169, 170, 162, 255), outline=(83, 86, 82, 255))
        draw.line([cx - 2, cy - 5, cx + 2, cy - 2], fill=(196, 197, 188, 255))
    elif resource == "gems":
        draw.polygon(
            [(cx, cy - 9), (cx + 8, cy - 2), (cx + 5, cy + 8), (cx - 5, cy + 8), (cx - 8, cy - 2)],
            fill=(156, 76, 184, 255),
            outline=(82, 36, 108, 255),
        )
        draw.polygon(
            [(cx, cy - 9), (cx + 8, cy - 2), (cx + 1, cy - 1)],
            fill=(203, 141, 222, 255),
        )
        draw.polygon(
            [(cx, cy - 9), (cx - 8, cy - 2), (cx - 1, cy - 1)],
            fill=(175, 107, 199, 255),
        )
        draw.line([cx - 1, cy - 1, cx - 5, cy + 8], fill=(112, 54, 137, 255))
        draw.line([cx + 1, cy - 1, cx + 5, cy + 8], fill=(112, 54, 137, 255))
        draw.point((cx + 4, cy - 6), fill=(255, 240, 255, 255))
    else:
        raise ValueError(f"unknown resource {resource}")


def draw_unit_icon(
    draw: ImageDraw.ImageDraw,
    x0: int,
    y0: int,
    unit: str,
    accent: tuple[int, int, int, int],
) -> None:
    """Draw a 24x24 unit/city icon with a readable dark outline.

    ``accent`` is the owning civilization's team color.  It is applied to a
    consistent piece of each icon (tent, shield, cloak, flag/banner, or shirt)
    so the sprite itself identifies the owner.
    """
    outline = (36, 30, 24, 255)
    shadow = (24, 34, 30, 190)
    ar, ag, ab, _aa = accent
    accent_dark = (int(ar * 0.62), int(ag * 0.62), int(ab * 0.62), 255)
    accent_light = (min(255, int(ar * 1.16) + 12),
                    min(255, int(ag * 1.16) + 12),
                    min(255, int(ab * 1.16) + 12), 255)
    draw.ellipse([x0 + 5, y0 + 20, x0 + 19, y0 + 23], fill=shadow)

    if unit == "settler":
        draw.rectangle([x0 + 5, y0 + 11, x0 + 8, y0 + 16], fill=(151, 111, 64, 255), outline=outline)
        draw.polygon(
            [(x0 + 8, y0 + 18), (x0 + 7, y0 + 11), (x0 + 11, y0 + 6),
             (x0 + 16, y0 + 10), (x0 + 16, y0 + 18)],
            fill=accent,
            outline=outline,
        )
        draw.ellipse([x0 + 9, y0 + 5, x0 + 14, y0 + 10], fill=(211, 173, 125, 255), outline=outline)
        draw.polygon(
            [(x0 + 9, y0 + 7), (x0 + 10, y0 + 4), (x0 + 13, y0 + 3),
             (x0 + 15, y0 + 6), (x0 + 13, y0 + 5)],
            fill=(105, 76, 46, 255),
        )
        draw.line([x0 + 18, y0 + 5, x0 + 20, y0 + 21], fill=(112, 82, 50, 255), width=2)
        draw.point((x0 + 18, y0 + 4), fill=accent_light)
        draw.line([x0 + 9, y0 + 18, x0 + 8, y0 + 21], fill=outline)
        draw.line([x0 + 14, y0 + 18, x0 + 15, y0 + 21], fill=outline)
    elif unit == "warrior":
        draw.rectangle([x0 + 8, y0 + 10, x0 + 15, y0 + 18], fill=(139, 148, 157, 255), outline=outline)
        draw.rectangle([x0 + 9, y0 + 13, x0 + 14, y0 + 15], fill=(94, 104, 114, 255))
        draw.ellipse([x0 + 9, y0 + 5, x0 + 14, y0 + 10], fill=(203, 168, 120, 255), outline=outline)
        draw.pieslice([x0 + 8, y0 + 3, x0 + 15, y0 + 10], 180, 360, fill=(171, 178, 186, 255), outline=outline)
        draw.line([x0 + 11, y0 + 2, x0 + 12, y0 + 5], fill=accent_dark, width=2)
        draw.ellipse([x0 + 3, y0 + 9, x0 + 9, y0 + 18], fill=accent, outline=outline)
        draw.ellipse([x0 + 5, y0 + 12, x0 + 7, y0 + 14], fill=(218, 178, 92, 255))
        draw.line([x0 + 20, y0 + 3, x0 + 20, y0 + 21], fill=(126, 92, 54, 255), width=2)
        draw.polygon([(x0 + 20, y0 + 1), (x0 + 18, y0 + 5), (x0 + 22, y0 + 5)], fill=(203, 207, 211, 255))
        draw.line([x0 + 9, y0 + 18, x0 + 9, y0 + 21], fill=outline)
        draw.line([x0 + 14, y0 + 18, x0 + 14, y0 + 21], fill=outline)
    elif unit == "scout":
        draw.polygon(
            [(x0 + 8, y0 + 18), (x0 + 8, y0 + 10), (x0 + 12, y0 + 6),
             (x0 + 16, y0 + 10), (x0 + 15, y0 + 18)],
            fill=accent,
            outline=outline,
        )
        draw.ellipse([x0 + 9, y0 + 4, x0 + 14, y0 + 9], fill=(207, 170, 122, 255), outline=outline)
        draw.polygon(
            [(x0 + 9, y0 + 6), (x0 + 12, y0 + 2), (x0 + 15, y0 + 6)],
            fill=accent_dark,
        )
        draw.line([x0 + 15, y0 + 3, x0 + 18, y0 + 1], fill=(218, 178, 92, 255), width=2)
        draw.arc([x0 + 15, y0 + 6, x0 + 22, y0 + 18], 270, 90, fill=(126, 92, 54, 255), width=2)
        draw.line([x0 + 19, y0 + 6, x0 + 19, y0 + 17], fill=(214, 206, 180, 255))
        draw.line([x0 + 9, y0 + 18, x0 + 8, y0 + 21], fill=outline)
        draw.line([x0 + 14, y0 + 18, x0 + 15, y0 + 21], fill=outline)
    elif unit == "city":
        draw.rectangle([x0 + 3, y0 + 14, x0 + 21, y0 + 22], fill=(126, 126, 118, 255), outline=outline)
        for bx in range(x0 + 4, x0 + 21, 4):
            draw.rectangle([bx, y0 + 12, bx + 1, y0 + 14], fill=(150, 150, 141, 255))
        draw.rectangle([x0 + 5, y0 + 8, x0 + 9, y0 + 20], fill=(144, 144, 134, 255), outline=outline)
        draw.rectangle([x0 + 14, y0 + 5, x0 + 18, y0 + 20], fill=(155, 155, 144, 255), outline=outline)
        draw.polygon([(x0 + 4, y0 + 8), (x0 + 7, y0 + 4), (x0 + 10, y0 + 8)], fill=(120, 74, 58, 255), outline=outline)
        draw.polygon([(x0 + 13, y0 + 5), (x0 + 16, y0 + 1), (x0 + 19, y0 + 5)], fill=(120, 74, 58, 255), outline=outline)
        draw.rectangle([x0 + 10, y0 + 11, x0 + 14, y0 + 22], fill=(168, 160, 142, 255), outline=outline)
        draw.rectangle([x0 + 11, y0 + 17, x0 + 13, y0 + 22], fill=(58, 46, 36, 255))
        draw.rectangle([x0 + 11, y0 + 11, x0 + 13, y0 + 16], fill=accent, outline=outline)
        draw.line([x0 + 11, y0 + 14, x0 + 13, y0 + 14], fill=outline)
        draw.point((x0 + 7, y0 + 11), fill=(232, 198, 112, 255))
        draw.point((x0 + 16, y0 + 8), fill=(232, 198, 112, 255))
        draw.line([x0 + 16, y0 + 1, x0 + 16, y0 - 2], fill=(120, 92, 54, 255))
        draw.polygon([(x0 + 16, y0 - 2), (x0 + 20, y0), (x0 + 16, y0 + 2)], fill=accent)
    elif unit == "worker":
        draw.rectangle([x0 + 7, y0 + 11, x0 + 16, y0 + 18], fill=accent, outline=outline)
        draw.line([x0 + 7, y0 + 14, x0 + 16, y0 + 14], fill=(94, 72, 48, 255))
        draw.ellipse([x0 + 9, y0 + 4, x0 + 15, y0 + 10], fill=(207, 170, 122, 255), outline=outline)
        draw.rectangle([x0 + 9, y0 + 3, x0 + 15, y0 + 5], fill=accent_dark, outline=outline)
        draw.line([x0 + 18, y0 + 4, x0 + 16, y0 + 18], fill=(126, 92, 54, 255), width=2)
        draw.arc([x0 + 12, y0 + 2, x0 + 22, y0 + 10], 180, 360, fill=(190, 196, 202, 255), width=2)
        draw.line([x0 + 9, y0 + 18, x0 + 8, y0 + 21], fill=outline)
        draw.line([x0 + 14, y0 + 18, x0 + 15, y0 + 21], fill=outline)
    else:
        raise ValueError(f"unknown unit {unit}")


def generate_terrain_atlas() -> None:
    img = Image.new("RGBA", (TERRAIN_ATLAS_W, TERRAIN_ATLAS_H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    rng = random.Random(0xC1A17A5)

    for index, terrain in enumerate(TERRAIN_NAMES):
        col = index % 3
        row = index // 3
        draw_terrain_tile(draw, col * TERRAIN_TILE, row * TERRAIN_TILE, terrain, rng)

    resource_row = 3
    for index, resource in enumerate(RESOURCE_NAMES):
        draw_resource_icon(draw, index * TERRAIN_TILE, resource_row * TERRAIN_TILE, resource, rng)

    unit_row = 4
    for owner, accent in enumerate(CIV_COLORS):
        for index, unit in enumerate(UNIT_NAMES):
            draw_unit_icon(draw, index * TERRAIN_TILE,
                           (unit_row + owner) * TERRAIN_TILE, unit, accent)

    SRC_DIR.mkdir(parents=True, exist_ok=True)
    GEN_DIR.mkdir(parents=True, exist_ok=True)
    img.save(SRC_DIR / "terrain_atlas.png")
    meta = {
        "atlas_w": TERRAIN_ATLAS_W,
        "atlas_h": TERRAIN_ATLAS_H,
        "tile_w": TERRAIN_TILE,
        "tile_h": TERRAIN_TILE,
        "terrain_cols": 3,
        "terrain_rows": 3,
        "resource_row": resource_row,
        "unit_row": unit_row,
        "unit_rows": UNIT_OWNER_ROWS,
        "terrain": TERRAIN_NAMES,
        "resources": RESOURCE_NAMES,
        "units": UNIT_NAMES,
    }
    (GEN_DIR / "terrain_atlas.json").write_text(
        json.dumps(meta, indent=2) + "\n", encoding="utf-8"
    )


def main() -> None:
    generate_icon()
    generate_pic1()
    generate_terrain_atlas()
    print("generated ICON0.PNG, PIC1.PNG, and terrain_atlas.png (terrain/resource/unit icons)")


if __name__ == "__main__":
    main()
