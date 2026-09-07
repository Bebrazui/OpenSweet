; ==============================================================================
; Opensweet OS - Modular Window Manager & Compositor (wm.inc)
; Multi-Window Management, Real Lifecycle (Open, Close via Red Button, Focus),
; Z-Ordering, Smooth Zero-Lag Dragging, and Interactive Dock Integration.
; ==============================================================================

; Application IDs
APP_TERM     = 0   ; Interactive Shell Terminal
APP_FILES    = 1   ; ext4 File Explorer
APP_SYS      = 2   ; System Monitor
APP_SETTINGS = 3   ; Settings & Personalization
APP_COUNT    = 4

; Window Lifecycle States
WM_STATE_CLOSED = 0
WM_STATE_OPEN   = 1

align 16
; Application Descriptors & Geometry
wm_state      db 1, 0, 0, 0                     ; Term open by default, others closed
wm_x          dd 120, 760, 1260, 520
wm_y          dd 80,  120, 160,  240
wm_w          dd 680, 620, 480,  560
wm_h          dd 600, 560, 480,  440

; Global Compositor / WM State
wm_z_order    db 0, 1, 2, 3                     ; App IDs from back (0) to front (open_count - 1)
wm_open_count db 1                              ; Number of active open windows
wm_focus_app  db 0                              ; Currently focused application ID
wm_drag_app   db 0xFF                           ; 0xFF = no window being dragged
wm_drag_off_x dd 0
wm_drag_off_y dd 0
wm_prev_lmb   db 0

; Window Titles
str_title_term db "Terminal - Opensweet Shell (x86_64)", 0
str_title_file db "File Explorer - ext4 Storage (/)", 0
str_title_sys  db "System Monitor", 0
str_title_set  db "Settings & Personalization", 0

; ==============================================================================
; wm_init: Initialize Window Manager default state
; ==============================================================================
wm_init:
    push rax

    ; Set initial states
    mov byte [r15 + wm_state - kmain + 0], 1     ; Term OPEN
    mov byte [r15 + wm_state - kmain + 1], 0     ; Files CLOSED
    mov byte [r15 + wm_state - kmain + 2], 0     ; Sys CLOSED
    mov byte [r15 + wm_state - kmain + 3], 0     ; Settings CLOSED

    mov dword [r15 + wm_x - kmain + 0*4], 120
    mov dword [r15 + wm_y - kmain + 0*4], 80
    mov dword [r15 + wm_w - kmain + 0*4], 680
    mov dword [r15 + wm_h - kmain + 0*4], 600

    mov dword [r15 + wm_x - kmain + 1*4], 760
    mov dword [r15 + wm_y - kmain + 1*4], 120
    mov dword [r15 + wm_w - kmain + 1*4], 620
    mov dword [r15 + wm_h - kmain + 1*4], 560

    mov dword [r15 + wm_x - kmain + 2*4], 1260
    mov dword [r15 + wm_y - kmain + 2*4], 160
    mov dword [r15 + wm_w - kmain + 2*4], 480
    mov dword [r15 + wm_h - kmain + 2*4], 480

    mov dword [r15 + wm_x - kmain + 3*4], 520
    mov dword [r15 + wm_y - kmain + 3*4], 240
    mov dword [r15 + wm_w - kmain + 3*4], 560
    mov dword [r15 + wm_h - kmain + 3*4], 440

    mov byte [r15 + wm_z_order - kmain + 0], APP_TERM
    mov byte [r15 + wm_open_count - kmain], 1
    mov byte [r15 + wm_focus_app - kmain], APP_TERM
    mov byte [r15 + wm_drag_app - kmain], 0xFF
    mov byte [r15 + wm_prev_lmb - kmain], 0

    pop rax
    ret

; ==============================================================================
; wm_open_app: Open application window AL
; If closed: marks open, adds to top of z-order, sets focus
; If open: brings to top of z-order
; ==============================================================================
wm_open_app:
    push rax
    push rbx
    push rcx
    push rdx

    movzx ebx, al                       ; app_id
    cmp ebx, APP_COUNT
    jae .open_ret

    ; If already open, bring to front
    cmp byte [r15 + wm_state - kmain + rbx], WM_STATE_OPEN
    je .already_open

    ; Mark open
    mov byte [r15 + wm_state - kmain + rbx], WM_STATE_OPEN

    ; Append to z-order at open_count
    movzx ecx, byte [r15 + wm_open_count - kmain]
    cmp ecx, APP_COUNT
    jae .open_ret

    mov [r15 + wm_z_order - kmain + rcx], bl
    inc byte [r15 + wm_open_count - kmain]
    mov [r15 + wm_focus_app - kmain], bl
    mov byte [r15 + md_term_dirty - kmain], 1
    jmp .open_ret

