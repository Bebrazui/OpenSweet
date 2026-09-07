; ==============================================================================
; Opensweet OS - Modern Acrylic Desktop Compositor (modern_desktop.inc)
; Floating Windows, Glassmorphism Dock, Status Bar, Anti-Aliased Typography,
; Modular Window Manager integration, and Zero-Lag Drag & Drop.
; ==============================================================================

include 'D:\Opensweet\gui\render2d.inc'
include 'D:\Opensweet\gui\app_term.inc'
include 'D:\Opensweet\gui\app_files.inc'
include 'D:\Opensweet\gui\app_sysmon.inc'
include 'D:\Opensweet\gui\app_settings.inc'
include 'D:\Opensweet\gui\wm.inc'

MD_TERM_ROWS = 24
MD_TERM_COLS = 64

; State Variables
align 16
md_mouse_x      dd 512
md_mouse_y      dd 384
md_mouse_prev_x dd 0
md_mouse_prev_y dd 0
md_term_dirty   db 1

; Terminal line buffers (24 rows x 72 bytes: 4 bytes color + 64 chars + 4 pad)
align 16
md_term_lines   rb (MD_TERM_ROWS * 72)
md_term_cur_row dd 0
md_term_cur_col dd 0

; Strings
str_md_title    db "Opensweet OS", 0
str_md_menu_t   db "Terminal", 0
str_md_menu_f   db "Files", 0
str_md_menu_s   db "System", 0
str_md_menu_set db "Settings", 0
str_md_clock    db "14:38:00", 0

str_md_banner1  db "Opensweet OS v0.0.3 [x86_64 Long Mode] - 1920x1080 @ 32bpp", 0
str_md_banner2  db "Type 'help' for list of available hardware and ext4 commands.", 0
str_md_prompt   db "opensweet:/#", 0

; ==============================================================================
; modern_desktop_init: Initialize desktop state, clear terminal buffer
; ==============================================================================
modern_desktop_init:
    push rax
    push rdi
    push rcx

    call r2d_init
    call wm_init

    ; Clear terminal line buffers
    lea rdi, [r15 + md_term_lines - kmain]
    mov ecx, (MD_TERM_ROWS * 72) / 4
    xor eax, eax
    rep stosd

    ; Put initial banner lines into modern terminal
    mov ecx, 0                        ; row 0
    mov edx, 0xFFA855F7               ; Purple
    lea rsi, [r15 + str_md_banner1 - kmain]
    call md_set_term_line

    mov ecx, 1                        ; row 1
    mov edx, 0xFF94A3B8               ; Slate
    lea rsi, [r15 + str_md_banner2 - kmain]
    call md_set_term_line

    mov ecx, 3                        ; row 3 prompt
    mov edx, 0xFF10B981               ; Emerald
    lea rsi, [r15 + str_md_prompt - kmain]
    call md_set_term_line

    mov dword [r15 + md_term_cur_row - kmain], 3
    mov dword [r15 + md_term_cur_col - kmain], 13

    call modern_desktop_render

    pop rcx
    pop rdi
    pop rax
    ret

; ==============================================================================
; md_set_term_line: Set row ECX to color EDX and string RSI
; ==============================================================================
md_set_term_line:
    push rdi
    push rsi
    push rax
    push rcx

    cmp ecx, MD_TERM_ROWS
    jae .stl_done

    imul ecx, 72
    lea rdi, [r15 + md_term_lines - kmain + rcx]
    mov [rdi], edx                    ; store color dword
    add rdi, 4

    ; Copy string up to 63 chars
    mov ecx, 63
.copy_ch:
    lodsb
    stosb
    test al, al
    jz .stl_done
    dec ecx
    jnz .copy_ch
    mov byte [rdi], 0

.stl_done:
    pop rcx
    pop rax
    pop rsi
    pop rdi
    ret

