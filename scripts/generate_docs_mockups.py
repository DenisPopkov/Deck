#!/usr/bin/env python3
"""Generate Deck UI mockups for the GitHub Pages site."""

from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "assets"

BG = (245, 240, 230)
PANEL = (250, 247, 240)
CARD = (255, 255, 255)
BORDER = (17, 17, 17)
BORDER_LIGHT = (204, 204, 204)
ACCENT = (232, 85, 0)
GREEN = (45, 106, 79)
TEXT = (17, 17, 17)
TEXT_SEC = (68, 68, 68)
TEXT_MUTED = (102, 102, 102)
SIDEBAR = (235, 228, 216)
BLUE = (74, 144, 217)


def font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    paths = [
        "/System/Library/Fonts/SFNS.ttf",
        "/System/Library/Fonts/Supplemental/Arial Bold.ttf" if bold else "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial.ttf",
    ]
    for path in paths:
        try:
            return ImageFont.truetype(path, size=size)
        except OSError:
            continue
    return ImageFont.load_default()


def rounded_rect(draw: ImageDraw.ImageDraw, xy, radius: int, fill, outline=None, width: int = 1):
    draw.rounded_rectangle(xy, radius=radius, fill=fill, outline=outline, width=width)


def dashed_rect(draw: ImageDraw.ImageDraw, box, colour, dash: int = 8):
    x0, y0, x1, y1 = box
    for edge in (
        ((x0, y0), (x1, y0)),
        ((x1, y0), (x1, y1)),
        ((x1, y1), (x0, y1)),
        ((x0, y1), (x0, y0)),
    ):
        draw_dashed_line(draw, edge[0], edge[1], colour, dash)


def draw_dashed_line(draw, p0, p1, colour, dash: int):
    x0, y0 = p0
    x1, y1 = p1
    length = math.hypot(x1 - x0, y1 - y0)
    if length == 0:
        return
    dx = (x1 - x0) / length
    dy = (y1 - y0) / length
    pos = 0.0
    draw_on = True
    while pos < length:
        seg = min(dash, length - pos)
        if draw_on:
            sx = x0 + dx * pos
            sy = y0 + dy * pos
            ex = x0 + dx * (pos + seg)
            ey = y0 + dy * (pos + seg)
            draw.line([(sx, sy), (ex, ey)], fill=colour, width=1)
        pos += seg
        draw_on = not draw_on


