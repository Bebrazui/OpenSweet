; ==============================================================================
; Opensweet OS - Modern Acrylic Desktop Compositor (modern_desktop.inc)
; Floating Windows, Glassmorphism Dock, Status Bar, Anti-Aliased Typography,
; Modular Window Manager integration, and Zero-Lag Drag & Drop.
; ==============================================================================

include 'D:\Opensweet\gui\render2d.inc'
include 'D:\Opensweet\gui\wm.inc'
include 'D:\Opensweet\kernel\app_registry.inc'

MD_TERM_ROWS = 24
MD_TERM_COLS = 64

; State Variables
align 16
md_mouse_x        dd 512
md_mouse_y        dd 384
md_mouse_prev_x   dd 0
md_mouse_prev_y   dd 0
md_mouse_fp_x     dd (512 shl 16)
md_mouse_fp_y     dd (384 shl 16)
md_mouse_vx       dd 0
md_mouse_vy       dd 0
md_term_dirty     db 1

md_dock_x       dd 800
md_dock_y       dd 996
md_dock_w       dd 320
md_dock_h       dd 68
md_dock_count   dd 4
align 16
str_tip_buf     rb 64

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
str_md_fps      db " 60 FPS", 0
str_md_banner1  db "Opensweet OS v0.0.3 [x86_64 Long Mode] - 1920x1080 @ 32bpp", 0
str_md_banner2  db "Type 'help' for list of available hardware and ext4 commands.", 0
str_md_prompt   db "opensweet:/#", 0
str_log_frender db "[DESKTOP] full_render cycles=", 0



; ==============================================================================
; modern_desktop_init: Initialize desktop state, clear terminal buffer
; ==============================================================================
modern_desktop_init:
    push rax
    push rdi
    push rcx

    call r2d_init
    call wm_init
    call app_reg_init

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
    push r12
    push r13

    ; Acquire GUI compositor lock to prevent concurrent daemon blitting
    mov dword [r15 + gui_lock - kmain], 1

    ; 1. Ambient Wallpaper (fast 64-bit copy from pre-rendered cache in 0.3ms)
    call r2d_draw_ambient_wallpaper

    ; 2. Top Menu Bar
    call md_render_menubar

    ; 2.5 Window animations (smooth Ease-Out PiP morphing transitions)
    call wm_update_animations

    ; 3. Render all active open windows in Z-order
    call wm_render_all

    ; 4. Floating Bottom Dock
    call md_render_dock

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

    pop r13
    pop r12
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
; md_format_fps: Convert gui_fps (0..999) to string " XX FPS"
; ==============================================================================
md_format_fps:
    push rax
    push rbx
    push rcx
    push rdx

    mov eax, [r15 + gui_fps - kmain]
    cmp eax, 999
    jbe @f
    mov eax, 999
@@:
    xor edx, edx
    mov ecx, 100
    div ecx                           ; eax = hundreds, edx = remainder
    test al, al
    jz .fps_no_h
    add al, '0'
    mov [r15 + str_md_fps - kmain + 0], al
    jmp .fps_tens
.fps_no_h:
    mov byte [r15 + str_md_fps - kmain + 0], ' '
.fps_tens:
    mov eax, edx
    xor edx, edx
    mov ecx, 10
    div ecx                           ; al = tens, dl = units
    add al, '0'
    add dl, '0'
    mov [r15 + str_md_fps - kmain + 1], al
    mov [r15 + str_md_fps - kmain + 2], dl
    mov byte [r15 + str_md_fps - kmain + 3], ' '
    mov byte [r15 + str_md_fps - kmain + 4], 'F'
    mov byte [r15 + str_md_fps - kmain + 5], 'P'
    mov byte [r15 + str_md_fps - kmain + 6], 'S'
    mov byte [r15 + str_md_fps - kmain + 7], 0

    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; ==============================================================================