.already_open:
    mov al, bl
    call wm_bring_to_front

.open_ret:
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; ==============================================================================
; wm_close_app: Cleanly close application window AL
; Removes from z-order, passes focus to remaining top window, clears drag
; ==============================================================================
wm_close_app:
    push rax
    push rbx
    push rcx
    push rdx

    movzx ebx, al                       ; app_id
    cmp ebx, APP_COUNT
    jae .close_ret

    cmp byte [r15 + wm_state - kmain + rbx], WM_STATE_CLOSED
    je .close_ret

    ; Mark closed
    mov byte [r15 + wm_state - kmain + rbx], WM_STATE_CLOSED

    ; Clear drag if this window was being dragged
    cmp [r15 + wm_drag_app - kmain], bl
    jne @f
    mov byte [r15 + wm_drag_app - kmain], 0xFF
@@:
    ; Find in wm_z_order
    xor ecx, ecx
    movzx edx, byte [r15 + wm_open_count - kmain]
.find_loop:
    cmp ecx, edx
    jae .close_done
    cmp [r15 + wm_z_order - kmain + rcx], bl
    je .found_in_z
    inc ecx
    jmp .find_loop

.found_in_z:
    ; Shift remaining elements down: z[i] = z[i+1]
    mov eax, ecx
.shift_loop:
    inc eax
    cmp eax, edx
    jae .shifted
    mov r8b, [r15 + wm_z_order - kmain + rax]
    mov [r15 + wm_z_order - kmain + rax - 1], r8b
    jmp .shift_loop

.shifted:
    dec byte [r15 + wm_open_count - kmain]
    movzx edx, byte [r15 + wm_open_count - kmain]
    test edx, edx
    jz .no_focus
    ; New top window receives focus
    mov al, [r15 + wm_z_order - kmain + rdx - 1]
    mov [r15 + wm_focus_app - kmain], al
    jmp .close_done

.no_focus:
    mov byte [r15 + wm_focus_app - kmain], 0xFF

.close_done:
    mov byte [r15 + md_term_dirty - kmain], 1

.close_ret:
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; ==============================================================================
; wm_bring_to_front: Bring application window AL to top of z-order
; ==============================================================================
wm_bring_to_front:
    push rax
    push rbx
    push rcx
    push rdx

    movzx ebx, al                       ; app_id
    cmp ebx, APP_COUNT
    jae .btf_ret

    movzx edx, byte [r15 + wm_open_count - kmain]
    cmp edx, 1
    jle .btf_focus_only                 ; 0 or 1 window, nothing to reorder

    ; If already top, nothing to shift
    cmp [r15 + wm_z_order - kmain + rdx - 1], bl
    je .btf_focus_only

    ; Find app_id in z-order
    xor ecx, ecx
.btf_find:
    cmp ecx, edx
    jae .btf_ret
    cmp [r15 + wm_z_order - kmain + rcx], bl
    je .btf_found
    inc ecx
    jmp .btf_find

.btf_found:
    ; Shift elements from ecx to edx-2 down
    mov eax, ecx
.btf_shift:
    inc eax
    cmp eax, edx
    jae .btf_put_top
    mov r8b, [r15 + wm_z_order - kmain + rax]
    mov [r15 + wm_z_order - kmain + rax - 1], r8b
    jmp .btf_shift

.btf_put_top:
    mov [r15 + wm_z_order - kmain + rdx - 1], bl
    mov byte [r15 + md_term_dirty - kmain], 1

.btf_focus_only:
    mov [r15 + wm_focus_app - kmain], bl

.btf_ret:
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; ==============================================================================
; wm_toggle_dock: Dock icon AL clicked
; ==============================================================================
wm_toggle_dock:
    push rax
    push rbx

    movzx ebx, al
    cmp ebx, APP_COUNT
    jae .td_ret

    cmp byte [r15 + wm_state - kmain + rbx], WM_STATE_CLOSED
    je .td_open

    ; Already open: bring to front
    mov al, bl
    call wm_bring_to_front
    jmp .td_ret

.td_open:
    mov al, bl
    call wm_open_app

.td_ret:
    pop rbx
    pop rax
    ret

