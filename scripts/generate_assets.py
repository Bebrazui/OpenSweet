import os
from PIL import Image, ImageFont, ImageDraw

out_dir = r'd:\Opensweet\gui'
os.makedirs(out_dir, exist_ok=True)

# 1. Generate Monospace Terminal Font (Consolas 13pt, 8x15 cell)
GLYPH_M_W = 8
GLYPH_M_H = 15
font_mono = ImageFont.truetype('C:\\Windows\\Fonts\\consola.ttf', 13)

mono_inc_path = os.path.join(out_dir, 'font_mono_aa.inc')
with open(mono_inc_path, 'w') as f:
    f.write('; Opensweet OS - Anti-Aliased Monospace Font (Consolas 8x15, 8-bit alpha)\n')
    f.write('FONT_MONO_AA_W = 8\n')
    f.write('FONT_MONO_AA_H = 15\n')
    f.write('align 16\n')
    f.write('font_mono_aa_data:\n')
    
    for ch_code in range(32, 127):
        ch = chr(ch_code)
        im = Image.new('L', (GLYPH_M_W, GLYPH_M_H), 0)
        draw = ImageDraw.Draw(im)
        draw.text((0, -1), ch, font=font_mono, fill=255)
        raw = list(im.getdata())
        
        f.write(f'    ; Char {ch_code} ({ch})\n    db ')
        f.write(', '.join(f'0x{b:02X}' for b in raw))
        f.write('\n')

print(f"Generated {mono_inc_path}")

# 2. Generate Proportional UI Font (Segoe UI 13pt, max 12x16 cell)
GLYPH_U_MAX_W = 12
GLYPH_U_H = 16
font_ui = ImageFont.truetype('C:\\Windows\\Fonts\\segoeui.ttf', 13)

ui_inc_path = os.path.join(out_dir, 'font_ui_aa.inc')
with open(ui_inc_path, 'w') as f:
    f.write('; Opensweet OS - Anti-Aliased Proportional UI Font (Segoe UI, 8-bit alpha)\n')
    f.write('FONT_UI_AA_MAX_W = 12\n')
    f.write('FONT_UI_AA_H = 16\n')
    f.write('align 16\n')
    f.write('font_ui_widths:\n    db ')
    
    widths = []
    bitmaps = []
    for ch_code in range(32, 127):
        ch = chr(ch_code)
        bbox = font_ui.getbbox(ch)
        w = max(4, min(GLYPH_U_MAX_W, bbox[2] - bbox[0] + 2 if bbox else 6))
        if ch == ' ':
            w = 5
        widths.append(w)
        
        im = Image.new('L', (GLYPH_U_MAX_W, GLYPH_U_H), 0)
        draw = ImageDraw.Draw(im)
        draw.text((0, -1), ch, font=font_ui, fill=255)
        bitmaps.append((ch_code, ch, list(im.getdata())))

    f.write(', '.join(str(w) for w in widths))
    f.write('\n\nalign 16\nfont_ui_aa_data:\n')
    for ch_code, ch, raw in bitmaps:
        f.write(f'    ; Char {ch_code} ({ch})\n    db ')
        f.write(', '.join(f'0x{b:02X}' for b in raw))
        f.write('\n')

print(f"Generated {ui_inc_path}")

