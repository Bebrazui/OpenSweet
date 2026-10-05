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
APP_USER_0   = 4   ; User Application 0
APP_USER_1   = 5   ; User Application 1
APP_USER_2   = 6   ; User Application 2
APP_USER_3   = 7   ; User Application 3
APP_COUNT    = 8

; Window Lifecycle States
WM_STATE_CLOSED    = 0
WM_STATE_OPEN      = 1
WM_STATE_MINIMIZED = 2

align 16
; Application Descriptors & Geometry (8 windows: 4 system + 4 dynamic user apps)
wm_state      db 1, 0, 0, 0, 0, 0, 0, 0         ; Term open by default, others closed
wm_x          dd 120, 760, 1260, 520, 240, 300, 360, 420
wm_y          dd 80,  120, 160,  240, 140, 180, 220, 260
wm_w          dd 680, 620, 480,  560, 500, 500, 500, 500
wm_h          dd 600, 560, 480,  440, 380, 380, 380, 380
wm_maximized  db 0, 0, 0, 0, 0, 0, 0, 0
wm_saved_x    dd 0, 0, 0, 0, 0, 0, 0, 0
wm_saved_y    dd 0, 0, 0, 0, 0, 0, 0, 0
wm_saved_w    dd 0, 0, 0, 0, 0, 0, 0, 0
wm_saved_h    dd 0, 0, 0, 0, 0, 0, 0, 0

; Global Compositor / WM State
wm_z_order    db 0, 1, 2, 3, 4, 5, 6, 7         ; App IDs from back (0) to front (open_count - 1)
wm_open_count db 1                              ; Number of active open windows
wm_focus_app  db 0                              ; Currently focused application ID
wm_drag_app        db 0xFF                           ; 0xFF = no window being dragged
wm_drag_off_x      dd 0
wm_drag_off_y      dd 0
wm_drag_prev_x     dd 0
wm_drag_prev_y     dd 0
wm_prev_lmb        db 0
wm_last_drag_tick  dd 0
wm_drag_dirty_x    dd 0
wm_drag_dirty_y    dd 0
wm_drag_dirty_w    dd 0
wm_drag_dirty_h    dd 0

; Dynamic User Window Storage (8 slots: 0..7)
align 16
wm_user_canvas    dq 0x00800000, 0x00A00000, 0x00C00000, 0x00E00000, 0x01000000, 0x01200000, 0x01400000, 0x01600000
wm_user_titles    rb 8 * 64
wm_user_ev_head   db 0, 0, 0, 0, 0, 0, 0, 0
wm_user_ev_tail   db 0, 0, 0, 0, 0, 0, 0, 0
align 16
wm_user_events    rb 8 * 16 * 16       ; 8 windows * 16 events * 16 bytes = 2048 bytes

; Window Titles
str_title_term db "Terminal - Opensweet Shell (x86_64)", 0
str_title_file db "File Explorer - ext4 Storage (/)", 0
str_title_sys  db "System Monitor", 0
str_title_set  db "Settings & Personalization", 0
str_wm_drag_log   db "[WM] drag_step x=", 0
str_wm_y_log      db " y=", 0
str_wm_w_log      db " w=", 0
str_wm_h_log      db " h=", 0
str_wm_cyc_log    db " cycles=", 0
str_wm_flip_cyc   db " flip_cyc=", 0
str_wm_touch_log  db "[WM] touch_down app=", 0
str_wm_rel_log    db "[WM] lmb_release", 10, 0


; ==============================================================================
; wm_init: Initialize Window Manager default state
; ==============================================================================
wm_init:
    push rax

    ; Set initial states (all closed initially, terminal.elf launched dynamically)
    mov byte [r15 + wm_state - kmain + 0], 0     ; Term CLOSED
    mov byte [r15 + wm_state - kmain + 1], 0     ; Files CLOSED
    mov byte [r15 + wm_state - kmain + 2], 0     ; Sys CLOSED
    mov byte [r15 + wm_state - kmain + 3], 0     ; Settings CLOSED
    mov byte [r15 + wm_state - kmain + 4], 0     ; User 0 CLOSED
    mov byte [r15 + wm_state - kmain + 5], 0     ; User 1 CLOSED
    mov byte [r15 + wm_state - kmain + 6], 0     ; User 2 CLOSED
    mov byte [r15 + wm_state - kmain + 7], 0     ; User 3 CLOSED
    mov qword [r15 + wm_user_ev_head - kmain], 0
    mov qword [r15 + wm_user_ev_tail - kmain], 0

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

    mov byte [r15 + wm_open_count - kmain], 0
    mov byte [r15 + wm_focus_app - kmain], 0xFF
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
; wm_minimize_app: Minimize application window AL (hide from screen, keep state)
; ==============================================================================
wm_minimize_app:
    push rax
    push rbx
    push rcx
    push rdx
    push r8

    movzx ebx, al
    cmp ebx, APP_COUNT
    jae .min_ret

    cmp byte [r15 + wm_state - kmain + rbx], WM_STATE_OPEN
    jne .min_ret

    ; Mark minimized
    mov byte [r15 + wm_state - kmain + rbx], WM_STATE_MINIMIZED

    ; Clear drag if this window was being dragged
    cmp [r15 + wm_drag_app - kmain], bl
    jne @f
    mov byte [r15 + wm_drag_app - kmain], 0xFF
@@:
    ; Find in wm_z_order
    xor ecx, ecx
    movzx edx, byte [r15 + wm_open_count - kmain]
.min_find_loop:
    cmp ecx, edx
    jae .min_done
    cmp [r15 + wm_z_order - kmain + rcx], bl
    je .min_found_in_z
    inc ecx
    jmp .min_find_loop

.min_found_in_z:
    ; Shift remaining elements down: z[i] = z[i+1]
    mov eax, ecx
.min_shift_loop:
    inc eax
    cmp eax, edx
    jae .min_shifted
    mov r8b, [r15 + wm_z_order - kmain + rax]
    mov [r15 + wm_z_order - kmain + rax - 1], r8b
    jmp .min_shift_loop

