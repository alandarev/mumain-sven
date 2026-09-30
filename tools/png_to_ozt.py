#!/usr/bin/env python3
"""Package a PNG as the client's 32-bit, bottom-up OZT texture (offline only)."""

import argparse
import struct
from pathlib import Path

from PIL import Image

MAX_DIMENSION = 1024


def encode(image):
    """Preserve straight-alpha RGBA pixels; the runtime premultiplies them."""
    width, height = image.size
    if not 0 < width <= MAX_DIMENSION or not 0 < height <= MAX_DIMENSION:
        raise ValueError("OZT dimensions must be between 1 and 1024")
    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, width, height, 32, 8)
    return b"OZT\0" + header + image.convert("RGBA").tobytes("raw", "BGRA", 0, -1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--fit", action="store_true", help="Downsample to the 1024-pixel loader limit")
    args = parser.parse_args()
    with Image.open(args.source) as source:
        image = source.convert("RGBA")
        if args.fit:
            image.thumbnail((MAX_DIMENSION, MAX_DIMENSION), Image.Resampling.LANCZOS)
        payload = encode(image)
    args.destination.write_bytes(payload)
    print(f"{args.destination}: {image.width}x{image.height}, {len(payload)} bytes, straight alpha")


if __name__ == "__main__":
    main()
