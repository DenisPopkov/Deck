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
OUT = ASSETS / "AppIcon.png"
ICON_JPEG = Path.home() / "Downloads" / "f9161d43778860fee7db0696ea31365f.jpg"
APPSTORE_PNG = Path.home() / "Desktop" / "appstore.png"
DESKTOP_CASSETTE_PNG = Path.home() / "Desktop" / "cassette.png"
# Cassette artwork fill inside the tile (before whole-tile Dock scaling).
MACOS_SAFE_FILL = 0.84
SQUARE_MASTER_FILL = 0.8925
# Shrink the entire squircle tile — matches Audacity / system Dock size.
DOCK_TILE_SCALE = 0.84
# Legacy alias used by make_icns import
MACOS_ARTWORK_SCALE = MACOS_SAFE_FILL
APPSTORE_ARTWORK_FILL = 0.84
ICON_BACKGROUND = (0, 0, 0)


def squircle_alpha(size: int) -> Image.Image:
    """Rounded rect mask (~22.3% radius) — transparent corners like system icons."""
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
    """Clip to macOS squircle; transparent outside, premultiplied RGB."""
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
    """Shrink the whole icon (background + artwork), not just inner art."""
    size = icon.size[0]
    inner = max(1, int(size * tile_scale))
    scaled = icon.resize((inner, inner), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    offset = (size - inner) // 2
    canvas.paste(scaled, (offset, offset), scaled)
    return canvas


def detect_background_rgb(src: Image.Image) -> tuple[int, int, int]:
    """Sample corners — cassette icon uses near-black matte."""
    img = src.convert("RGBA")
    w, h = img.size
    samples = [img.getpixel((0, 0)), img.getpixel((w - 1, 0)), img.getpixel((0, h - 1)), img.getpixel((w - 1, h - 1))]
    rs = [c[0] for c in samples]
    gs = [c[1] for c in samples]
    bs = [c[2] for c in samples]
    return (sum(rs) // len(rs), sum(gs) // len(gs), sum(bs) // len(bs))


def compose_macos_icon(
    src: Image.Image,
    size: int = 1024,
    artwork_scale: float = MACOS_SAFE_FILL,
) -> Image.Image:
    """Square icon clipped to macOS squircle (RGBA, transparent outside like system apps)."""
    bg = ICON_BACKGROUND
    src_rgba = src.convert("RGBA")

    # Square masters (cassette.png): keep full frame — no matte stripping.
    if abs(src_rgba.width - src_rgba.height) <= max(src_rgba.width, src_rgba.height) * 0.05:
        inner = int(size * SQUARE_MASTER_FILL)
        fitted = src_rgba.resize((inner, inner), Image.Resampling.LANCZOS)
        canvas = Image.new("RGBA", (size, size), (*bg, 255))
        offset = (size - inner) // 2
        canvas.paste(fitted, (offset, offset), fitted)
        return scale_dock_tile(apply_squircle(canvas))

    cropped = crop_to_artwork(src_rgba)
    cw, ch = cropped.size
    inner = int(size * artwork_scale)
    scale = inner / max(cw, ch)
    nw = max(1, int(cw * scale))
    nh = max(1, int(ch * scale))
    scaled = cropped.resize((nw, nh), Image.Resampling.LANCZOS).convert("RGBA")

    canvas = Image.new("RGBA", (size, size), (*bg, 255))
    canvas.paste(scaled, ((size - nw) // 2, (size - nh) // 2), scaled)
    return scale_dock_tile(apply_squircle(canvas))


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
    if DESKTOP_CASSETTE_PNG.exists():
        return DESKTOP_CASSETTE_PNG
    if SOURCE_MASTER.exists():
        return SOURCE_MASTER
    if SRC.exists() and _has_visible_pixels(SRC):
        return SRC
    if APPSTORE_PNG.exists():
        return APPSTORE_PNG
    if ICON_JPEG.exists():
        return ICON_JPEG
    if SRC.exists():
        return SRC
    return ICON_JPEG


def crop_to_artwork(src: Image.Image, threshold: int = 28) -> Image.Image:
    """Trim near-black margins so the cassette can be scaled larger."""
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


def fill_canvas(src: Image.Image, size: int = 1024, fill: float = 1.0) -> Image.Image:
    """Scale artwork to fill the squircle (crop trims black margins first)."""
    cropped = crop_to_artwork(src)
    cw, ch = cropped.size
    inner = int(size * fill)
    scale = inner / max(cw, ch)
    nw = max(1, int(cw * scale))
    nh = max(1, int(ch * scale))
    scaled = cropped.resize((nw, nh), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    canvas.paste(scaled, ((size - nw) // 2, (size - nh) // 2), scaled)
    return canvas


def from_appstore_master(path: Path, size: int = 1024) -> Image.Image:
    """Large cassette fill for Dock/Launchpad."""
    src = Image.open(path).convert("RGBA")
    if src.size != (size, size):
        src = src.resize((size, size), Image.Resampling.LANCZOS)
    return compose_macos_icon(src, size=size, artwork_scale=APPSTORE_ARTWORK_FILL)


def main() -> None:
    source = pick_source()
    if not source.exists():
        print(f"Source not found: {source}", file=sys.stderr)
        sys.exit(1)

    icon = compose_macos_icon(Image.open(source).convert("RGBA"))
    label = "macOS AppIcon (squircle RGBA)"

    ASSETS.mkdir(parents=True, exist_ok=True)
    icon.save(OUT, "PNG")
    print(f"Wrote {label}: {OUT}")


if __name__ == "__main__":
    main()