.min_shifted:
    dec byte [r15 + wm_open_count - kmain]
    movzx edx, byte [r15 + wm_open_count - kmain]
    test edx, edx
    jz .min_no_focus
    ; New top window receives focus
    mov al, [r15 + wm_z_order - kmain + rdx - 1]
    mov [r15 + wm_focus_app - kmain], al
    jmp .min_done

.min_no_focus:
    mov byte [r15 + wm_focus_app - kmain], 0xFF

.min_done:
    ; Post EVENT_WIN_MINIMIZE = 7
    movzx ebx, bl
    mov eax, 7
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    call wm_post_event

    mov byte [r15 + md_term_dirty - kmain], 1

.min_ret:
    pop r8
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; ==============================================================================
; wm_toggle_maximize_app: Toggle maximize / zoom for window AL
; ==============================================================================
wm_toggle_maximize_app:
    push rax
    push rbx
    push rcx
    push rdx

    movzx ebx, al
    cmp ebx, APP_COUNT
    jae .max_ret

    cmp byte [r15 + wm_state - kmain + rbx], WM_STATE_OPEN
    jne .max_ret

    cmp byte [r15 + wm_maximized - kmain + rbx], 0
    jne .restore_from_max

    ; 1. Maximize window:
    ; Save current geometry
    mov eax, [r15 + wm_x - kmain + rbx*4]
    mov [r15 + wm_saved_x - kmain + rbx*4], eax
    mov eax, [r15 + wm_y - kmain + rbx*4]
    mov [r15 + wm_saved_y - kmain + rbx*4], eax
    mov eax, [r15 + wm_w - kmain + rbx*4]
    mov [r15 + wm_saved_w - kmain + rbx*4], eax
    mov eax, [r15 + wm_h - kmain + rbx*4]
    mov [r15 + wm_saved_h - kmain + rbx*4], eax

    mov byte [r15 + wm_maximized - kmain + rbx], 1

    ; Large maximized geometry: 960x560, centered at x=480, y=80
    mov dword [r15 + wm_x - kmain + rbx*4], 480
    mov dword [r15 + wm_y - kmain + rbx*4], 80
    mov dword [r15 + wm_w - kmain + rbx*4], 960
    mov dword [r15 + wm_h - kmain + rbx*4], 560
    jmp .max_done

.restore_from_max:
    ; 2. Restore normal geometry:
    mov eax, [r15 + wm_saved_x - kmain + rbx*4]
    mov [r15 + wm_x - kmain + rbx*4], eax
    mov eax, [r15 + wm_saved_y - kmain + rbx*4]
    mov [r15 + wm_y - kmain + rbx*4], eax
    mov eax, [r15 + wm_saved_w - kmain + rbx*4]
    mov [r15 + wm_w - kmain + rbx*4], eax
    mov eax, [r15 + wm_saved_h - kmain + rbx*4]
    mov [r15 + wm_h - kmain + rbx*4], eax

    mov byte [r15 + wm_maximized - kmain + rbx], 0

.max_done:
    ; Bring to front & notify
    mov al, bl
    call wm_bring_to_front

    ; Post EVENT_WIN_MAXIMIZE = 8
    movzx ebx, bl
    mov eax, 8
    mov ecx, [r15 + wm_w - kmain + rbx*4]
    mov edx, [r15 + wm_h - kmain + rbx*4]
    xor r8d, r8d
    call wm_post_event

    mov byte [r15 + md_term_dirty - kmain], 1

.max_ret:
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
    push r8

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
    pop r8
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
    push rsi

    movzx ebx, al
    cmp ebx, APP_COUNT
    jae .td_ret

    cmp byte [r15 + wm_state - kmain + rbx], WM_STATE_MINIMIZED
    je .td_unminimize

    cmp byte [r15 + wm_state - kmain + rbx], WM_STATE_CLOSED
    je .td_open

    ; Already open: if already top-most and focused, toggle minimize!
    cmp bl, [r15 + wm_focus_app - kmain]
    jne .td_bring_front
    mov al, bl
    call wm_minimize_app
    jmp .td_ret

.td_bring_front:
    mov al, bl
    call wm_bring_to_front
    mov byte [r15 + md_term_dirty - kmain], 1
    jmp .td_ret

.td_unminimize:
    ; Restore minimized window
    mov al, bl
    call wm_open_app
    mov byte [r15 + md_term_dirty - kmain], 1
    jmp .td_ret

.td_open:
    ; Check if this app has an executable path on disk (e.g. "/notepad.elf")
    imul eax, ebx, APP_ENTRY_SIZE
    lea rsi, [r15 + app_registry_data - kmain + rax + REG_APP_PATH]
    cmp byte [rsi], 0
    je .td_open_sys

    ; Launch ELF executable into Ring 3!
    call elf_load_from_ext4
    jmp .td_ret

.td_open_sys:
    mov al, bl
    call wm_open_app

.td_ret:
    pop rsi
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
    add ecx, 16                         ; 16px from left edge (generous clearance from squircle corner)
    mov edx, r9d
    add edx, 10                         ; 10px from top edge (perfectly vertically centered in 32px title bar)
    call r2d_draw_traffic_lights

    ; 4. Window Title Text
    mov ecx, r8d
    add ecx, 76                         ; Starts cleanly after traffic lights (wx + 76)
    mov edx, r9d
    add edx, 8

    test r12d, r12d
    jz .title_unfocused
    mov eax, 0xFFF8FAFC                 ; Bright White when focused
    jmp .draw_title
.title_unfocused:
    mov eax, 0xFF94A3B8                 ; Slate 400 when unfocused
