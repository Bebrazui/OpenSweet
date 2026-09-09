#!/usr/bin/env python3
"""
OpenSweet OS - Wallpaper Converter
Converts a 1920x1080 JPEG/PNG image into a stored-blocks PNG (RFC 1950/1951, BTYPE 0)
expected by the high-performance kernel decoder (gui/png.inc).
"""
import os
import sys
import struct
import zlib
from PIL import Image

def convert_wallpaper(src_path, dst_path):
    if not os.path.exists(src_path):
        print(f"[Wallpaper] Warning: Source image not found: {src_path}")
        return False

    print(f"[Wallpaper] Loading {src_path}...")
    img = Image.open(src_path).convert('RGB')
    if img.size != (1920, 1080):
        print(f"[Wallpaper] Resizing from {img.size} to (1920, 1080)...")
        img = img.resize((1920, 1080), Image.Resampling.LANCZOS)

    # Format raw scanlines: 1 filter byte (0 = None) + 1920*3 bytes RGB per line
    raw_bytes = bytearray()
    rgb_data = img.tobytes()
    line_len = 1920 * 3
    for y in range(1080):
        raw_bytes.append(0)  # Filter 0
        raw_bytes.extend(rgb_data[y * line_len : (y + 1) * line_len])

    # Zlib compress with level=0 (stored / uncompressed blocks)
    compressed = zlib.compress(bytes(raw_bytes), level=0)

    # PNG Signature (8 bytes)
    png_sig = b'\x89PNG\r\n\x1a\n'

    # IHDR Chunk (13 bytes data)
    ihdr_data = struct.pack('>IIBBBBB', 1920, 1080, 8, 2, 0, 0, 0)
    ihdr_crc = struct.pack('>I', zlib.crc32(b'IHDR' + ihdr_data))
    ihdr_chunk = struct.pack('>I', 13) + b'IHDR' + ihdr_data + ihdr_crc

    # IDAT Chunk
    idat_crc = struct.pack('>I', zlib.crc32(b'IDAT' + compressed))
    idat_chunk = struct.pack('>I', len(compressed)) + b'IDAT' + compressed + idat_crc

    # IEND Chunk
    iend_crc = struct.pack('>I', zlib.crc32(b'IEND'))
    iend_chunk = struct.pack('>I', 0) + b'IEND' + iend_crc

    full_png = png_sig + ihdr_chunk + idat_chunk + iend_chunk

    os.makedirs(os.path.dirname(os.path.abspath(dst_path)), exist_ok=True)
    with open(dst_path, 'wb') as f:
        f.write(full_png)

    print(f"[Wallpaper] Saved {dst_path} ({len(full_png)} bytes). Done!")
    return True

if __name__ == '__main__':
    src = sys.argv[1] if len(sys.argv) > 1 else r"C:\Users\ttt79\Downloads\opensweet_walp.jpeg"
    dst = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), "..", "build", "wallpaper.png")
    convert_wallpaper(src, dst)
