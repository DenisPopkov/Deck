#!/usr/bin/env python3
"""Build AppIcon.png for macOS Dock/Finder (squircle alpha + opaque edge ring)."""
from __future__ import annotations

import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError:
    print("Install Pillow: pip3 install Pillow", file=sys.stderr)
    sys.exit(1)

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "Assets"
SRC = ASSETS / "AppIcon.png"
SOURCE_MASTER = ASSETS / "IconSource.png"
CASSETTE_PNG = ASSETS / "IconSource.png"
OUT = ASSETS / "AppIcon.png"
ICON_JPEG = ASSETS / "AppIconSource.jpg"
APPSTORE_PNG = ASSETS / "AppStoreSource.png"
MACOS_SAFE_FILL = 0.84
SQUARE_MASTER_FILL = 0.8925
DOCK_TILE_SCALE = 0.84
MACOS_ARTWORK_SCALE = MACOS_SAFE_FILL
APPSTORE_ARTWORK_FILL = 0.84
BRAND_ORANGE = (253, 105, 2)  # #FD6902
ICON_BACKGROUND = BRAND_ORANGE
LOGO_NEW = ASSETS / "LogoNew.png"


def prepare_deck_source(src: Image.Image, size: int = 1024) -> Image.Image:
    """Flat brand-orange plate + white DECK lettering (no circle seam)."""
    rgba = src.convert("RGBA")
    w, h = rgba.size
    side = min(w, h)
    rgba = rgba.crop(((w - side) // 2, (h - side) // 2, (w + side) // 2, (h + side) // 2))
    rgba = rgba.resize((size, size), Image.Resampling.LANCZOS)

    out = Image.new("RGBA", (size, size), (*BRAND_ORANGE, 255))
    spx = rgba.load()
    opx = out.load()
    br, bg, bb = BRAND_ORANGE

    for y in range(size):
        for x in range(size):
            r, g, b, a = spx[x, y]
            if a < 16:
                continue
            # Keep near-white text + soft antialias; flatten every orange shade to brand.
            if min(r, g, b) >= 210:
                opx[x, y] = (r, g, b, 255)
            elif r > 170 and g > 110 and b > 70:
                t = max(0.0, min(1.0, (min(r, g, b) - 110) / 100.0))
                opx[x, y] = (
                    int(br + (255 - br) * t),
                    int(bg + (255 - bg) * t),
                    int(bb + (255 - bb) * t),
                    255,
                )
            else:
                opx[x, y] = (br, bg, bb, 255)

    return out


def squircle_alpha(size: int) -> Image.Image:
    mask = Image.new("L", (size, size), 0)
    radius = max(1, int(size * 0.223))
    ImageDraw.Draw(mask).rounded_rectangle(
        (0, 0, size - 1, size - 1),
        radius=radius,
        fill=255,
    )
    return mask


def load_source(path: Path, size: int = 1024) -> Image.Image:
    img = Image.open(path).convert("RGBA")
    if img.size != (size, size):
        img = img.resize((size, size), Image.Resampling.LANCZOS)
    return img


def apply_squircle(img: Image.Image) -> Image.Image:
    size = img.size[0]
    mask = squircle_alpha(size)
    r, g, b, a = img.convert("RGBA").split()
    a = Image.composite(a, Image.new("L", (size, size), 0), mask)
    out = Image.merge("RGBA", (r, g, b, a))
    px = out.load()
    for y in range(size):
        for x in range(size):
            rv, gv, bv, al = px[x, y]
            if al == 0:
                px[x, y] = (0, 0, 0, 0)
            elif al < 255:
                t = al / 255.0
                px[x, y] = (int(rv * t), int(gv * t), int(bv * t), al)
    return out


def scale_dock_tile(icon: Image.Image, tile_scale: float = DOCK_TILE_SCALE) -> Image.Image:
    """Shrink artwork for Dock safe-zone; outer margin stays transparent."""
    size = icon.size[0]
    inner = max(1, int(size * tile_scale))
    scaled = icon.resize((inner, inner), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    offset = (size - inner) // 2
    canvas.paste(scaled, (offset, offset), scaled)
    return canvas


def is_full_bleed_plate(src: Image.Image, bg: tuple[int, int, int], tolerance: int = 24) -> bool:
    """True when artwork fills the square (e.g. flat brand plate)."""
    rgba = src.convert("RGBA")
    if rgba.width != rgba.height:
        return False
    px = rgba.load()
    br, bgc, bb = bg
    for x, y in ((0, 0), (rgba.width - 1, 0), (0, rgba.height - 1), (rgba.width - 1, rgba.height - 1)):
        r, g, b, a = px[x, y]
        if a < 200:
            return False
        if abs(r - br) > tolerance or abs(g - bgc) > tolerance or abs(b - bb) > tolerance:
            return False
    return True


def compose_macos_icon(
    src: Image.Image,
    size: int = 1024,
    artwork_scale: float = MACOS_SAFE_FILL,
) -> Image.Image:
    bg = ICON_BACKGROUND
    src_rgba = src.convert("RGBA")

    if is_full_bleed_plate(src_rgba, bg):
        fitted = src_rgba if src_rgba.size == (size, size) else src_rgba.resize(
            (size, size), Image.Resampling.LANCZOS
        )
        # Round the coloured plate first; otherwise Dock shows a sharp orange square.
        return scale_dock_tile(apply_squircle(fitted))

    if abs(src_rgba.width - src_rgba.height) <= max(src_rgba.width, src_rgba.height) * 0.05:
        inner = int(size * SQUARE_MASTER_FILL)
        fitted = src_rgba.resize((inner, inner), Image.Resampling.LANCZOS)
        canvas = Image.new("RGBA", (size, size), (*bg, 255))
        offset = (size - inner) // 2
        canvas.paste(fitted, (offset, offset), fitted)
        return apply_squircle(scale_dock_tile(canvas))

    cropped = crop_to_artwork(src_rgba)
    cw, ch = cropped.size
    inner = int(size * artwork_scale)
    scale = inner / max(cw, ch)
    nw = max(1, int(cw * scale))
    nh = max(1, int(ch * scale))
    scaled = cropped.resize((nw, nh), Image.Resampling.LANCZOS).convert("RGBA")

    canvas = Image.new("RGBA", (size, size), (*bg, 255))
    canvas.paste(scaled, ((size - nw) // 2, (size - nh) // 2), scaled)
    return apply_squircle(scale_dock_tile(canvas))


def compose_icon(
    src: Image.Image,
    size: int = 1024,
    artwork_scale: float = MACOS_SAFE_FILL,
) -> Image.Image:
    return compose_macos_icon(src, size=size, artwork_scale=artwork_scale)


def _has_visible_pixels(path: Path) -> bool:
    if not path.exists():
        return False
    img = Image.open(path).convert("RGBA")
    lo, hi = img.split()[3].getextrema()
    return hi > 0 and any(c.getextrema()[1] > 0 for c in img.split()[:3])


def pick_source() -> Path:
    for path in (LOGO_NEW, CASSETTE_PNG, APPSTORE_PNG, ICON_JPEG, SRC):
        if not path.exists():
            continue
        if path == SRC and not _has_visible_pixels(path):
            continue
        return path
    return CASSETTE_PNG


def crop_to_artwork(src: Image.Image, threshold: int = 28) -> Image.Image:
    img = src.convert("RGBA")
    px = img.load()
    w, h = img.size
    min_x, min_y, max_x, max_y = w, h, 0, 0
    found = False
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a > 12 and max(r, g, b) > threshold:
                found = True
                min_x = min(min_x, x)
                min_y = min(min_y, y)
                max_x = max(max_x, x)
                max_y = max(max_y, y)
    if not found:
        return img
    pad = max(2, int(min(w, h) * 0.01))
    min_x = max(0, min_x - pad)
    min_y = max(0, min_y - pad)
    max_x = min(w - 1, max_x + pad)
    max_y = min(h - 1, max_y + pad)
    return img.crop((min_x, min_y, max_x + 1, max_y + 1))


def from_appstore_master(path: Path, size: int = 1024) -> Image.Image:
    src = Image.open(path).convert("RGBA")
    if src.size != (size, size):
        src = src.resize((size, size), Image.Resampling.LANCZOS)
    return compose_macos_icon(src, size=size, artwork_scale=APPSTORE_ARTWORK_FILL)


def main() -> None:
    source = pick_source()
    if not source.exists():
        print(f"Source not found: {source}", file=sys.stderr)
        sys.exit(1)

    raw = Image.open(source).convert("RGBA")
    master = prepare_deck_source(raw) if source.resolve() in {
        LOGO_NEW.resolve(),
        CASSETTE_PNG.resolve(),
    } else raw
    ASSETS.mkdir(parents=True, exist_ok=True)
    if source.resolve() in {LOGO_NEW.resolve(), CASSETTE_PNG.resolve()}:
        master.save(CASSETTE_PNG, "PNG")

    icon = compose_macos_icon(master)
    icon.save(OUT, "PNG")
    print(f"Wrote macOS AppIcon (squircle RGBA): {OUT}")


if __name__ == "__main__":
    main()