.draw_title:
    ; Window Title Text (r13d: 0..7)
    mov edx, r13d
    shl edx, 6                         ; edx = slot * 64
    lea rsi, [r15 + wm_user_titles - kmain + rdx]
    cmp byte [rsi], 0
    jne .do_title_draw
    ; Fallback title
    lea rsi, [r15 + str_title_term - kmain]
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

    ; Blit client canvas for window (r13d: 0..7)!
    ; Client area: (wx, wy + 33, ww, wh - 33)
    add edx, 33                       ; dst_y = wy + 33
    sub edi, 33                       ; h = wh - 33
    mov eax, r13d
    mov r8, [r15 + wm_user_canvas - kmain + rax*8]
    test r8, r8
    jz .win_rendered

    call r2d_blit_bitmap

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

    ; 1. Mouse physics & momentum integration
    mov eax, [r15 + mouse_x - kmain]
    mov edx, [r15 + mouse_y - kmain]

    ; Check if mouse button is down / clicked
    movzx ebx, byte [r15 + mouse_buttons - kmain]
    and ebx, 1
    test ebx, ebx
    jnz .wm_mouse_snap              ; Snapping when clicking ensures pixel-perfect targeting

    ; Delta in 16.16 fixed-point
    shl eax, 16
    sub eax, [r15 + md_mouse_fp_x - kmain]  ; eax = diff_x (16.16)
    shl edx, 16
    sub edx, [r15 + md_mouse_fp_y - kmain]  ; edx = diff_y (16.16)

    ; Calculate abs(diff_x)
    mov r8d, eax
    test r8d, r8d
    jns @f
    neg r8d
@@:
    mov r9d, edx
    test r9d, r9d
    jns @f
    neg r9d
@@:
    cmp r8d, 0x00018000             ; ~1.5 pixels threshold
    ja .wm_mouse_physics
    cmp r9d, 0x00018000
    ja .wm_mouse_physics

.wm_mouse_snap:
    mov eax, [r15 + mouse_x - kmain]
    mov edx, [r15 + mouse_y - kmain]
    mov [r15 + md_mouse_x - kmain], eax
    mov [r15 + md_mouse_y - kmain], edx
    shl eax, 16
    shl edx, 16
    mov [r15 + md_mouse_fp_x - kmain], eax
    mov [r15 + md_mouse_fp_y - kmain], edx
    mov dword [r15 + md_mouse_vx - kmain], 0
    mov dword [r15 + md_mouse_vy - kmain], 0
    jmp .wm_mouse_done

.wm_mouse_physics:
    ; Momentum lerp: Vx = (Vx * 3 + diff_x) / 4
    mov ecx, [r15 + md_mouse_vx - kmain]
    lea ecx, [ecx + ecx*2]
    add ecx, eax
    sar ecx, 2
    mov [r15 + md_mouse_vx - kmain], ecx

    ; Vy = (Vy * 3 + diff_y) / 4
    mov r8d, [r15 + md_mouse_vy - kmain]
    lea r8d, [r8d + r8d*2]
    add r8d, edx
    sar r8d, 2
    mov [r15 + md_mouse_vy - kmain], r8d

    ; Position += Velocity
    mov eax, [r15 + md_mouse_fp_x - kmain]
    add eax, ecx
    mov [r15 + md_mouse_fp_x - kmain], eax
    sar eax, 16

    mov edx, [r15 + md_mouse_fp_y - kmain]
    add edx, r8d
    mov [r15 + md_mouse_fp_y - kmain], edx
    sar edx, 16

    ; Clamp X to [0, R2D_SCREEN_W - 20]
    test eax, eax
    jns @f
    xor eax, eax
@@:
    cmp eax, R2D_SCREEN_W - 20
    jle @f
    mov eax, R2D_SCREEN_W - 20
@@:
    mov [r15 + md_mouse_x - kmain], eax

    ; Clamp Y to [0, R2D_SCREEN_H - 20]
    test edx, edx
    jns @f
    xor edx, edx
@@:
    cmp edx, R2D_SCREEN_H - 20
    jle @f
    mov edx, R2D_SCREEN_H - 20
@@:
    mov [r15 + md_mouse_y - kmain], edx

.wm_mouse_done:

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
    lea rsi, [r15 + str_wm_rel_log - kmain]
    call serial_puts

    movzx ebx, byte [r15 + wm_drag_app - kmain]
    cmp ebx, APP_COUNT
    jae .release_no_drag

    ; Flush any throttled final movement if window ended at different position
    mov eax, [r15 + wm_x - kmain + rbx*4]
    mov edx, [r15 + wm_y - kmain + rbx*4]
    cmp eax, [r15 + wm_drag_prev_x - kmain]
    jne .flush_final_drag
    cmp edx, [r15 + wm_drag_prev_y - kmain]
    je .release_done
.flush_final_drag:
    call wm_drag_step

.release_done:
    mov byte [r15 + wm_drag_app - kmain], 0xFF
    mov byte [r15 + md_term_dirty - kmain], 1
    jmp .mouse_ret

.release_no_drag:
    mov byte [r15 + wm_drag_app - kmain], 0xFF

    ; Post EVENT_MOUSE_UP = 3 to focused window (if any)
    movzx ebx, byte [r15 + wm_focus_app - kmain]
    cmp bl, APP_COUNT
    jae .mouse_ret

    mov ecx, [r15 + md_mouse_x - kmain]
    sub ecx, [r15 + wm_x - kmain + rbx*4]
    mov edx, [r15 + md_mouse_y - kmain]
    sub edx, [r15 + wm_y - kmain + rbx*4]
    sub edx, 33

    mov eax, 3                         ; EVENT_MOUSE_UP = 3
    mov r8d, 1                         ; Button 1 (LMB)
    call wm_post_event
    jmp .mouse_ret

.just_hover:
    ; Cursor move only: ultra-fast microsecond LFB blit (<1 us)
    mov eax, [r15 + md_mouse_x - kmain]
    cmp eax, [r15 + md_mouse_prev_x - kmain]
    jne @f
    mov edx, [r15 + md_mouse_y - kmain]
    cmp edx, [r15 + md_mouse_prev_y - kmain]
    jne @f
    jmp .mouse_ret
@@:
    ; If mouse is in or near dock area (Y >= 940), trigger desktop dirty so tooltip tracks smoothly
    mov edx, [r15 + md_mouse_y - kmain]
    cmp edx, 940
    jae .dock_hover_refresh
    mov edx, [r15 + md_mouse_prev_y - kmain]
    cmp edx, 940
    jae .dock_hover_refresh
    jmp .do_fast_cursor

