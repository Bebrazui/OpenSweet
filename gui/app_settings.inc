; ==============================================================================
; Opensweet OS - Settings & Personalization Module (app_settings.inc)
; Display settings, wallpaper configuration, theme selection, and system info.
; ==============================================================================

str_set_title_disp db "DISPLAY & RESOLUTION", 0
str_set_res        db "Mode: 1920 x 1080 @ 32 bpp TrueColor (VBE LFB)", 0
str_set_fps        db "Target Refresh: 60 FPS Composited", 0

str_set_title_wall db "WALLPAPER & DESKTOP", 0
str_set_wall_cur   db "Current: Deep Space Procedural Nebula", 0
str_set_wall_png   db "Custom PNG Wallpaper: Supported via ext4 /storage", 0

str_set_title_ui   db "PERSONALIZATION & THEME", 0
str_set_accent     db "UI Accent Color: Sky Blue (#38BDF8)", 0
str_set_theme      db "Window Style: macOS Traffic Lights + Acrylic Blur", 0

; ==============================================================================
; app_settings_render: Render Settings window contents
; ECX = wx, EDX = wy, ESI = ww, EDI = wh, EAX = is_focused
; ==============================================================================
app_settings_render:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    mov r8d, ecx                        ; wx
    mov r9d, edx                        ; wy
    mov r10d, esi                       ; ww
    mov r11d, edi                       ; wh

    ; --- Section 1: Display ---
    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 46
    mov eax, 0xFF38BDF8
    lea rsi, [r15 + str_set_title_disp - kmain]
    call r2d_draw_text_ui_aa

    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 68
    mov eax, 0xFFCBD5E1
    lea rsi, [r15 + str_set_res - kmain]
    call r2d_draw_text_ui_aa

    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 88
    mov eax, 0xFF94A3B8
    lea rsi, [r15 + str_set_fps - kmain]
    call r2d_draw_text_ui_aa

    ; Divider 1
    mov ecx, r8d
    add ecx, 16
    mov edx, r9d
    add edx, 112
    mov esi, r10d
    sub esi, 32
    mov edi, 1
    mov eax, 0x30FFFFFF
    call r2d_fill_rect_alpha

    ; --- Section 2: Wallpaper ---
    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 124
    mov eax, 0xFF38BDF8
    lea rsi, [r15 + str_set_title_wall - kmain]
    call r2d_draw_text_ui_aa

    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 146
    mov eax, 0xFF10B981                 ; Emerald
    lea rsi, [r15 + str_set_wall_cur - kmain]
    call r2d_draw_text_ui_aa

    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 168
    mov eax, 0xFF94A3B8
    lea rsi, [r15 + str_set_wall_png - kmain]
    call r2d_draw_text_ui_aa

    ; Divider 2
    mov ecx, r8d
    add ecx, 16
    mov edx, r9d
    add edx, 194
    mov esi, r10d
    sub esi, 32
    mov edi, 1
    mov eax, 0x30FFFFFF
    call r2d_fill_rect_alpha

    ; --- Section 3: Personalization & Theme ---
    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 206
    mov eax, 0xFF38BDF8
    lea rsi, [r15 + str_set_title_ui - kmain]
    call r2d_draw_text_ui_aa

    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 228
    mov eax, 0xFFCBD5E1
    lea rsi, [r15 + str_set_accent - kmain]
    call r2d_draw_text_ui_aa

    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 250
    mov eax, 0xFF94A3B8
    lea rsi, [r15 + str_set_theme - kmain]
    call r2d_draw_text_ui_aa

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret
