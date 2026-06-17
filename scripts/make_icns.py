#!/usr/bin/env python3
"""Build Icon.icns from macOS-safe opaque master artwork."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Install Pillow: pip3 install Pillow", file=sys.stderr)
    sys.exit(1)

sys.path.insert(0, str(Path(__file__).resolve().parent))
from round_app_icon import (  # noqa: E402
    APPSTORE_PNG,
    compose_macos_icon,
    from_appstore_master,
    load_source,
    pick_source,
)

ICONSET = Path("/tmp/Deck.iconset")
SIZES = (16, 32, 128, 256, 512)


def build_master(source: Path) -> Image.Image:
    if source.resolve() == APPSTORE_PNG.resolve() and APPSTORE_PNG.exists():
        return from_appstore_master(source)

    if source.suffix.lower() in {".jpg", ".jpeg"}:
        return compose_macos_icon(load_source(source))

    src = Image.open(source).convert("RGBA")
    return compose_macos_icon(src)


def write_iconset(master: Image.Image, iconset_dir: Path) -> None:
    iconset_dir.mkdir(parents=True, exist_ok=True)
    master = master.convert("RGBA")
    for s in SIZES:
        master.resize((s, s), Image.Resampling.LANCZOS).save(
            iconset_dir / f"icon_{s}x{s}.png", "PNG"
        )
        master.resize((s * 2, s * 2), Image.Resampling.LANCZOS).save(
            iconset_dir / f"icon_{s}x{s}@2x.png", "PNG"
        )


def main() -> None:
    if len(sys.argv) != 2:
        print(f"Usage: {Path(__file__).name} <out.icns>", file=sys.stderr)
        sys.exit(2)

    source = pick_source()
    if not source.exists():
        print(f"Source not found: {source}", file=sys.stderr)
        sys.exit(1)

    master = build_master(source)

    if ICONSET.exists():
        for p in ICONSET.glob("*.png"):
            p.unlink()
    else:
        ICONSET.mkdir(parents=True)

    write_iconset(master, ICONSET)

    out_icns = Path(sys.argv[1])
    if out_icns.exists():
        out_icns.unlink()
    subprocess.run(
        ["iconutil", "-c", "icns", "-o", str(out_icns), str(ICONSET)],
        check=True,
    )
    print(f"Wrote {out_icns} from {source}")


if __name__ == "__main__":
    main()