.dock_hover_refresh:
    mov byte [r15 + md_term_dirty - kmain], 1
    jmp .do_fast_cursor

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
    ; Test Dock Region: y in [996, 1064], x in [md_dock_x, md_dock_x + md_dock_w]
    mov eax, [r15 + md_mouse_x - kmain]
    mov edx, [r15 + md_mouse_y - kmain]

    cmp edx, 996
    jb .test_windows
    cmp edx, 1064
    ja .test_windows
    cmp eax, [r15 + md_dock_x - kmain]
    jb .test_windows
    mov ecx, [r15 + md_dock_x - kmain]
    add ecx, [r15 + md_dock_w - kmain]
    cmp eax, ecx
    jae .test_windows

    ; Click is inside Dock! Determine icon slot:
    sub eax, [r15 + md_dock_x - kmain]
    xor edx, edx
    mov ecx, 80
    div ecx                           ; EAX = clicked slot index (0..dock_count-1)
    cmp eax, [r15 + md_dock_count - kmain]
    jae .mouse_ret

    mov ebx, eax
    call app_reg_get_nth_installed    ; EAX = app_id (0..7)
    cmp eax, -1
    je .mouse_ret

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
    ; Check 1: Traffic Lights (Close = Red, Minimize = Yellow, Maximize = Green)
    mov ecx, r9d
    add ecx, 6
    cmp edx, ecx
    jb .not_traffic_lights
    add ecx, 20
    cmp edx, ecx
    ja .not_traffic_lights

    ; Button 1: RED Close Button (wx + 9 .. wx + 26)
    mov ecx, r8d
    add ecx, 9
    cmp eax, ecx
    jb .not_traffic_lights
    add ecx, 17                         ; wx + 26
    cmp eax, ecx
    jbe .click_red_btn

    ; Button 2: YELLOW / AMBER Minimize Button (wx + 27 .. wx + 44)
    add ecx, 18                         ; wx + 44
    cmp eax, ecx
    jbe .click_yellow_btn

    ; Button 3: GREEN Maximize / Zoom Button (wx + 45 .. wx + 65)
    add ecx, 21                         ; wx + 65
    cmp eax, ecx
    jbe .click_green_btn

    jmp .not_traffic_lights

.click_red_btn:
    ; Red close button CLICKED! Cleanly close app!
    movzx ebx, bl
    mov eax, 5                         ; EVENT_WIN_CLOSE = 5
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    call wm_post_event

    mov al, bl
    call wm_close_app
    jmp .mouse_ret

.click_yellow_btn:
    ; Yellow minimize button CLICKED! Minimize app!
    movzx ebx, bl
    mov al, bl
    call wm_minimize_app
    jmp .mouse_ret

.click_green_btn:
    ; Green maximize button CLICKED! Toggle maximize / zoom!
    movzx ebx, bl
    mov al, bl
    call wm_toggle_maximize_app
    jmp .mouse_ret

.not_traffic_lights:
    ; Check 2: Title bar drag region (wy to wy + 32)
    mov ecx, r9d
    add ecx, 32
    cmp edx, ecx
    ja .body_click

    ; Title bar clicked: Bring to front and start dragging
    lea rsi, [r15 + str_wm_touch_log - kmain]
    call serial_puts
    movzx eax, bl
    call serial_putdec64
    mov al, 10
    call serial_putc

    mov al, bl
    call wm_bring_to_front

    movzx ebx, al
    mov [r15 + wm_drag_app - kmain], bl

    ; Safely reload window coordinates directly from memory
    mov r8d, [r15 + wm_x - kmain + rbx*4]
    mov r9d, [r15 + wm_y - kmain + rbx*4]

    mov eax, [r15 + md_mouse_x - kmain]
    sub eax, r8d
    mov [r15 + wm_drag_off_x - kmain], eax
    mov edx, [r15 + md_mouse_y - kmain]
    sub edx, r9d
    mov [r15 + wm_drag_off_y - kmain], edx

    mov [r15 + wm_drag_prev_x - kmain], r8d
    mov [r15 + wm_drag_prev_y - kmain], r9d

    ; Render focused window directly to backbuffer
    mov eax, 1                                  ; is_focused = 1
    call wm_render_window
    mov byte [r15 + md_term_dirty - kmain], 0   ; Avoid full-screen render stall on drag start!

    ; Timestamp of drag start for frame pacing
    mov eax, [r15 + timer_ticks - kmain]
    mov [r15 + wm_last_drag_tick - kmain], eax

    ; Flip window bounds + shadow margin to screen immediately (<1 ms)
    mov ecx, r8d
    sub ecx, 18
    jns @f
    xor ecx, ecx
@@:
    and ecx, not 1
    mov edx, r9d
    sub edx, 18
    jns @f
    xor edx, edx
@@:
    mov esi, [r15 + wm_w - kmain + rbx*4]
    add esi, 36
    mov edi, [r15 + wm_h - kmain + rbx*4]
    add edi, 36
    call r2d_flip_rect

    ; Restore previous cursor
    mov ecx, [r15 + md_mouse_prev_x - kmain]
    mov edx, [r15 + md_mouse_prev_y - kmain]
    call r2d_restore_cursor_lfb

    ; Draw cursor directly over window
    mov ecx, [r15 + md_mouse_x - kmain]
    mov edx, [r15 + md_mouse_y - kmain]
    mov [r15 + md_mouse_prev_x - kmain], ecx
    mov [r15 + md_mouse_prev_y - kmain], edx
    call r2d_draw_cursor_lfb

    jmp .mouse_ret

.body_click:
    ; Window body clicked: Bring to front and pass click to app
    mov al, bl
    call wm_bring_to_front

    ; Post MOUSE_DOWN event to target window (ebx: 0..7)
    mov ecx, [r15 + md_mouse_x - kmain]
    sub ecx, [r15 + wm_x - kmain + rbx*4]

    mov edx, [r15 + md_mouse_y - kmain]
    sub edx, [r15 + wm_y - kmain + rbx*4]
    sub edx, 33                        ; Title bar height = 33

    mov eax, 2                         ; EVENT_MOUSE_DOWN = 2
    mov r8d, 1                         ; Button = 1 (LMB)
    call wm_post_event
    jmp .mouse_ret