; ==============================================================================
; wm_render_window: Render window frame, title bar, shadow, and content for app EBX
; EBX = app_id, EAX = is_focused
; ==============================================================================
wm_render_window:
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

    mov r12d, eax                       ; is_focused
    mov r13d, ebx                       ; app_id (preserved in r13 across calls!)

    ; Load geometry
    mov r8d, [r15 + wm_x - kmain + r13*4]  ; wx
    mov r9d, [r15 + wm_y - kmain + r13*4]  ; wy
    mov r10d, [r15 + wm_w - kmain + r13*4] ; ww
    mov r11d, [r15 + wm_h - kmain + r13*4] ; wh

    ; 1. Soft Ambient Drop Shadow
    mov ecx, r8d
    mov edx, r9d
    mov esi, r10d
    mov edi, r11d
    mov ebx, 14
    call r2d_draw_shadow

    ; 2. Window Card Background (Opaque Dark Slate 900)
    mov ecx, r8d
    mov edx, r9d
    mov esi, r10d
    mov edi, r11d
    mov ebx, 14
    mov eax, 0xFF0F172A
    call r2d_draw_rounded_rect_aa

    ; 2b. Acrylic Glass Top Edge Sheen (1px)
    mov ecx, r8d
    add ecx, 14
    mov edx, r9d
    mov esi, r10d
    sub esi, 28
    mov edi, 1
    mov eax, 0x38FFFFFF
    call r2d_fill_rect_alpha

    ; 2c. Title Bar Subtle Separator (1px)
    mov ecx, r8d
    mov edx, r9d
    add edx, 29
    mov esi, r10d
    mov edi, 1
    mov eax, 0x18FFFFFF
    call r2d_fill_rect_alpha

    ; 3. Traffic Lights (Close = Red, Minimize = Amber, Maximize = Emerald)
    mov ecx, r8d
    add ecx, 14
    mov edx, r9d
    add edx, 9
    call r2d_draw_traffic_lights

    ; 4. Window Title Text
    mov ecx, r8d
    add ecx, 130
    mov edx, r9d
    add edx, 8

    test r12d, r12d
    jz .title_unfocused
    mov eax, 0xFFF8FAFC                 ; Bright White when focused
    jmp .draw_title
.title_unfocused:
    mov eax, 0xFF94A3B8                 ; Slate 400 when unfocused
.draw_title:
    ; Choose title string by app_id (r13d)
    cmp r13d, APP_TERM
    je .t_term
    cmp r13d, APP_FILES
    je .t_files
    cmp r13d, APP_SYS
    je .t_sys
    lea rsi, [r15 + str_title_set - kmain]
    jmp .do_title_draw
.t_term:
    lea rsi, [r15 + str_title_term - kmain]
    jmp .do_title_draw
.t_files:
    lea rsi, [r15 + str_title_file - kmain]
    jmp .do_title_draw
.t_sys:
    lea rsi, [r15 + str_title_sys - kmain]
.do_title_draw:
    mov edx, r9d
    add edx, 8
    call r2d_draw_text_ui_aa

    ; 5. Title Bar 1px Separator
    mov ecx, r8d
    mov edx, r9d
    add edx, 32
    mov esi, r10d
    mov edi, 1
    mov eax, 0x40FFFFFF
    call r2d_fill_rect_alpha

    ; 6. Dispatch Content Render by app_id (r13d)
    mov ecx, r8d                        ; wx
    mov edx, r9d                        ; wy
    mov esi, r10d                       ; ww
    mov edi, r11d                       ; wh
    mov eax, r12d                       ; is_focused

    cmp r13d, APP_TERM
    je .call_term
    cmp r13d, APP_FILES
    je .call_files
    cmp r13d, APP_SYS
    je .call_sys
    call app_settings_render
    jmp .win_rendered

.call_term:
    call app_term_render
    jmp .win_rendered
.call_files:
    call app_files_render
    jmp .win_rendered
.call_sys:
    call app_sys_render

.win_rendered:
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
; wm_render_all: Render all open windows from back to front of z-order
; ==============================================================================
wm_render_all:
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

    xor r12d, r12d                      ; z index 0..open_count-1
.render_loop:
    cmp r12b, [r15 + wm_open_count - kmain]
    jae .all_done

    movzx ebx, byte [r15 + wm_z_order - kmain + r12] ; app_id

    ; Check if this window is currently focused
    xor eax, eax
    cmp bl, [r15 + wm_focus_app - kmain]
    sete al                             ; eax = 1 if focused, 0 otherwise

    call wm_render_window

    inc r12d
    jmp .render_loop

