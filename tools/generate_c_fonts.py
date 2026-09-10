#!/usr/bin/env python3
import os

def parse_inc(path):
    with open(path, 'r') as f:
        content = f.read()
    
    bytes_list = []
    for line in content.splitlines():
        line = line.strip()
        if line.startswith('db '):
            parts = line[3:].split(',')
            for p in parts:
                p = p.strip()
                if p.startswith('0x'):
                    bytes_list.append(int(p, 16))
                elif p.isdigit():
                    bytes_list.append(int(p))
    return bytes_list

def main():
    root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    gui_dir = os.path.join(root_dir, 'gui')
    include_dir = os.path.join(root_dir, 'include')
    out_path = os.path.join(include_dir, 'opensweet_font_aa.h')

    mono_inc = os.path.join(gui_dir, 'font_mono_aa.inc')
    ui_inc = os.path.join(gui_dir, 'font_ui_aa.inc')

    mono_bytes = parse_inc(mono_inc)
    ui_bytes = parse_inc(ui_inc)

    ui_widths = ui_bytes[:95]
    ui_glyph_data = ui_bytes[95:]

    with open(out_path, 'w') as f:
        f.write("/* OpenSweet OS - Anti-Aliased Typography Tables */\n")
        f.write("/* Generated automatically from gui/font_ui_aa.inc and gui/font_mono_aa.inc */\n\n")
        f.write("#ifndef OPENSWEET_FONT_AA_H\n")
        f.write("#define OPENSWEET_FONT_AA_H\n\n")
        f.write("/* Note: freestanding types uint8_t should be defined before this */\n\n")
        
        # UI Font
        f.write("#define OS_FONT_UI_MAX_W 12\n")
        f.write("#define OS_FONT_UI_H     16\n")
        f.write("#define OS_FONT_MONO_W   8\n")
        f.write("#define OS_FONT_MONO_H   15\n\n")

        f.write("static const uint8_t os_font_ui_widths[95] = {\n    ")
        f.write(', '.join(str(w) for w in ui_widths))
        f.write("\n};\n\n")

        f.write("static const uint8_t os_font_ui_data[95][192] = {\n")
        for i in range(95):
            ch = chr(i + 32)
            if ch == '\\': ch = "'\\\\'"
            elif ch == '"': ch = "'\\\"'"
            else: ch = f"'{ch}'"
            f.write(f"    /* {i + 32} {ch} */\n    {{")
            glyph = ui_glyph_data[i * 192 : (i + 1) * 192]
            f.write(', '.join(f"0x{b:02X}" for b in glyph))
            if i < 94:
                f.write("},\n")
            else:
                f.write("}\n")
        f.write("};\n\n")

        # Mono Font
        f.write("static const uint8_t os_font_mono_data[95][120] = {\n")
        for i in range(95):
            ch = chr(i + 32)
            if ch == '\\': ch = "'\\\\'"
            elif ch == '"': ch = "'\\\"'"
            else: ch = f"'{ch}'"
            f.write(f"    /* {i + 32} {ch} */\n    {{")
            glyph = mono_bytes[i * 120 : (i + 1) * 120]
            f.write(', '.join(f"0x{b:02X}" for b in glyph))
            if i < 94:
                f.write("},\n")
            else:
                f.write("}\n")
        f.write("};\n\n")

        f.write("#endif /* OPENSWEET_FONT_AA_H */\n")

    print(f"Successfully generated {out_path}")

if __name__ == '__main__':
    main()