.lmb_is_held:
    ; --- LMB HELD: DRAGGING ---
    movzx ebx, byte [r15 + wm_drag_app - kmain]
    cmp ebx, APP_COUNT
    jae .just_hover

    ; Compute new window coordinates
    mov eax, [r15 + md_mouse_x - kmain]
    sub eax, [r15 + wm_drag_off_x - kmain]
    ; Clamp X between 20 and SCREEN_W - w - 20
    cmp eax, 20
    jge @f
    mov eax, 20
@@:
    mov ecx, R2D_SCREEN_W - 20
    sub ecx, [r15 + wm_w - kmain + rbx*4]
    cmp eax, ecx
    jle @f
    mov eax, ecx
@@:

    mov edx, [r15 + md_mouse_y - kmain]
    sub edx, [r15 + wm_drag_off_y - kmain]
    ; Clamp Y between 32 (below top bar) and SCREEN_H - h - 20
    cmp edx, 32
    jge @f
    mov edx, 32
@@:
    mov ecx, R2D_SCREEN_H - 20
    sub ecx, [r15 + wm_h - kmain + rbx*4]
    cmp edx, ecx
    jle @f
    mov edx, ecx
@@:

    ; Always store latest target coordinates
    mov [r15 + wm_x - kmain + rbx*4], eax
    mov [r15 + wm_y - kmain + rbx*4], edx

    ; Check if window position actually changed compared to last blitted position!
    cmp eax, [r15 + wm_drag_prev_x - kmain]
    jne .check_drag_pacing
    cmp edx, [r15 + wm_drag_prev_y - kmain]
    je .mouse_ret                     ; Position unchanged, return!

.check_drag_pacing:
    ; 60 FPS Frame Pacing: at least 16 ms since last blit (coalesce 1000 Hz packets)
    mov eax, [r15 + timer_ticks - kmain]
    sub eax, [r15 + wm_last_drag_tick - kmain]
    cmp eax, 16
    jb .mouse_ret                     ; < 16 ms: coalesce! Will be flushed on next tick

    mov eax, [r15 + timer_ticks - kmain]
    mov [r15 + wm_last_drag_tick - kmain], eax

    mov eax, [r15 + wm_x - kmain + rbx*4]
    mov edx, [r15 + wm_y - kmain + rbx*4]
    call wm_drag_step
    jmp .mouse_ret

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

; ==============================================================================
; wm_drag_step: Fast sub-rectangle dirty blit for window dragging
; EBX = app_id, EAX = new_x, EDX = new_y
; ==============================================================================
wm_drag_step:
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
    push r14
    push rbp

    mov r12d, eax                     ; r12d = new_x
    mov r13d, edx                     ; r13d = new_y

    rdtsc
    shl rdx, 32
    or rax, rdx
    mov r14, rax                        ; r14 = start cycles

    ; 0. Restore previous cursor from backbuffer before any window movement
    mov ecx, [r15 + md_mouse_prev_x - kmain]
    mov edx, [r15 + md_mouse_prev_y - kmain]
    call r2d_restore_cursor_lfb

    mov r8d, [r15 + wm_drag_prev_x - kmain] ; r8d = old_x
    mov r9d, [r15 + wm_drag_prev_y - kmain] ; r9d = old_y
    mov r10d, [r15 + wm_w - kmain + rbx*4]  ; r10d = win_w
    mov r11d, [r15 + wm_h - kmain + rbx*4]  ; r11d = win_h

    ; X_min = min(old_x, new_x, mouse_x, mouse_prev_x)
    mov ecx, r8d
    cmp ecx, r12d
    jbe .xmin_ok
    mov ecx, r12d
.xmin_ok:
    cmp ecx, [r15 + md_mouse_x - kmain]
    jbe @f
    mov ecx, [r15 + md_mouse_x - kmain]
@@:
    cmp ecx, [r15 + md_mouse_prev_x - kmain]
    jbe @f
    mov ecx, [r15 + md_mouse_prev_x - kmain]
@@:

    ; Y_min = min(old_y, new_y, mouse_y, mouse_prev_y)
    mov edx, r9d
    cmp edx, r13d
    jbe .ymin_ok
    mov edx, r13d
.ymin_ok:
    cmp edx, [r15 + md_mouse_y - kmain]
    jbe @f
    mov edx, [r15 + md_mouse_y - kmain]
@@:
    cmp edx, [r15 + md_mouse_prev_y - kmain]
    jbe @f
    mov edx, [r15 + md_mouse_prev_y - kmain]
@@:

    ; X_max = max(old_x + win_w, new_x + win_w, mouse_x + 20, mouse_prev_x + 20)
    mov esi, r8d
    cmp esi, r12d
    jae .xmax_ok
    mov esi, r12d
.xmax_ok:
    add esi, r10d
    mov eax, [r15 + md_mouse_x - kmain]
    add eax, CURSOR_AA_W
    cmp esi, eax
    jae @f
    mov esi, eax
@@:
    mov eax, [r15 + md_mouse_prev_x - kmain]
    add eax, CURSOR_AA_W
    cmp esi, eax
    jae @f
    mov esi, eax
@@:

    ; Y_max = max(old_y + win_h, new_y + win_h, mouse_y + 20, mouse_prev_y + 20)
    mov edi, r9d
    cmp edi, r13d
    jae .ymax_ok
    mov edi, r13d
.ymax_ok:
    add edi, r11d
    mov eax, [r15 + md_mouse_y - kmain]
    add eax, CURSOR_AA_H
    cmp edi, eax
    jae @f
    mov edi, eax
@@:
    mov eax, [r15 + md_mouse_prev_y - kmain]
    add eax, CURSOR_AA_H
    cmp edi, eax
    jae @f
    mov edi, eax
@@:

    ; Expand bounding box with shadow margin (18 px)
    sub ecx, 18
    jns .cx_ok
    xor ecx, ecx
.cx_ok:
    sub edx, 18
    jns .dx_ok
    xor edx, edx