# 3. Generate Modern 32bpp Vector Icons
def create_rounded_rect(size, radius, fill):
    im = Image.new('RGBA', size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(im)
    draw.rounded_rectangle([(0, 0), (size[0]-1, size[1]-1)], radius, fill=fill)
    return im

# 3.1 Terminal Icon (Dark Slate with Emerald prompt)
term = create_rounded_rect((32, 32), 8, (15, 23, 42, 255))
d = ImageDraw.Draw(term)
d.rounded_rectangle([(0, 0), (31, 31)], 8, outline=(51, 65, 85, 255), width=1)
d.line([(8, 12), (14, 16), (8, 20)], fill=(16, 185, 129, 255), width=2)
d.line([(16, 22), (23, 22)], fill=(52, 211, 153, 255), width=2)

# 3.2 Files Icon (Sky Blue modern folder)
files = create_rounded_rect((32, 32), 8, (2, 132, 199, 255))
d = ImageDraw.Draw(files)
d.rounded_rectangle([(0, 0), (31, 31)], 8, outline=(56, 189, 248, 255), width=1)
d.rectangle([(6, 10), (14, 14)], fill=(255, 255, 255, 220))
d.rounded_rectangle([(6, 13), (25, 24)], 3, fill=(255, 255, 255, 240))

# 3.3 System Info Icon (Emerald with activity pulse)
sys_icon = create_rounded_rect((32, 32), 8, (6, 78, 59, 255))
d = ImageDraw.Draw(sys_icon)
d.rounded_rectangle([(0, 0), (31, 31)], 8, outline=(16, 185, 129, 255), width=1)
d.line([(6, 16), (11, 16), (14, 9), (18, 23), (21, 16), (26, 16)], fill=(52, 211, 153, 255), width=2)

# 3.4 Settings Icon (Slate with modern sliders)
settings = create_rounded_rect((32, 32), 8, (51, 65, 85, 255))
d = ImageDraw.Draw(settings)
d.rounded_rectangle([(0, 0), (31, 31)], 8, outline=(100, 116, 139, 255), width=1)
d.line([(8, 12), (24, 12)], fill=(226, 232, 240, 255), width=2)
d.ellipse([(12, 10), (16, 14)], fill=(56, 189, 248, 255))
d.line([(8, 20), (24, 20)], fill=(226, 232, 240, 255), width=2)
d.ellipse([(18, 18), (22, 22)], fill=(168, 85, 247, 255))

icons = [
    ('icon_terminal_32x32', term),
    ('icon_files_32x32', files),
    ('icon_sys_32x32', sys_icon),
    ('icon_settings_32x32', settings)
]

icons_inc_path = os.path.join(out_dir, 'icons_32bpp.inc')
with open(icons_inc_path, 'w') as f:
    f.write('; Opensweet OS - 32bpp RGBA Vector Icons (32x32 pixels)\n')
    f.write('ICON_SIZE = 32\n\n')
    for name, icon_img in icons:
        f.write(f'align 16\n{name}:\n')
        pixels = list(icon_img.getdata())
        for row in range(32):
            row_pixels = pixels[row*32 : (row+1)*32]
            dwords = []
            for r, g, b, a in row_pixels:
                argb = (a << 24) | (r << 16) | (g << 8) | b
                dwords.append(f'0x{argb:08X}')
            f.write('    dd ' + ', '.join(dwords) + '\n')
        f.write('\n')

print(f"Generated {icons_inc_path}")

# 4. Generate Anti-Aliased Modern Mouse Cursor (18x18 RGBA with drop shadow)
cur_im = Image.new('RGBA', (18, 18), (0, 0, 0, 0))
d = ImageDraw.Draw(cur_im)
# Drop shadow
d.polygon([(1, 1), (1, 14), (4, 11), (7, 16), (9, 15), (6, 10), (11, 10)], fill=(0, 0, 0, 110))
# Black outline
d.polygon([(0, 0), (0, 13), (3, 10), (6, 15), (8, 14), (5, 9), (10, 9)], fill=(0, 0, 0, 255))
# Crisp White interior
d.polygon([(1, 1), (1, 11), (3, 9), (5, 13), (6, 13), (4, 8), (8, 8)], fill=(255, 255, 255, 255))

cursor_inc_path = os.path.join(out_dir, 'cursor_aa.inc')
with open(cursor_inc_path, 'w') as f:
    f.write('; Opensweet OS - Anti-Aliased Mouse Cursor (18x18 RGBA)\n')
    f.write('CURSOR_AA_W = 18\n')
    f.write('CURSOR_AA_H = 18\n')
    f.write('align 16\ncursor_aa_data:\n')
    pixels = list(cur_im.getdata())
    for row in range(18):
        row_pixels = pixels[row*18 : (row+1)*18]
        dwords = []
        for r, g, b, a in row_pixels:
            argb = (a << 24) | (r << 16) | (g << 8) | b
            dwords.append(f'0x{argb:08X}')
        f.write('    dd ' + ', '.join(dwords) + '\n')

print(f"Generated {cursor_inc_path}")
print("All modern graphical assets generated successfully!")
