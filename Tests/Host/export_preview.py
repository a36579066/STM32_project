#!/usr/bin/env python3
"""Export native renderer P6 snapshots as 2x nearest-neighbor PNGs (stdlib only)."""

import pathlib
import struct
import sys
import zlib


def chunk(kind, data):
    return (struct.pack(">I", len(data)) + kind + data
            + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))


source_dir, output_dir = map(pathlib.Path, sys.argv[1:])
output_dir.mkdir(parents=True, exist_ok=True)
for source in sorted(source_dir.glob("*.ppm")):
    name = source.stem
    magic, dimensions, maximum, rgb = (source_dir / (name + ".ppm")).read_bytes().split(b"\n", 3)
    assert magic == b"P6" and maximum == b"255"
    width, height = map(int, dimensions.split())
    assert len(rgb) == width * height * 3
    rows = bytearray()
    for y in range(height):
        row = bytearray(b"\x00")
        for x in range(width):
            start = (y * width + x) * 3
            row.extend(rgb[start:start + 3] * 2)
        rows.extend(row * 2)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width * 2, height * 2, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(rows), 9))
    png += chunk(b"IEND", b"")
    target = output_dir / (name + ".png")
    target.write_bytes(png)
    print(target)