.dx_ok:
    add esi, 18
    cmp esi, R2D_SCREEN_W
    jbe .si_ok
    mov esi, R2D_SCREEN_W
.si_ok:
    add edi, 18
    cmp edi, R2D_SCREEN_H
    jbe .di_ok
    mov edi, R2D_SCREEN_H
.di_ok:

    ; Align min_x down to even
    and ecx, not 1

    ; Dirty width = max_x - min_x
    sub esi, ecx
    test esi, 1
    jz .w_even
    inc esi
.w_even:
    ; Ensure min_x + w <= R2D_SCREEN_W
    mov eax, ecx
    add eax, esi
    cmp eax, R2D_SCREEN_W
    jbe .w_clamped
    mov esi, R2D_SCREEN_W
    sub esi, ecx
.w_clamped:

    ; Dirty height = max_y - min_y
    sub edi, edx

    ; Update window coordinates and drag_prev
    mov [r15 + wm_x - kmain + rbx*4], r12d
    mov [r15 + wm_y - kmain + rbx*4], r13d
    mov [r15 + wm_drag_prev_x - kmain], r12d
    mov [r15 + wm_drag_prev_y - kmain], r13d

    ; Sanity check: dirty box must be strictly within screen bounds
    test esi, esi
    jle .step_skip_blit
    test edi, edi
    jle .step_skip_blit
    cmp ecx, R2D_SCREEN_W
    jae .step_skip_blit
    cmp edx, R2D_SCREEN_H
    jae .step_skip_blit

    ; Acquire GUI lock
    mov dword [r15 + gui_lock - kmain], 1

    ; Save dirty rectangle to memory
    mov [r15 + wm_drag_dirty_x - kmain], ecx
    mov [r15 + wm_drag_dirty_y - kmain], edx
    mov [r15 + wm_drag_dirty_w - kmain], esi
    mov [r15 + wm_drag_dirty_h - kmain], edi

    ; Step 1: Restore wallpaper in dirty rect (<0.05 ms)
    mov ecx, [r15 + wm_drag_dirty_x - kmain]
    mov edx, [r15 + wm_drag_dirty_y - kmain]
    mov esi, [r15 + wm_drag_dirty_w - kmain]
    mov edi, [r15 + wm_drag_dirty_h - kmain]
    call r2d_restore_wallpaper_rect

    ; Step 2: Redraw Top Menu Bar if dirty rect touches it (Y_min < 28)
    mov edx, [r15 + wm_drag_dirty_y - kmain]
    cmp edx, 28
    jae .skip_mb
    call md_render_menubar
.skip_mb:

    ; Step 3: Render all intersecting windows in Z-order
    mov r8d, [r15 + wm_drag_dirty_x - kmain]
    mov r9d, [r15 + wm_drag_dirty_y - kmain]
    mov r10d, [r15 + wm_drag_dirty_w - kmain]
    mov r11d, [r15 + wm_drag_dirty_h - kmain]
    call wm_render_intersecting

    ; Step 4: Redraw Dock if dirty rect touches it (Dock is always on top)
    mov eax, [r15 + wm_drag_dirty_y - kmain]
    add eax, [r15 + wm_drag_dirty_h - kmain]     ; Y_max
    cmp eax, 990
    jb .skip_dk
    mov eax, [r15 + wm_drag_dirty_x - kmain]
    add eax, [r15 + wm_drag_dirty_w - kmain]     ; X_max
    cmp eax, 800
    jb .skip_dk
    cmp dword [r15 + wm_drag_dirty_x - kmain], 1120
    ja .skip_dk
    call md_render_dock
.skip_dk:
.render_done:

    ; Step 5: Flip only dirty rect to VRAM LFB (~3-5 ms)
    rdtsc
    shl rdx, 32
    or rax, rdx
    mov rbp, rax

    mov ecx, [r15 + wm_drag_dirty_x - kmain]
    mov edx, [r15 + wm_drag_dirty_y - kmain]
    mov esi, [r15 + wm_drag_dirty_w - kmain]
    mov edi, [r15 + wm_drag_dirty_h - kmain]
    call r2d_flip_rect

    rdtsc
    shl rdx, 32
    or rax, rdx
    sub rax, rbp
    mov rbp, rax

    ; Step 6: Draw cursor directly to VRAM LFB at new position
    mov ecx, [r15 + md_mouse_x - kmain]
    mov edx, [r15 + md_mouse_y - kmain]
    mov [r15 + md_mouse_prev_x - kmain], ecx
    mov [r15 + md_mouse_prev_y - kmain], edx
    call r2d_draw_cursor_lfb

    ; Step 7: Record frame & update real-time FPS
    call r2d_record_frame

    ; Release GUI lock
    mov dword [r15 + gui_lock - kmain], 0

    rdtsc
    shl rdx, 32
    or rax, rdx
    sub rax, r14
    mov r14, rax                        ; r14 = elapsed cycles

    lea rsi, [r15 + str_wm_drag_log - kmain]
    call serial_puts
    mov eax, [r15 + wm_drag_dirty_x - kmain]
    call serial_putdec64
    lea rsi, [r15 + str_wm_y_log - kmain]
    call serial_puts
    mov eax, [r15 + wm_drag_dirty_y - kmain]
    call serial_putdec64
    lea rsi, [r15 + str_wm_w_log - kmain]
    call serial_puts
    mov eax, [r15 + wm_drag_dirty_w - kmain]
    call serial_putdec64
    lea rsi, [r15 + str_wm_h_log - kmain]
    call serial_puts
    mov eax, [r15 + wm_drag_dirty_h - kmain]
    call serial_putdec64
    lea rsi, [r15 + str_wm_cyc_log - kmain]
    call serial_puts
    mov rax, r14
    call serial_putdec64
    lea rsi, [r15 + str_wm_flip_cyc - kmain]
    call serial_puts
    mov rax, rbp
    call serial_putdec64
    mov al, 10
    call serial_putc