; ==============================================================================
; md_term_putc: Put character in AL into modern terminal buffer
; ==============================================================================
md_term_putc:
    push rax
    push rbx
    push rcx
    push rdx
    push rdi

    mov bl, al
    mov ecx, [r15 + md_term_cur_row - kmain]
    mov edx, [r15 + md_term_cur_col - kmain]

    cmp bl, 10                        ; \n
    je .handle_lf
    cmp bl, 13                        ; \r
    je .handle_cr
    cmp bl, 8                         ; \b backspace
    je .handle_bs

    cmp edx, MD_TERM_COLS - 1
    jae .handle_lf

    ; Write char at cur_row, cur_col
    imul ecx, 72
    lea rdi, [r15 + md_term_lines - kmain + rcx + 4]
    mov [rdi + rdx], bl
    inc edx
    mov byte [rdi + rdx], 0
    mov [r15 + md_term_cur_col - kmain], edx
    jmp .putc_done

.handle_cr:
    mov dword [r15 + md_term_cur_col - kmain], 0
    jmp .putc_done

.handle_lf:
    mov dword [r15 + md_term_cur_col - kmain], 0
    inc dword [r15 + md_term_cur_row - kmain]
    mov ecx, [r15 + md_term_cur_row - kmain]
    cmp ecx, MD_TERM_ROWS - 2
    jb .putc_done

    ; Scroll terminal buffer up 1 row
    push rsi
    lea rdi, [r15 + md_term_lines - kmain]
    lea rsi, [r15 + md_term_lines - kmain + 72]
    mov ecx, ((MD_TERM_ROWS - 1) * 72) / 8
    rep movsq

    ; Clear bottom row
    mov ecx, 72 / 4
    xor eax, eax
    rep stosd
    pop rsi

    mov dword [r15 + md_term_cur_row - kmain], MD_TERM_ROWS - 3
    jmp .putc_done

.handle_bs:
    test edx, edx
    jz .putc_done
    dec edx
    mov [r15 + md_term_cur_col - kmain], edx
    imul ecx, 72
    lea rdi, [r15 + md_term_lines - kmain + rcx + 4]
    mov byte [rdi + rdx], 0