.all_done:
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
; wm_handle_mouse: Comprehensive mouse dispatcher
; Handles Dock launch/switch, window focus, red close button, title bar drag,
; and microsecond fast cursor path.
; ==============================================================================
wm_handle_mouse:
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

    ; 1. Bounds-clamp mouse coordinates
    mov eax, [r15 + mouse_x - kmain]
    cmp eax, R2D_SCREEN_W - CURSOR_AA_W
    jbe @f
    mov eax, R2D_SCREEN_W - CURSOR_AA_W
@@: mov [r15 + md_mouse_x - kmain], eax

    mov edx, [r15 + mouse_y - kmain]
    cmp edx, R2D_SCREEN_H - CURSOR_AA_H
    jbe @f
    mov edx, R2D_SCREEN_H - CURSOR_AA_H
@@: mov [r15 + md_mouse_y - kmain], edx

    ; 2. Check mouse button transitions
    movzx ebx, byte [r15 + mouse_buttons - kmain]
    and ebx, 1                          ; ebx = current LMB state (0 or 1)
    movzx ecx, byte [r15 + wm_prev_lmb - kmain] ; ecx = previous LMB state
    mov [r15 + wm_prev_lmb - kmain], bl

    ; Test transitions:
    ; ebx=1, ecx=0 -> Click Down
    ; ebx=1, ecx=1 -> Held Down (Drag)
    ; ebx=0, ecx=1 -> Release
    ; ebx=0, ecx=0 -> Hover Move (Fast Cursor)
    test ebx, ebx
    jnz .lmb_is_down

    ; LMB is released (0)
    test ecx, ecx
    jz .just_hover

    ; Transition: Click Released
    mov byte [r15 + wm_drag_app - kmain], 0xFF
    jmp .mouse_ret

.just_hover:
    ; Cursor move only: ultra-fast microsecond LFB blit (<1 us)
    mov eax, [r15 + md_mouse_x - kmain]
    cmp eax, [r15 + md_mouse_prev_x - kmain]
    jne .do_fast_cursor
    mov edx, [r15 + md_mouse_y - kmain]
    cmp edx, [r15 + md_mouse_prev_y - kmain]
    jne .do_fast_cursor
    jmp .mouse_ret

.do_fast_cursor:
    ; Restore previous 18x18 cursor area from backbuffer to VRAM LFB
    mov ecx, [r15 + md_mouse_prev_x - kmain]
    mov edx, [r15 + md_mouse_prev_y - kmain]
    call r2d_restore_cursor_lfb

    ; Update previous position
    mov eax, [r15 + md_mouse_x - kmain]
    mov [r15 + md_mouse_prev_x - kmain], eax
    mov edx, [r15 + md_mouse_y - kmain]
    mov [r15 + md_mouse_prev_y - kmain], edx

    ; Draw cursor directly to VRAM LFB
    mov ecx, eax
    call r2d_draw_cursor_lfb
    jmp .mouse_ret

.lmb_is_down:
    test ecx, ecx
    jnz .lmb_is_held

    ; --- TRANSITION: LMB CLICK DOWN ---
    ; Test Dock Region: y in [996, 1064], x in [800, 1120]
    mov eax, [r15 + md_mouse_x - kmain]
    mov edx, [r15 + md_mouse_y - kmain]

    cmp edx, 996
    jb .test_windows
    cmp edx, 1064
    ja .test_windows
    cmp eax, 800
    jb .test_windows
    cmp eax, 1120
    ja .test_windows

    ; Click is inside Dock! Determine icon:
    ; Icon 0 (Terminal): 800..879
    ; Icon 1 (Files):    880..959
    ; Icon 2 (System):   960..1039
    ; Icon 3 (Settings): 1040..1120
    cmp eax, 880
    jae @f
    mov al, APP_TERM
    call wm_toggle_dock
    jmp .mouse_ret
@@:
    cmp eax, 960
    jae @f
    mov al, APP_FILES
    call wm_toggle_dock
    jmp .mouse_ret
@@:
    cmp eax, 1040
    jae @f
    mov al, APP_SYS
    call wm_toggle_dock
    jmp .mouse_ret
@@:
    mov al, APP_SETTINGS
    call wm_toggle_dock
    jmp .mouse_ret

.test_windows:
    ; Hit test windows in reverse z-order (top to bottom)
    movzx r12d, byte [r15 + wm_open_count - kmain]