.step_skip_blit:
    pop rbp
    pop r14
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
; wm_render_intersecting: Render open windows in Z-order that intersect dirty box
; R8D = X_min, R9D = Y_min, R10D = W_dirty, R11D = H_dirty
; ==============================================================================
wm_render_intersecting:
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
    sub rsp, 32                       ; local variables frame

    ; [rsp + 0] = X_min
    ; [rsp + 4] = Y_min
    ; [rsp + 8] = X_max
    ; [rsp + 12] = Y_max
    mov [rsp + 0], r8d
    mov [rsp + 4], r9d
    mov eax, r8d
    add eax, r10d
    mov [rsp + 8], eax                ; X_max
    mov edx, r9d
    add edx, r11d
    mov [rsp + 12], edx               ; Y_max

    xor r12d, r12d                    ; z index 0..open_count-1
.check_win_loop:
    cmp r12b, [r15 + wm_open_count - kmain]
    jae .intersect_done

    movzx ebx, byte [r15 + wm_z_order - kmain + r12] ; app_id

    ; Load window bounds + shadow margin 18
    mov eax, [r15 + wm_x - kmain + rbx*4]
    sub eax, 18                       ; wx1
    jns .wx1_pos
    xor eax, eax
.wx1_pos:
    mov ecx, [r15 + wm_x - kmain + rbx*4]
    add ecx, [r15 + wm_w - kmain + rbx*4]
    add ecx, 18                       ; wx2

    mov edx, [r15 + wm_y - kmain + rbx*4]
    sub edx, 18                       ; wy1
    jns .wy1_pos
    xor edx, edx
.wy1_pos:
    mov esi, [r15 + wm_y - kmain + rbx*4]
    add esi, [r15 + wm_h - kmain + rbx*4]
    add esi, 18                       ; wy2

    ; Intersection test:
    cmp ecx, [rsp + 0]                ; wx2 <= X_min ?
    jbe .skip_this_win
    cmp eax, [rsp + 8]                ; wx1 >= X_max ?
    jae .skip_this_win
    cmp esi, [rsp + 4]                ; wy2 <= Y_min ?
    jbe .skip_this_win
    cmp edx, [rsp + 12]               ; wy1 >= Y_max ?
    jae .skip_this_win

    ; Window intersects! Render it.
    xor eax, eax
    cmp bl, [r15 + wm_focus_app - kmain]
    sete al                           ; 1 if focused, 0 otherwise

    call wm_render_window

.skip_this_win:
    inc r12d
    jmp .check_win_loop

.intersect_done:
    add rsp, 32
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
; wm_drag_flush_pending: Flush coalesced drag step if 16ms elapsed and moved
; ==============================================================================
wm_drag_flush_pending:
    push rax
    push rbx
    push rdx

    movzx ebx, byte [r15 + wm_drag_app - kmain]
    cmp ebx, APP_COUNT
    jae .dfp_done

    mov eax, [r15 + wm_x - kmain + rbx*4]
    cmp eax, [r15 + wm_drag_prev_x - kmain]
    jne .dfp_check_time
    mov edx, [r15 + wm_y - kmain + rbx*4]
    cmp edx, [r15 + wm_drag_prev_y - kmain]
    je .dfp_done

.dfp_check_time:
    mov eax, [r15 + timer_ticks - kmain]
    sub eax, [r15 + wm_last_drag_tick - kmain]
    cmp eax, 16
    jb .dfp_done

    mov eax, [r15 + timer_ticks - kmain]
    mov [r15 + wm_last_drag_tick - kmain], eax

    mov eax, [r15 + wm_x - kmain + rbx*4]
    mov edx, [r15 + wm_y - kmain + rbx*4]
    call wm_drag_step

.dfp_done:
    pop rdx
    pop rbx
    pop rax
    ret

; ==============================================================================
; wm_create_user_window: Allocate and show a dynamic user application window
; RDI = title pointer (user or kernel memory)
; ESI = x, EDX = y, R10D = w, R8D = h
; Returns: RAX = (canvas_ptr << 32) | win_id (or -1 if no slots available)
; ==============================================================================
; wm_create_user_window: Allocate and show a dynamic user application window
; RDI = title pointer (user or kernel memory)
; ESI = x, EDX = y, R10D = w, R8D = h
; Returns: RAX = (canvas_ptr << 32) | win_id (or -1 if no slots available)
; ==============================================================================
wm_create_user_window:
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

    ; 1. Find free user window slot (0..7)
    mov ebx, 0
.find_slot:
    cmp ebx, APP_COUNT
    jae .no_slot
    cmp byte [r15 + wm_state - kmain + rbx], WM_STATE_CLOSED
    je .found_slot
    inc ebx
    jmp .find_slot

.no_slot:
    mov rax, -1
    jmp .create_done

.found_slot:
    ; ebx = slot (0..7), app_id = ebx
    mov r12d, ebx

    ; 2. Validate and clamp dimensions (H1 Mitigation: 960x560 fits 2MB canvas)
    ; Width: clamp to [200, 960]
    cmp r10d, 200
    jge @f
    mov r10d, 200
@@:
    cmp r10d, 960
    jle @f
    mov r10d, 960
@@:

    ; Height: clamp to [150, 560]
    cmp r8d, 150
    jge @f
    mov r8d, 150
@@:
    cmp r8d, 560
    jle @f
    mov r8d, 560
@@:

    ; Position X: clamp/default
    cmp esi, 20
    jge @f
    ; Default X staggered by slot: 240 + slot * 40
    mov esi, 240
    mov eax, ebx
    imul eax, 40
    add esi, eax
@@:
    cmp esi, 1500
    jle @f
    mov esi, 1500
@@:

    ; Position Y: clamp/default
    cmp edx, 32
    jge @f
    ; Default Y staggered by slot: 140 + slot * 40
    mov edx, 140
    mov eax, ebx
    imul eax, 40
    add edx, eax
@@:
    cmp edx, 800
    jle @f
    mov edx, 800