.putc_done:
    mov byte [r15 + md_term_dirty - kmain], 1
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; ==============================================================================
; modern_desktop_render: Composite all modern desktop elements
; ==============================================================================
modern_desktop_render:
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

    ; Acquire GUI compositor lock to prevent concurrent daemon blitting
    mov dword [r15 + gui_lock - kmain], 1

    ; 1. Ambient Wallpaper (fast 64-bit copy from pre-rendered cache in 0.3ms)
    call r2d_draw_ambient_wallpaper

    ; 2. Top Menu Bar (height 28)
    xor ecx, ecx
    xor edx, edx
    mov esi, R2D_SCREEN_W
    mov edi, 28
    mov eax, 0xC00F172A               ; Translucent Slate 900
    call r2d_fill_rect_alpha

    ; Menu text
    mov ecx, 16
    mov edx, 6
    mov eax, 0xFFF8FAFC               ; White
    lea rsi, [r15 + str_md_title - kmain]
    call r2d_draw_text_ui_aa

    mov ecx, 140
    mov edx, 6
    mov eax, 0xFF94A3B8               ; Slate
    lea rsi, [r15 + str_md_menu_t - kmain]
    call r2d_draw_text_ui_aa

    mov ecx, 215
    mov edx, 6
    lea rsi, [r15 + str_md_menu_f - kmain]
    call r2d_draw_text_ui_aa

    mov ecx, 270
    mov edx, 6
    lea rsi, [r15 + str_md_menu_s - kmain]
    call r2d_draw_text_ui_aa

    mov ecx, 335
    mov edx, 6
    lea rsi, [r15 + str_md_menu_set - kmain]
    call r2d_draw_text_ui_aa

    ; Menu bar right clock
    mov ecx, R2D_SCREEN_W - 78
    mov edx, 6
    mov eax, 0xFFF8FAFC
    lea rsi, [r15 + str_md_clock - kmain]
    call r2d_draw_text_ui_aa

    ; 3. Render all active open windows in Z-order
    call wm_render_all

    ; 4. Floating Bottom Dock (w=320, h=68, x=800, y=996)
    mov ecx, 800
    mov edx, 996
    mov esi, 320
    mov edi, 68
    mov ebx, 18
    call r2d_draw_shadow

    mov ecx, 800
    mov edx, 996
    mov esi, 320
    mov edi, 68
    mov ebx, 18
    mov eax, 0xD80F172A               ; Translucent glass
    call r2d_draw_rounded_rect_aa

    ; Dock top rim frosted glass highlight
    mov ecx, 800 + 18
    mov edx, 996
    mov esi, 320 - 36
    mov edi, 1
    mov eax, 0x38FFFFFF
    call r2d_fill_rect_alpha

    ; 4 Icons in Dock (44x44 Modern Anti-Aliased Squircle Tiles)
    mov ecx, 800 + 18                 ; 818
    mov edx, 996 + 12                 ; 1008
    lea rsi, [r15 + icon_terminal_44x44 - kmain]
    call r2d_draw_icon_44

    mov ecx, 800 + 98                 ; 898
    mov edx, 996 + 12
    lea rsi, [r15 + icon_files_44x44 - kmain]
    call r2d_draw_icon_44

    mov ecx, 800 + 178                ; 978
    mov edx, 996 + 12
    lea rsi, [r15 + icon_sys_44x44 - kmain]
    call r2d_draw_icon_44

    mov ecx, 800 + 258                ; 1058
    mov edx, 996 + 12
    lea rsi, [r15 + icon_settings_44x44 - kmain]
    call r2d_draw_icon_44

    ; Active dot indicators under open apps
    cmp byte [r15 + wm_state - kmain + 0], WM_STATE_OPEN
    jne @f
    mov ecx, 800 + 38                 ; 838
    mov edx, 996 + 58                 ; 1054
    mov esi, 4
    mov edi, 4
    mov ebx, 2
    mov eax, 0xFFF8FAFC
    call r2d_draw_rounded_rect_aa
@@:
    cmp byte [r15 + wm_state - kmain + 1], WM_STATE_OPEN
    jne @f
    mov ecx, 800 + 118                ; 918
    mov edx, 996 + 58
    mov esi, 4
    mov edi, 4
    mov ebx, 2
    mov eax, 0xFFF8FAFC
    call r2d_draw_rounded_rect_aa
@@:
    cmp byte [r15 + wm_state - kmain + 2], WM_STATE_OPEN
    jne @f
    mov ecx, 800 + 198                ; 998
    mov edx, 996 + 58
    mov esi, 4
    mov edi, 4
    mov ebx, 2
    mov eax, 0xFFF8FAFC
    call r2d_draw_rounded_rect_aa
@@:
    cmp byte [r15 + wm_state - kmain + 3], WM_STATE_OPEN
    jne @f
    mov ecx, 800 + 278                ; 1078
    mov edx, 996 + 58
    mov esi, 4
    mov edi, 4
    mov ebx, 2
    mov eax, 0xFFF8FAFC
    call r2d_draw_rounded_rect_aa
@@:

    ; 5. Flip clean backbuffer to VRAM LFB
    call r2d_flip

    ; 6. Draw mouse cursor directly to VRAM LFB
    mov ecx, [r15 + md_mouse_x - kmain]
    mov edx, [r15 + md_mouse_y - kmain]
    mov [r15 + md_mouse_prev_x - kmain], ecx
    mov [r15 + md_mouse_prev_y - kmain], edx
    call r2d_draw_cursor_lfb

    ; Release GUI compositor lock
    mov dword [r15 + gui_lock - kmain], 0

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

; ==============================================================================
; md_handle_mouse: Delegate to Window Manager mouse dispatcher
; ==============================================================================
md_handle_mouse:
    jmp wm_handle_mouse
