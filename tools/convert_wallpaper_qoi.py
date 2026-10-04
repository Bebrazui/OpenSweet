#!/usr/bin/env python3
"""
OpenSweet OS - Wallpaper Converter to QOI
Converts a 1920x1080 image into Quite OK Image (.qoi) format
for ultra-fast, lossless kernel decoding in <3ms.
"""
import os
import sys
import struct
from PIL import Image

def convert_to_qoi(src_path, dst_path):
    if not os.path.exists(src_path):
        print(f"[QOI] Error: source not found: {src_path}")
        return False

    print(f"[QOI] Loading {src_path}...")
    img = Image.open(src_path).convert('RGB')
    if img.size != (1920, 1080):
        print(f"[QOI] Resizing from {img.size} to (1920, 1080)...")
        img = img.resize((1920, 1080), Image.Resampling.LANCZOS)

    w, h = img.size
    pixels = img.tobytes()
    index = [(0, 0, 0, 255)] * 64
    prev_r, prev_g, prev_b, prev_a = 0, 0, 0, 255
    run = 0
    out = bytearray(b'qoif' + struct.pack('>IIBB', w, h, 3, 0))

    for i in range(0, len(pixels), 3):
        r, g, b = pixels[i], pixels[i+1], pixels[i+2]
        a = 255
        if (r, g, b, a) == (prev_r, prev_g, prev_b, prev_a):
            run += 1
            if run == 62:
                out.append(0xC0 | (run - 1))
                run = 0
        else:
            if run > 0:
                out.append(0xC0 | (run - 1))
                run = 0
            idx = (r * 3 + g * 5 + b * 7 + a * 11) % 64
            if index[idx] == (r, g, b, a):
                out.append(0x00 | idx)
            else:
                index[idx] = (r, g, b, a)
                vr = (r - prev_r + 256) % 256
                vg = (g - prev_g + 256) % 256
                vb = (b - prev_b + 256) % 256
                dr = (vr + 2) % 256 - 2
                dg = (vg + 2) % 256 - 2
                db = (vb + 2) % 256 - 2
                if -2 <= dr <= 1 and -2 <= dg <= 1 and -2 <= db <= 1:
                    out.append(0x40 | ((dr + 2) << 4) | ((dg + 2) << 2) | (db + 2))
                else:
                    dr_dg = (dr - dg + 256) % 256
                    db_dg = (db - dg + 256) % 256
                    dr_dg = (dr_dg + 8) % 256 - 8
                    db_dg = (db_dg + 8) % 256 - 8
                    if -32 <= dg <= 31 and -8 <= dr_dg <= 7 and -8 <= db_dg <= 7:
                        out.append(0x80 | (dg + 32))
                        out.append(((dr_dg + 8) << 4) | (db_dg + 8))
                    else:
                        out.append(0xFE)
                        out.extend([r, g, b])
            prev_r, prev_g, prev_b, prev_a = r, g, b, a

    if run > 0:
        out.append(0xC0 | (run - 1))
    out.extend(b'\x00' * 7 + b'\x01')

    os.makedirs(os.path.dirname(os.path.abspath(dst_path)), exist_ok=True)
    with open(dst_path, 'wb') as f:
        f.write(out)

    print(f"[QOI] Successfully saved {dst_path} ({len(out)} bytes).")
    return True

if __name__ == '__main__':
    src = sys.argv[1] if len(sys.argv) > 1 else r'C:\Users\ttt79\Downloads\opensweet_walp.jpeg'
    dst = sys.argv[2] if len(sys.argv) > 2 else r'build\wallpaper.qoi'
    convert_to_qoi(src, dst)