@@:

    ; Save window geometry
    mov [r15 + wm_x - kmain + r12*4], esi
    mov [r15 + wm_y - kmain + r12*4], edx
    mov [r15 + wm_w - kmain + r12*4], r10d
    mov [r15 + wm_h - kmain + r12*4], r8d

    ; 3. Copy title string to wm_user_titles[slot * 64]
    mov eax, ebx
    shl eax, 6                         ; slot * 64
    lea r13, [r15 + wm_user_titles - kmain + rax]

    test rdi, rdi
    jz .default_title

    ; Validate user title pointer (< 0x0000800000000000)
    push rdi
    mov rsi, 64
    call sys_validate_user_ptr
    pop rdi
    jc .default_title

    ; Copy from user string at rdi
    mov rsi, rdi
    mov rdi, r13
    mov ecx, 63
.copy_title:
    lodsb
    stosb
    test al, al
    jz .title_done
    dec ecx
    jnz .copy_title
    mov byte [rdi], 0
    jmp .title_done

.default_title:
    mov dword [r13 + 0], "User"
    mov dword [r13 + 4], " App"
    mov byte  [r13 + 8], 0

.title_done:
    ; 4. Clear client canvas buffer
    ; Canvas ptr = wm_user_canvas[slot]
    mov r9, [r15 + wm_user_canvas - kmain + rbx*8]
    mov rdi, r9
    mov ecx, r10d                     ; width
    mov eax, r8d                      ; height
    sub eax, 33                       ; client height = h - 33
    imul ecx, eax                     ; total dwords
    ; H1 Mitigation: Never write beyond 2MB canvas (524288 dwords)
    cmp ecx, 524288
    jbe @f
    mov ecx, 524288
@@:
    mov eax, 0xFF0F172A               ; Default Dark Slate background
    cld
    rep stosd

    ; 5. Clear event state
    mov byte [r15 + wm_user_ev_head - kmain + rbx], 0
    mov byte [r15 + wm_user_ev_tail - kmain + rbx], 0

    ; 6. Open window via WM
    mov al, r12b                      ; app_id
    call wm_open_app

    ; 7. Return (canvas_ptr << 32) | app_id
    mov rdx, [r15 + wm_user_canvas - kmain + rbx*8]
    shl rdx, 32
    mov eax, r12d                     ; app_id (0..7)
    or rax, rdx

.create_done:
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
    ret

; ==============================================================================
; wm_post_event: Push 16-byte event into window EBX's event ring buffer
; Input:
; EBX = win_id (0..7)
; EAX = event_type (1..6)
; ECX = data1 / x
; EDX = data2 / y
; R8D = param / key / button
; ==============================================================================
wm_post_event:
    cmp ebx, APP_COUNT
    jae .wpe_ret
    push rdi
    push rsi
    push r9

    movzx esi, byte [r15 + wm_user_ev_head - kmain + rbx]
    lea r9d, [esi + 1]
    and r9d, 15
    cmp r9b, byte [r15 + wm_user_ev_tail - kmain + rbx]
    je .wpe_full              ; Queue is full, drop event

    ; Calculate slot offset: (slot << 8) + (head << 4)
    movzx edi, bl
    shl edi, 8
    shl esi, 4
    add edi, esi
    lea rdi, [r15 + wm_user_events - kmain + rdi]

    mov [rdi + 0], eax        ; type
    mov [rdi + 4], ecx        ; data1
    mov [rdi + 8], edx        ; data2
    mov [rdi + 12], r8d       ; param

    mov [r15 + wm_user_ev_head - kmain + rbx], r9b
    mov byte [r15 + md_term_dirty - kmain], 1

.wpe_full:
    pop r9
    pop rsi
    pop rdi
.wpe_ret:
    ret

; ==============================================================================
; wm_close_user_window: Close user application window
; EDI = win_id (0..7)
; Returns: RAX = 0 on success, -1 on error
; ==============================================================================
wm_close_user_window:
    cmp edi, APP_COUNT
    jae .cuw_err

    mov al, dil
    call wm_close_app

    mov eax, edi
    mov byte [r15 + wm_user_ev_head - kmain + rax], 0
    mov byte [r15 + wm_user_ev_tail - kmain + rax], 0
    xor eax, eax
    ret

.cuw_err:
    mov rax, -1
    ret

; ==============================================================================
; wm_poll_user_event: Poll for pending GUI events for window EDI
; EDI = win_id (0..7), RSI = pointer to 16-byte user buffer
; Returns: RAX = 1 if event retrieved, 0 if no event, -1 on invalid win_id
; ==============================================================================
wm_poll_user_event:
    cmp edi, APP_COUNT
    jae .pue_err
    test rsi, rsi
    jz .pue_err

    ; Validate 16-byte user buffer
    push rdi
    push rsi
    mov rdi, rsi
    mov esi, 16
    call sys_validate_user_ptr
    pop rsi
    pop rdi
    jc .pue_err

    movzx ebx, dil            ; window slot (0..7)
    movzx edx, byte [r15 + wm_user_ev_tail - kmain + rbx]
    cmp dl, byte [r15 + wm_user_ev_head - kmain + rbx]
    je .pue_none              ; Head == Tail: No event!

    ; Read event at tail: (slot << 8) + (tail << 4)
    movzx eax, bl
    shl eax, 8
    movzx ecx, dl
    shl ecx, 4
    add eax, ecx
    lea r8, [r15 + wm_user_events - kmain + rax]

    mov rax, [r8]
    mov [rsi], rax
    mov rax, [r8 + 8]
    mov [rsi + 8], rax

    ; Advance tail: (tail + 1) & 15
    movzx edx, byte [r15 + wm_user_ev_tail - kmain + rbx]
    inc edx
    and edx, 15
    mov [r15 + wm_user_ev_tail - kmain + rbx], dl

    mov eax, 1                ; Event retrieved!
    ret

.pue_none:
    xor eax, eax
    ret

.pue_err:
    mov rax, -1
    ret

; ==============================================================================
; wm_update_user_window: Request window redraw (triggers compositor)
; EDI = win_id (0..7)
; Returns: RAX = 0 on success, -1 on error
; ==============================================================================
wm_update_user_window:
    cmp edi, APP_COUNT
    jae .uuw_err

    mov byte [r15 + md_term_dirty - kmain], 1
    xor eax, eax
    ret

.uuw_err:
    mov rax, -1
    ret