; md_render_menubar: Top Menu Bar (height 28)
; ==============================================================================
md_render_menubar:
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
    push r12

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

    ; Real-time FPS indicator
    call md_format_fps
    mov ecx, R2D_SCREEN_W - 165
    mov edx, 6
    mov eax, 0xFF10B981               ; Vibrant Emerald Green
    lea rsi, [r15 + str_md_fps - kmain]
    call r2d_draw_text_ui_aa

    ; Menu bar right clock
    mov ecx, R2D_SCREEN_W - 78
    mov edx, 6
    mov eax, 0xFFF8FAFC
    lea rsi, [r15 + str_md_clock - kmain]
    call r2d_draw_text_ui_aa

    pop r12
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
; md_render_dock: Dynamic Floating Bottom Dock with Hover Tooltip
; Automatically sizes width to N installed apps (N * 80), centers on screen,
; renders 44x44 icons, active dot indicators, and floating signature tooltips.
; ==============================================================================
md_render_dock:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14

    ; 1. Calculate installed applications count
    call app_reg_count_installed      ; EAX = N
    cmp eax, 1
    jge @f
    mov eax, 1
@@:
    cmp eax, 8
    jle @f
    mov eax, 8
@@:
    mov [r15 + md_dock_count - kmain], eax
    mov r12d, eax                     ; r12d = N

    ; 2. Compute dynamic geometry
    imul eax, 80                      ; W = N * 80
    mov [r15 + md_dock_w - kmain], eax
    mov r14d, eax                     ; r14d = W

    mov ecx, 1920
    sub ecx, eax
    shr ecx, 1                        ; X = (1920 - W) / 2
    mov [r15 + md_dock_x - kmain], ecx
    mov r13d, ecx                     ; r13d = X

    ; 3. Draw Dock drop shadow
    mov ecx, r13d
    mov edx, 996
    mov esi, r14d
    mov edi, 68
    mov ebx, 18
    call r2d_draw_shadow

    ; 4. Draw Dock frosted glass rounded container
    mov ecx, r13d
    mov edx, 996
    mov esi, r14d
    mov edi, 68
    mov ebx, 18
    mov eax, 0xD80F172A               ; Translucent glass
    call r2d_draw_rounded_rect_aa

    ; 5. Dock top rim frosted glass highlight
    mov ecx, r13d
    add ecx, 18
    mov edx, 996
    mov esi, r14d
    sub esi, 36
    mov edi, 1
    mov eax, 0x38FFFFFF
    call r2d_fill_rect_alpha

    ; 6. Render installed icons and active dot indicators
    xor ebp, ebp                      ; ebp = index (0..N-1)
.dock_icon_loop:
    cmp ebp, r12d
    jae .dock_icons_done

    mov ebx, ebp
    call app_reg_get_nth_installed    ; RDI = reg entry, EAX = app_id
    test rdi, rdi
    jz .next_dock_icon

    ; Draw 44x44 icon
    mov ecx, ebp
    imul ecx, 80
    add ecx, r13d
    add ecx, 18                       ; icon_x = X + 18 + k*80
    mov edx, 996 + 12                 ; icon_y = 1008
    mov rsi, [rdi + REG_APP_ICON_PTR]
    test rsi, rsi
    jz .check_dot
    call r2d_draw_icon_44

.check_dot:
    ; Active dot indicator if app window is open
    movzx eax, byte [rdi + REG_APP_ID]
    cmp eax, APP_COUNT
    jae .next_dock_icon
    cmp byte [r15 + wm_state - kmain + rax], WM_STATE_CLOSED
    je .next_dock_icon

    mov ecx, ebp
    imul ecx, 80
    add ecx, r13d
    add ecx, 38                       ; dot_x = X + 38 + k*80
    mov edx, 996 + 58                 ; dot_y = 1054
    mov esi, 4
    mov edi, 4
    mov ebx, 2
    mov eax, 0xFFF8FAFC               ; Pure white dot
    call r2d_draw_rounded_rect_aa

