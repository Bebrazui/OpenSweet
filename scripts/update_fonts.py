import os
import re

with open('gui/font_ui_aa.inc') as f:
    ui_inc = f.read()

with open('gui/font_mono_aa.inc') as f:
    mono_inc = f.read()

# Parse widths
w_match = re.search(r'font_ui_widths:\s*\n\s*db\s*([0-9,\s]+)', ui_inc)
widths = [int(x.strip()) for x in w_match.group(1).split(',')]

# Parse UI bitmaps
ui_chars = re.findall(r'; Char (\d+) \((.?)\)\s*\n\s*db\s*([0-9A-Fa-fx,\s]+)', ui_inc)
# Parse Mono bitmaps
mono_chars = re.findall(r'; Char (\d+) \((.?)\)\s*\n\s*db\s*([0-9A-Fa-fx,\s]+)', mono_inc)

with open('include/opensweet_font_aa.h', 'w') as f:
    f.write('/* OpenSweet OS - Anti-Aliased Typography Tables */\n')
    f.write('/* Generated automatically from gui/font_ui_aa.inc and gui/font_mono_aa.inc */\n\n')
    f.write('#ifndef OPENSWEET_FONT_AA_H\n#define OPENSWEET_FONT_AA_H\n\n')
    f.write('#define OS_FONT_UI_MAX_W 12\n')
    f.write('#define OS_FONT_UI_H     16\n')
    f.write('#define OS_FONT_MONO_W   9\n')
    f.write('#define OS_FONT_MONO_H   16\n\n')
    
    f.write('static const uint8_t os_font_ui_widths[95] = {\n    ')
    f.write(', '.join(str(w) for w in widths))
    f.write('\n};\n\n')
    
    f.write('static const uint8_t os_font_ui_data[95][192] = {\n')
    for i, (code, ch, bytes_str) in enumerate(ui_chars):
        raw_bytes = [b.strip() for b in bytes_str.split(',')]
        safe_ch = repr(ch)[1:-1]
        f.write(f'    /* {code} {safe_ch} */\n    {{')
        f.write(', '.join(raw_bytes))
        suffix = '},\n' if i < len(ui_chars) - 1 else '}\n'
        f.write(suffix)
    f.write('};\n\n')
    
    # 9 * 16 = 144 bytes per mono glyph
    f.write('static const uint8_t os_font_mono_data[95][144] = {\n')
    for i, (code, ch, bytes_str) in enumerate(mono_chars):
        raw_bytes = [b.strip() for b in bytes_str.split(',')]
        safe_ch = repr(ch)[1:-1]
        f.write(f'    /* {code} {safe_ch} */\n    {{')
        f.write(', '.join(raw_bytes))
        suffix = '},\n' if i < len(mono_chars) - 1 else '}\n'
        f.write(suffix)
    f.write('};\n\n#endif\n')

print('Generated include/opensweet_font_aa.h successfully')