def draw_window_chrome(img: Image.Image, title: str = "Deck"):
    draw = ImageDraw.Draw(img)
    draw.rectangle((0, 0, img.width, 36), fill=SIDEBAR, outline=BORDER_LIGHT)
    for i, c in enumerate([(255, 95, 87), (255, 189, 46), (39, 201, 63)]):
        draw.ellipse((14 + i * 20, 12, 26 + i * 20, 24), fill=c, outline=BORDER)
    draw.text((img.width // 2, 18), title, fill=TEXT_SEC, font=font(13), anchor="mm")


def draw_button(draw, box, label, fill, text_fill=(255, 255, 255), outline=BORDER):
    rounded_rect(draw, box, 6, fill=fill, outline=outline, width=2)
    x0, y0, x1, y1 = box
    draw.text(((x0 + x1) // 2, (y0 + y1) // 2), label, fill=text_fill, font=font(12, bold=True), anchor="mm")


def draw_wizard(draw, x, y, w, active: int):
    steps = ["Add music", "Edit tracks", "Preparing", "Export"]
    step_w = w // len(steps)
    for i, name in enumerate(steps):
        cx = x + i * step_w + step_w // 2
        colour = ACCENT if i == active else TEXT_MUTED
        draw.ellipse((cx - 8, y, cx + 8, y + 16), fill=colour if i <= active else CARD, outline=colour, width=2)
        draw.text((cx, y + 26), name, fill=colour, font=font(10), anchor="mt")


def draw_sidebar(draw, x, y, h, highlight: str = "tape"):
    w = 180
    rounded_rect(draw, (x, y, x + w, y + h), 8, fill=PANEL, outline=BORDER_LIGHT)
    draw.text((x + 14, y + 14), "Tape setup", fill=TEXT_SEC, font=font(11, bold=True))

    labels = ["Type I (Normal)", "Type II (Chrome)", "Type IV (Metal)"]
    for i, label in enumerate(labels):
        box = (x + 12, y + 42 + i * 34, x + w - 12, y + 70 + i * 34)
        selected = highlight == "tape" and i == 1
        rounded_rect(
            draw,
            box,
            6,
            fill=ACCENT if selected else CARD,
            outline=BORDER if selected else BORDER_LIGHT,
            width=2 if selected else 1,
        )
        draw.text(
            (box[0] + 10, (box[1] + box[3]) // 2),
            label,
            fill=(255, 255, 255) if selected else TEXT,
            font=font(11),
            anchor="lm",
        )

    draw.text((x + 14, y + 160), "Length", fill=TEXT_SEC, font=font(11, bold=True))
    for i, label in enumerate(["C60", "C90", "C120"]):
        box = (x + 12 + i * 54, y + 182, x + 12 + i * 54 + 48, y + 210)
        selected = i == 1
        rounded_rect(
            draw,
            box,
            6,
            fill=ACCENT if selected else CARD,
            outline=BORDER if selected else BORDER_LIGHT,
            width=2 if selected else 1,
        )
        draw.text(((box[0] + box[2]) // 2, (box[1] + box[3]) // 2), label, fill=(255, 255, 255) if selected else TEXT, font=font(11, bold=True), anchor="mm")
    return w


def draw_waveform(draw, box, colour, seed: float = 0.0):
    x0, y0, x1, y1 = box
    mid = (y0 + y1) // 2
    amp = (y1 - y0) // 3
    points = []
    for x in range(x0, x1, 3):
        t = (x - x0) / max(1, x1 - x0)
        v = math.sin((t * 14 + seed) * math.pi) * 0.55 + math.sin((t * 37 + seed) * math.pi) * 0.25
        points.append((x, int(mid - v * amp)))
    if len(points) > 1:
        draw.line(points, fill=colour, width=2)


def mockup_drop() -> Image.Image:
    img = Image.new("RGB", (960, 600), BG)
    draw = ImageDraw.Draw(img)
    draw_window_chrome(img)
    draw.rectangle((0, 36, img.width, img.height), fill=BG)

    sb_w = draw_sidebar(draw, 16, 52, 500)
    main_x = 16 + sb_w + 16
    main_w = img.width - main_x - 16
    draw_wizard(draw, main_x, 52, main_w, active=0)

    top_y = 96
    rounded_rect(draw, (main_x, top_y, img.width - 16, top_y + 420), 10, fill=CARD, outline=BORDER_LIGHT)
    drop_box = (main_x + 24, top_y + 24, img.width - 40, top_y + 320)
    dashed_rect(draw, drop_box, BORDER_LIGHT)
    draw.text(((drop_box[0] + drop_box[2]) // 2, (drop_box[1] + drop_box[3]) // 2 - 20), "Drop music here", fill=TEXT, font=font(22, bold=True), anchor="mm")
    draw_button(draw, (main_x + main_w // 2 - 86, drop_box[3] - 30, main_x + main_w // 2 + 86, drop_box[3] + 6), "Import folder", CARD, TEXT, BORDER)

    draw_button(draw, (main_x, top_y + 360, main_x + 110, top_y + 396), "Import audio", CARD, TEXT, BORDER)
    draw_button(draw, (main_x + 120, top_y + 360, main_x + 210, top_y + 396), "Prepare", ACCENT)
    return img


def mockup_tape() -> Image.Image:
    img = Image.new("RGB", (960, 600), BG)
    draw = ImageDraw.Draw(img)
    draw_window_chrome(img)
    draw.rectangle((0, 36, img.width, img.height), fill=BG)
    sb_w = draw_sidebar(draw, 16, 52, 520, highlight="tape")

    panel = (16 + sb_w + 32, 80, img.width - 32, img.height - 32)
    rounded_rect(draw, panel, 12, fill=CARD, outline=BORDER_LIGHT)
    draw.text((panel[0] + 24, panel[1] + 24), "Match your blank tape", fill=TEXT, font=font(24, bold=True))
    draw.text((panel[0] + 24, panel[1] + 62), "Deck adapts EQ, headroom, and limiting to the formulation you select.", fill=TEXT_SEC, font=font(14))

    cards = [
        ("Type I", "Normal bias · warm, forgiving"),
        ("Type II", "Chrome · brighter highs"),
        ("Type IV", "Metal · maximum headroom"),
    ]
    for i, (title, sub) in enumerate(cards):
        cx = panel[0] + 24 + i * 210
        cy = panel[1] + 120
        box = (cx, cy, cx + 190, cy + 150)
        selected = i == 1
        rounded_rect(draw, box, 10, fill=(255, 244, 236) if selected else PANEL, outline=ACCENT if selected else BORDER_LIGHT, width=3 if selected else 1)
        draw.text((box[0] + 16, box[1] + 20), title, fill=TEXT, font=font(18, bold=True))
        draw.text((box[0] + 16, box[1] + 52), sub, fill=TEXT_SEC, font=font(12))
    return img


def mockup_mixtape() -> Image.Image:
    img = Image.new("RGB", (960, 600), BG)
    draw = ImageDraw.Draw(img)
    draw_window_chrome(img)
    draw.rectangle((0, 36, img.width, img.height), fill=BG)
    sb_w = draw_sidebar(draw, 16, 52, 500)
    main_x = 16 + sb_w + 16
    draw_wizard(draw, main_x, 52, img.width - main_x - 16, active=1)

    panel = (main_x, 96, img.width - 16, img.height - 24)
    rounded_rect(draw, panel, 10, fill=CARD, outline=BORDER_LIGHT)
    draw.text((panel[0] + 20, panel[1] + 16), "Side A — 42:18 / 45:00", fill=GREEN, font=font(13, bold=True))

    tracks = [
        ("01", "Midnight Drive", "3:42"),
        ("02", "Neon Static", "4:11"),
        ("03", "Tape Hiss Dreams", "5:03"),
        ("04", "Side Street", "3:58"),
        ("05", "Analog Summer", "4:27"),
    ]
    y = panel[1] + 52
    for num, title, dur in tracks:
        row = (panel[0] + 16, y, panel[2] - 16, y + 44)
        rounded_rect(draw, row, 8, fill=PANEL, outline=BORDER_LIGHT)
        draw.text((row[0] + 14, (row[1] + row[3]) // 2), num, fill=TEXT_MUTED, font=font(12), anchor="lm")
        draw.text((row[0] + 44, (row[1] + row[3]) // 2), title, fill=TEXT, font=font(13), anchor="lm")
        draw.text((row[2] - 14, (row[1] + row[3]) // 2), dur, fill=TEXT_SEC, font=font(12), anchor="rm")
        y += 52
    return img


def mockup_export() -> Image.Image:
    img = Image.new("RGB", (960, 600), BG)
    draw = ImageDraw.Draw(img)
    draw_window_chrome(img)
    draw.rectangle((0, 36, img.width, img.height), fill=BG)
    sb_w = draw_sidebar(draw, 16, 52, 500)
    main_x = 16 + sb_w + 16
    draw_wizard(draw, main_x, 52, img.width - main_x - 16, active=3)

    panel = (main_x, 96, img.width - 16, img.height - 80)
    rounded_rect(draw, panel, 10, fill=CARD, outline=BORDER_LIGHT)

    before = (panel[0] + 20, panel[1] + 20, panel[2] - 20, panel[1] + 200)
    after = (panel[0] + 20, panel[1] + 220, panel[2] - 20, panel[1] + 400)
    rounded_rect(draw, before, 8, fill=PANEL, outline=BORDER_LIGHT)
    rounded_rect(draw, after, 8, fill=PANEL, outline=BORDER_LIGHT)
    draw.text((before[0] + 12, before[1] + 10), "Before", fill=TEXT_SEC, font=font(12, bold=True))
    draw.text((after[0] + 12, after[1] + 10), "After", fill=ACCENT, font=font(12, bold=True))
    draw_waveform(draw, (before[0] + 12, before[1] + 36, before[2] - 12, before[3] - 12), TEXT_MUTED, 0.2)
    draw_waveform(draw, (after[0] + 12, after[1] + 36, after[2] - 12, after[3] - 12), ACCENT, 1.1)

    draw_button(draw, (panel[0] + 20, panel[3] - 56, panel[0] + 100, panel[3] - 20), "Before", CARD, TEXT, BORDER)
    draw_button(draw, (panel[0] + 110, panel[3] - 56, panel[0] + 180, panel[3] - 20), "After", CARD, TEXT, BORDER)
    draw_button(draw, (panel[2] - 150, panel[3] - 56, panel[2] - 20, panel[3] - 20), "Export WAV", GREEN)
    return img


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    exports = {
        "screenshot-drop.png": mockup_drop(),
        "screenshot-tape.png": mockup_tape(),
        "screenshot-mixtape.png": mockup_mixtape(),
        "screenshot-export.png": mockup_export(),
        "screenshot-hero.png": mockup_drop(),
    }
    for name, image in exports.items():
        path = OUT / name
        image.save(path, "PNG", optimize=True)
        print(f"Wrote {path}")


if __name__ == "__main__":
    main()
