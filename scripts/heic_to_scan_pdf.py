#!/usr/bin/env python3
"""Convert HEIC photos to a high-quality photocopy-style PDF."""

from __future__ import annotations

import sys
import tempfile
from pathlib import Path

import cv2
import img2pdf
import numpy as np
from PIL import Image

try:
    from pillow_heif import register_heif_opener

    register_heif_opener()
except ImportError:
    print("Install pillow-heif: pip install pillow-heif", file=sys.stderr)
    sys.exit(1)


def load_image(path: Path) -> np.ndarray:
    with Image.open(path) as img:
        rgb = img.convert("RGB")
    return cv2.cvtColor(np.array(rgb), cv2.COLOR_RGB2BGR)


def _remove_small_blobs(binary: np.ndarray, min_area: int) -> np.ndarray:
    """Drop tiny black specks on white background."""
    ink = (binary == 0).astype(np.uint8)
    num_labels, labels, stats, _ = cv2.connectedComponentsWithStats(ink, connectivity=8)
    cleaned = binary.copy()
    for label in range(1, num_labels):
        if stats[label, cv2.CC_STAT_AREA] < min_area:
            cleaned[labels == label] = 255
    return cleaned


def _ensure_text_black_on_white(binary: np.ndarray) -> np.ndarray:
    """Document text covers a minority of the page — invert if ink dominates."""
    black_ratio = float(np.count_nonzero(binary == 0)) / float(binary.size)
    if black_ratio > 0.30:
        return 255 - binary
    return binary


def _whiten_edge_shadows(binary: np.ndarray) -> np.ndarray:
    """Fill shadows in the outer margin that touch the frame."""
    h, w = binary.shape
    margin = max(20, min(h, w) // 25)
    work = binary.copy()
    flood_mask = np.zeros((h + 2, w + 2), np.uint8)

    def fill_if_ink(x: int, y: int) -> None:
        if work[y, x] == 0:
            cv2.floodFill(work, flood_mask, (x, y), 255)
            flood_mask.fill(0)

    for y in range(margin):
        for x in range(w):
            fill_if_ink(x, y)
    for y in range(h - margin, h):
        for x in range(w):
            fill_if_ink(x, y)
    for y in range(h):
        for x in range(margin):
            fill_if_ink(x, y)
        for x in range(w - margin, w):
            fill_if_ink(x, y)

    return work


def _binarize_page(gray: np.ndarray) -> np.ndarray:
    h, w = gray.shape
    smooth = cv2.GaussianBlur(gray, (5, 5), 0)

    sigma = max(18.0, min(min(h, w) / 12.0, 45.0))
    background = cv2.GaussianBlur(smooth, (0, 0), sigmaX=sigma, sigmaY=sigma)
    diff = cv2.subtract(background, smooth)
    if int(diff.max()) > 0:
        enhanced = cv2.normalize(diff, None, 0, 255, cv2.NORM_MINMAX).astype(np.uint8)
    else:
        clahe = cv2.createCLAHE(clipLimit=1.5, tileGridSize=(8, 8))
        enhanced = clahe.apply(smooth)

    block = max(51, min(min(h, w) // 8, 101))
    if block % 2 == 0:
        block += 1
    binary = cv2.adaptiveThreshold(
        enhanced,
        255,
        cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
        cv2.THRESH_BINARY,
        block,
        14,
    )
    binary = _ensure_text_black_on_white(binary)
    binary = _whiten_edge_shadows(binary)

    speckle_area = max(12, (h * w) // 50_000)
    binary = _remove_small_blobs(binary, min_area=speckle_area)
    binary = cv2.medianBlur(binary, 3)

    morph_kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (2, 2))
    return cv2.morphologyEx(binary, cv2.MORPH_CLOSE, morph_kernel, iterations=1)


def photocopy_filter(bgr: np.ndarray) -> np.ndarray:
    """Clean document binarization: white background, crisp black text."""
    gray = cv2.cvtColor(bgr, cv2.COLOR_BGR2GRAY)
    h, w = gray.shape

    max_side = 2600
    scale = min(1.0, max_side / float(max(h, w)))
    if scale < 1.0:
        proc = cv2.resize(gray, (int(w * scale), int(h * scale)), interpolation=cv2.INTER_AREA)
        binary = _binarize_page(proc)
        return cv2.resize(binary, (w, h), interpolation=cv2.INTER_NEAREST)

    return _binarize_page(gray)


def main() -> None:
    downloads = Path.home() / "Downloads"
    input_files = sorted(downloads.glob("IMG_239*.HEIC")) + sorted(downloads.glob("IMG_240*.HEIC"))
    # Deduplicate and sort numerically by filename.
    seen: set[Path] = set()
    images: list[Path] = []
    for p in sorted(input_files, key=lambda x: x.stem):
        if p not in seen:
            seen.add(p)
            images.append(p)

    if not images:
        print("No HEIC files found in Downloads.", file=sys.stderr)
        sys.exit(1)

    output_pdf = downloads / "скан_документ.pdf"
    print(f"Processing {len(images)} images...")

    png_paths: list[Path] = []
    with tempfile.TemporaryDirectory(prefix="heic_scan_") as tmp:
        tmp_dir = Path(tmp)
        for i, src in enumerate(images, start=1):
            print(f"  [{i}/{len(images)}] {src.name}")
            bgr = load_image(src)
            processed = photocopy_filter(bgr)
            out_png = tmp_dir / f"page_{i:03d}.png"
            cv2.imwrite(str(out_png), processed, [cv2.IMWRITE_PNG_COMPRESSION, 3])
            png_paths.append(out_png)

        # Lossless PNG → PDF, pages in filename order.
        pdf_bytes = img2pdf.convert([str(p) for p in png_paths])
        output_pdf.write_bytes(pdf_bytes)

    print(f"Done: {output_pdf}")
    print(f"Size: {output_pdf.stat().st_size / 1024 / 1024:.1f} MB")


if __name__ == "__main__":
    main()