.next_dock_icon:
    inc ebp
    jmp .dock_icon_loop

.dock_icons_done:
    ; 7. Floating Hover Tooltip (App Name, Version, Digital Signature/Author)
    mov eax, [r15 + md_mouse_x - kmain]
    mov edx, [r15 + md_mouse_y - kmain]

    cmp edx, 996
    jb .dock_done
    cmp edx, 1064
    ja .dock_done
    cmp eax, r13d
    jb .dock_done
    mov ecx, r13d
    add ecx, r14d
    cmp eax, ecx
    jae .dock_done

    ; Mouse is hovering inside the dock! Calculate hovered slot index
    sub eax, r13d
    xor edx, edx
    mov ecx, 80
    div ecx                           ; EAX = hovered slot (0..N-1)
    cmp eax, r12d
    jae .dock_done

    mov ebx, eax                      ; EBX = slot
    call app_reg_get_nth_installed    ; RDI = reg entry
    test rdi, rdi
    jz .dock_done

    push rdi                          ; [rsp] = save valid registry entry pointer!

    ; Compute tooltip coordinates
    ; Center tooltip above hovered icon:
    mov ecx, ebx
    imul ecx, 80
    add ecx, r13d
    add ecx, 40                       ; icon center X
    sub ecx, 120                      ; tip_x = center - (240 / 2)

    ; Clamp tooltip within screen bounds
    cmp ecx, 20
    jge @f
    mov ecx, 20
@@:
    cmp ecx, 1920 - 240 - 20
    jle @f
    mov ecx, 1920 - 240 - 20
@@:
    mov edx, 996 - 42 - 8             ; tip_y = 946
    mov r8d, ecx                      ; r8d = tip_x
    mov r9d, edx                      ; r9d = tip_y

    ; Tooltip Shadow
    mov esi, 240
    mov edi, 42
    mov ebx, 8
    call r2d_draw_shadow

    ; Tooltip Frosted Glass Body
    mov ecx, r8d
    mov edx, r9d
    mov esi, 240
    mov edi, 42
    mov ebx, 8
    mov eax, 0xEB0F172A               ; Acrylic Dark Glass
    call r2d_draw_rounded_rect_aa

    ; Tooltip Top Rim Highlight
    mov ecx, r8d
    add ecx, 8
    mov edx, r9d
    mov esi, 240 - 16
    mov edi, 1
    mov eax, 0x38FFFFFF
    call r2d_fill_rect_alpha

    ; Restore registry entry pointer from stack
    pop rdi

    ; Build Line 1: "<Name> v<Version>"
    push rdi
    lea rsi, [rdi + REG_APP_NAME]
    lea rdi, [r15 + str_tip_buf - kmain]
    cld
.copy_tip_name:
    lodsb
    stosb
    test al, al
    jz .tip_append_v
    jmp .copy_tip_name
.tip_append_v:
    dec rdi                           ; overwrite null
    mov byte [rdi], ' '
    inc rdi
    mov byte [rdi], 'v'
    inc rdi
    mov rdx, [rsp]                    ; rdx = reg entry
    lea rsi, [rdx + REG_APP_VER]
.copy_tip_ver:
    lodsb
    stosb
    test al, al
    jz .tip_l1_done
    jmp .copy_tip_ver
.tip_l1_done:
    pop rdi

    ; Draw Line 1 (Name & Version)
    mov ecx, r8d
    add ecx, 12
    mov edx, r9d
    add edx, 6
    mov eax, 0xFFF8FAFC               ; Bright White
    lea rsi, [r15 + str_tip_buf - kmain]
    call r2d_draw_text_ui_aa

    ; Draw Line 2 (Author / Signature)
    mov ecx, r8d
    add ecx, 12
    mov edx, r9d
    add edx, 23
    mov eax, 0xFF38BDF8               ; Sky Blue Accent
    lea rsi, [rdi + REG_APP_AUTH]
    call r2d_draw_text_ui_aa

.dock_done:
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
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