.win_hit_loop:
    test r12d, r12d
    jz .mouse_ret
    dec r12d

    movzx ebx, byte [r15 + wm_z_order - kmain + r12] ; app_id

    mov r8d, [r15 + wm_x - kmain + rbx*4]            ; wx
    mov r9d, [r15 + wm_y - kmain + rbx*4]            ; wy
    mov r10d, [r15 + wm_w - kmain + rbx*4]           ; ww
    mov r11d, [r15 + wm_h - kmain + rbx*4]           ; wh

    ; Check if mouse (eax, edx) is inside [wx, wy, wx+ww, wy+wh]
    mov eax, [r15 + md_mouse_x - kmain]
    mov edx, [r15 + md_mouse_y - kmain]

    cmp eax, r8d
    jb .win_hit_loop
    mov ecx, r8d
    add ecx, r10d
    cmp eax, ecx
    ja .win_hit_loop

    cmp edx, r9d
    jb .win_hit_loop
    mov ecx, r9d
    add ecx, r11d
    cmp edx, ecx
    ja .win_hit_loop

    ; HIT WINDOW ebx!
    ; Check 1: Traffic Light Red Close Button
    ; Button center at (wx + 19, wy + 16), radius 5
    ; Box: x in [wx + 9, wx + 26], y in [wy + 6, wy + 26]
    mov ecx, r8d
    add ecx, 9
    cmp eax, ecx
    jb .not_close_btn
    add ecx, 17
    cmp eax, ecx
    ja .not_close_btn

    mov ecx, r9d
    add ecx, 6
    cmp edx, ecx
    jb .not_close_btn
    add ecx, 20
    cmp edx, ecx
    ja .not_close_btn

    ; Red close button CLICKED! Cleanly close app!
    mov al, bl
    call wm_close_app
    jmp .mouse_ret

.not_close_btn:
    ; Check 2: Title bar drag region (wy to wy + 32)
    mov ecx, r9d
    add ecx, 32
    cmp edx, ecx
    ja .body_click

    ; Title bar clicked: Bring to front and start dragging
    mov al, bl
    call wm_bring_to_front

    mov [r15 + wm_drag_app - kmain], bl
    mov eax, [r15 + md_mouse_x - kmain]
    sub eax, r8d
    mov [r15 + wm_drag_off_x - kmain], eax
    mov edx, [r15 + md_mouse_y - kmain]
    sub edx, r9d
    mov [r15 + wm_drag_off_y - kmain], edx
    jmp .mouse_ret

.body_click:
    ; Window body clicked: Bring to front and pass click to app
    mov al, bl
    call wm_bring_to_front

    cmp ebx, APP_FILES
    jne .mouse_ret
    ; Dispatch to File Explorer
    mov ecx, [r15 + md_mouse_x - kmain]
    mov edx, [r15 + md_mouse_y - kmain]
    call app_files_click
    test al, al
    jz .mouse_ret
    mov byte [r15 + md_term_dirty - kmain], 1
    jmp .mouse_ret

.lmb_is_held:
    ; --- LMB HELD: DRAGGING ---
    movzx ebx, byte [r15 + wm_drag_app - kmain]
    cmp ebx, APP_COUNT
    jae .mouse_ret

    ; Compute new window coordinates
    mov eax, [r15 + md_mouse_x - kmain]
    sub eax, [r15 + wm_drag_off_x - kmain]
    ; Clamp X between 10 and SCREEN_W - w - 10
    cmp eax, 10
    jge @f
    mov eax, 10
@@:
    mov ecx, R2D_SCREEN_W - 10
    sub ecx, [r15 + wm_w - kmain + rbx*4]
    cmp eax, ecx
    jle @f
    mov eax, ecx
@@:
    mov [r15 + wm_x - kmain + rbx*4], eax

    mov edx, [r15 + md_mouse_y - kmain]
    sub edx, [r15 + wm_drag_off_y - kmain]
    ; Clamp Y between 32 (below top bar) and SCREEN_H - h - 10
    cmp edx, 32
    jge @f
    mov edx, 32
@@:
    mov ecx, R2D_SCREEN_H - 10
    sub ecx, [r15 + wm_h - kmain + rbx*4]
    cmp edx, ecx
    jle @f
    mov edx, ecx
@@:
    mov [r15 + wm_y - kmain + rbx*4], edx

    ; Mark dirty to re-render composited desktop
    mov byte [r15 + md_term_dirty - kmain], 1

.mouse_ret:
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
