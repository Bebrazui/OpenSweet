; ==============================================================================
; Opensweet OS - First Native Ring 3 GUI Application (user/gui_demo.asm)
; Demonstrates User-Space GUI Windowing API:
;   - sys_gui_create_win (10): Open native window with dedicated 32bpp ARGB canvas
;   - sys_gui_update_win (11): Request compositor refresh
;   - sys_gui_poll_event (12): Non-blocking mouse/keyboard/close event polling
;   - sys_gui_close_win  (13): Destroy window
; ==============================================================================

format ELF64 executable 3
entry start

SYS_EXIT           = 0
SYS_WRITE          = 1
SYS_READ           = 2
SYS_SLEEP          = 3
SYS_GUI_CREATE_WIN = 10
SYS_GUI_UPDATE_WIN = 11
SYS_GUI_POLL_EVENT = 12
SYS_GUI_CLOSE_WIN  = 13

EVENT_NONE         = 0
EVENT_MOUSE_MOVE   = 1
EVENT_MOUSE_DOWN   = 2
EVENT_MOUSE_UP     = 3
EVENT_KEY_DOWN     = 4
EVENT_WIN_CLOSE    = 5

WIN_WIDTH          = 440
WIN_HEIGHT         = 320
CLIENT_WIDTH       = 440
CLIENT_HEIGHT      = 287              ; 320 - 33

segment readable executable
start:
    ; 1. Announce startup to terminal/stdout
    mov eax, SYS_WRITE
    mov edi, 1
    mov rsi, msg_launch
    mov edx, msg_launch_len
    syscall

    ; 2. Create native desktop window via sys_gui_create_win (10)
    ; RDI = title string, RSI = x, RDX = y, R10 = w, R8 = h
    mov eax, SYS_GUI_CREATE_WIN
    mov rdi, win_title
    mov esi, 320                      ; Initial X
    mov edx, 160                      ; Initial Y
    mov r10d, WIN_WIDTH               ; 440
    mov r8d, WIN_HEIGHT               ; 320
    syscall

    ; RAX = (canvas_ptr << 32) | win_id
    cmp rax, -1
    je .create_failed

    mov [win_id], eax
    shr rax, 32
    mov [canvas_ptr], rax

    ; Print window creation success
    mov eax, SYS_WRITE
    mov edi, 1
    mov rsi, msg_created
    mov edx, msg_created_len
    syscall

    ; 3. Initial UI Render into client canvas buffer
    call render_initial_ui

    ; 4. Request initial window blit from compositor
    mov eax, SYS_GUI_UPDATE_WIN
    mov edi, [win_id]
    syscall

    ; 5. Interactive Event Loop
.event_loop:
    ; Check frame limit for automated testing / idle timeout (600 frames @ 16ms ~= 10 sec)
    inc dword [frame_counter]
    cmp dword [frame_counter], 600
    jae .loop_exit

    ; Poll pending event from Window Manager
    mov eax, SYS_GUI_POLL_EVENT
    mov edi, [win_id]
    mov rsi, event_buffer
    syscall

    test eax, eax
    jz .no_event

    ; Event received! Check event type at offset 0
    mov eax, [event_buffer + 0]
    cmp eax, EVENT_WIN_CLOSE
    je .loop_exit

    cmp eax, EVENT_MOUSE_DOWN
    je .on_mouse_down

    jmp .no_event

.on_mouse_down:
    ; Mouse clicked inside client area!
    ; offset 4 = mouse_x, offset 8 = mouse_y
    mov r8d, [event_buffer + 4]        ; click_x
    mov r9d, [event_buffer + 8]        ; click_y

    inc dword [click_count]

    ; Print click log to terminal
    push r8
    push r9
    mov eax, SYS_WRITE
    mov edi, 1
    mov rsi, msg_click
    mov edx, msg_click_len
    syscall
    pop r9
    pop r8

    ; Draw ripple effect at click coordinates
    ; Outer ring (radius 14, Cyan)
    mov ecx, r8d
    mov edx, r9d
    mov esi, 14
    mov eax, 0xFF06B6D4
    call draw_circle

    ; Middle ring (radius 9, Amber)
    mov ecx, r8d
    mov edx, r9d
    mov esi, 9
    mov eax, 0xFFF59E0B
    call draw_circle

    ; Center dot (radius 4, White)
    mov ecx, r8d
    mov edx, r9d
    mov esi, 4
    mov eax, 0xFFFFFFFF
    call draw_circle

    ; Update event info card
    call update_stats_display

    ; Notify compositor of update
    mov eax, SYS_GUI_UPDATE_WIN
    mov edi, [win_id]
    syscall

.no_event:
    ; Paced sleep for ~16ms (60 FPS event polling)
    mov eax, SYS_SLEEP
    mov edi, 16
    syscall
    jmp .event_loop

.loop_exit:
    ; Cleanly close window
    mov eax, SYS_GUI_CLOSE_WIN
    mov edi, [win_id]
    syscall

    ; Print exit message
    mov eax, SYS_WRITE
    mov edi, 1
    mov rsi, msg_exit
    mov edx, msg_exit_len
    syscall

    ; Clean sys_exit
    mov eax, SYS_EXIT
    xor edi, edi
    syscall

.halt:
    jmp .halt

.create_failed:
    mov eax, SYS_WRITE
    mov edi, 1
    mov rsi, msg_err
    mov edx, msg_err_len
    syscall

    mov eax, SYS_EXIT
    mov edi, 1
    syscall

; ==============================================================================
; render_initial_ui: Paint initial window components into client canvas
; ==============================================================================
render_initial_ui:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    ; 1. Window Background (Slate 900)
    mov ecx, 0
    mov edx, 0
    mov esi, CLIENT_WIDTH
    mov edi, CLIENT_HEIGHT
    mov eax, 0xFF0F172A
    call fill_rect

    ; 2. Header Banner Card (Indigo 700)
    mov ecx, 10
    mov edx, 10
    mov esi, 420
    mov edi, 46
    mov eax, 0xFF4338CA
    call fill_rect

    ; 2b. Header Top Edge Accent
    mov ecx, 10
    mov edx, 10
    mov esi, 420
    mov edi, 2
    mov eax, 0xFF818CF8
    call fill_rect

    ; Header Text
    mov ecx, 24
    mov edx, 18
    mov rsi, str_hdr1
    mov eax, 0xFFFFFFFF
    call draw_string

    mov ecx, 24
    mov edx, 34
    mov rsi, str_hdr2
    mov eax, 0xFFA5B4FC
    call draw_string

    ; 3. Left Card: Event Monitor (Slate 800)
    mov ecx, 10
    mov edx, 66
    mov esi, 205
    mov edi, 140
    mov eax, 0xFF1E293B
    call fill_rect

    ; Left Card Accent
    mov ecx, 10
    mov edx, 66
    mov esi, 205
    mov edi, 2
    mov eax, 0xFF0284C7
    call fill_rect

    mov ecx, 20
    mov edx, 76
    mov rsi, str_card1_title
    mov eax, 0xFF38BDF8
    call draw_string

    mov ecx, 20
    mov edx, 100
    mov rsi, str_clicks_lbl
    mov eax, 0xFF94A3B8
    call draw_string

    mov ecx, 20
    mov edx, 124
    mov rsi, str_x_lbl
    mov eax, 0xFF94A3B8
    call draw_string

    mov ecx, 20
    mov edx, 148
    mov rsi, str_y_lbl
    mov eax, 0xFF94A3B8
    call draw_string

    mov ecx, 20
    mov edx, 176
    mov rsi, str_status_val
    mov eax, 0xFF34D399
    call draw_string

    ; 4. Right Card: Canvas Pad (Slate 800)
    mov ecx, 225
    mov edx, 66
    mov esi, 205
    mov edi, 140
    mov eax, 0xFF1E293B
    call fill_rect

    ; Right Card Accent
    mov ecx, 225
    mov edx, 66
    mov esi, 205
    mov edi, 2
    mov eax, 0xFF10B981
    call fill_rect

    mov ecx, 235
    mov edx, 76
    mov rsi, str_card2_title
    mov eax, 0xFF34D399
    call draw_string

    mov ecx, 235
    mov edx, 106
    mov rsi, str_pad_msg1
    mov eax, 0xFFE2E8F0
    call draw_string

    mov ecx, 235
    mov edx, 126
    mov rsi, str_pad_msg2
    mov eax, 0xFFCBD5E1
    call draw_string

    mov ecx, 235
    mov edx, 150
    mov rsi, str_pad_msg3
    mov eax, 0xFFFCD34D
    call draw_string

    ; 5. Footer Card (Slate 800)
    mov ecx, 10
    mov edx, 216
    mov esi, 420
    mov edi, 60
    mov eax, 0xFF1E293B
    call fill_rect

    ; Footer Accent
    mov ecx, 10
    mov edx, 216
    mov esi, 420
    mov edi, 2
    mov eax, 0xFF6366F1
    call fill_rect

    mov ecx, 20
    mov edx, 228
    mov rsi, str_foot1
    mov eax, 0xFFCBD5E1
    call draw_string

    mov ecx, 20
    mov edx, 248
    mov rsi, str_foot2
    mov eax, 0xFF94A3B8
    call draw_string

    ; Display initial numbers
    call update_stats_display

    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; ==============================================================================
; update_stats_display: Redraw dynamic numbers in Left Card
; ==============================================================================
update_stats_display:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    ; Clear number regions
    mov ecx, 90
    mov edx, 98
    mov esi, 110
    mov edi, 68
    mov eax, 0xFF1E293B
    call fill_rect

    ; Draw Clicks number
    mov eax, [click_count]
    mov rdi, num_buf
    call itoa
    mov ecx, 90
    mov edx, 100
    mov rsi, num_buf
    mov eax, 0xFFF8FAFC
    call draw_string

    ; Draw Last X
    mov eax, [event_buffer + 4]
    mov rdi, num_buf
    call itoa
    mov ecx, 90
    mov edx, 124
    mov rsi, num_buf
    mov eax, 0xFFF8FAFC
    call draw_string

    ; Draw Last Y
    mov eax, [event_buffer + 8]
    mov rdi, num_buf
    call itoa
    mov ecx, 90
    mov edx, 148
    mov rsi, num_buf
    mov eax, 0xFFF8FAFC
    call draw_string

    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; ==============================================================================
; fill_rect: Fill rectangle with 32bpp ARGB color directly in user canvas
; ECX = x, EDX = y, ESI = w, EDI = h, EAX = color
; ==============================================================================
fill_rect:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    test esi, esi
    jle .fr_done
    test edi, edi
    jle .fr_done

    mov r8, [canvas_ptr]
    test r8, r8
    jz .fr_done

    mov r9d, ecx                      ; x
    mov r10d, edx                     ; cur_y
    mov r11d, edi                     ; remaining height
    mov ebx, eax                      ; color

.fr_row_loop:
    test r11d, r11d
    jz .fr_done

    cmp r10d, 0
    jl .fr_skip_row
    cmp r10d, CLIENT_HEIGHT
    jge .fr_done

    ; Calculate canvas row address: canvas_ptr + cur_y * (CLIENT_WIDTH * 4) + x * 4
    mov eax, r10d
    imul eax, CLIENT_WIDTH * 4
    lea rdi, [r8 + rax]
    lea rdi, [rdi + r9*4]

    ; Store width dwords
    push rcx
    mov ecx, esi
    mov eax, ebx
    cld
    rep stosd
    pop rcx

.fr_skip_row:
    inc r10d
    dec r11d
    jmp .fr_row_loop

.fr_done:
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
; draw_circle: Draw filled circle at (cx, cy) with radius in canvas
; ECX = cx, EDX = cy, ESI = radius, EAX = color
; ==============================================================================
draw_circle:
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

    mov r8, [canvas_ptr]
    test r8, r8
    jz .dc_done

    mov r9d, ecx                      ; cx
    mov r10d, edx                     ; cy
    mov r11d, esi                     ; r
    mov r12d, eax                     ; color

    ; r13d = r * r
    mov eax, r11d
    imul eax, eax
    mov r13d, eax

    ; dy from -r to +r
    mov ebx, r11d
    neg ebx                           ; ebx = dy = -r

.dc_y_loop:
    cmp ebx, r11d
    jg .dc_done

    ; py = cy + dy
    lea edx, [r10d + ebx]
    cmp edx, 0
    jl .dc_next_y
    cmp edx, CLIENT_HEIGHT
    jge .dc_next_y

    ; dx from -r to +r
    mov esi, r11d
    neg esi                           ; esi = dx = -r

.dc_x_loop:
    cmp esi, r11d
    jg .dc_next_y

    ; px = cx + dx
    lea ecx, [r9d + esi]
    cmp ecx, 0
    jl .dc_next_x
    cmp ecx, CLIENT_WIDTH
    jge .dc_next_x

    ; Check dist_sq = dx*dx + dy*dy <= r*r
    mov eax, esi
    imul eax, eax
    mov edi, ebx
    imul edi, edi
    add eax, edi
    cmp eax, r13d
    ja .dc_next_x

    ; Plot pixel (ecx, edx)
    mov eax, edx
    imul eax, CLIENT_WIDTH * 4
    lea rdi, [r8 + rax]
    mov [rdi + rcx*4], r12d

.dc_next_x:
    inc esi
    jmp .dc_x_loop

.dc_next_y:
    inc ebx
    jmp .dc_y_loop

.dc_done:
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
; draw_string: Draw null-terminated string at (x, y) with 8x8 font
; ECX = x, EDX = y, RSI = str_ptr, EAX = color
; ==============================================================================
draw_string:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    mov ebx, eax                      ; color
.ds_char_loop:
    lodsb
    test al, al
    jz .ds_done

    push rcx
    push rdx
    push rsi
    movzx esi, al                     ; char
    mov eax, ebx                      ; color
    call draw_char
    pop rsi
    pop rdx
    pop rcx

    add ecx, 8                        ; Advance 8 pixels horizontally
    jmp .ds_char_loop

.ds_done:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

; ==============================================================================
; draw_char: Draw single 8x8 character
; ECX = x, EDX = y, ESI = ASCII char, EAX = color
; ==============================================================================
draw_char:
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

    mov r8, [canvas_ptr]
    test r8, r8
    jz .dch_done

    mov r9d, ecx                      ; x
    mov r10d, edx                     ; y
    mov r11d, eax                     ; color

    ; Validate ASCII (32..90)
    cmp esi, 32
    jb .dch_done
    cmp esi, 90
    ja .dch_check_lower

.dch_has_glyph:
    sub esi, 32
    shl esi, 3                        ; esi = glyph offset = (char - 32) * 8
    lea rbx, [font8x8_data + rsi]

    ; 8 rows
    xor edx, edx                      ; row = 0
.dch_row_loop:
    cmp edx, 8
    jae .dch_done

    lea eax, [r10d + edx]             ; cur_y = y + row
    cmp eax, 0
    jl .dch_next_row
    cmp eax, CLIENT_HEIGHT
    jge .dch_done

    ; Row bitmap in r12b
    mov r12b, [rbx + rdx]

    ; Row start address
    imul eax, CLIENT_WIDTH * 4
    lea rdi, [r8 + rax]

    ; 8 columns
    xor ecx, ecx                      ; col = 0
.dch_col_loop:
    cmp ecx, 8
    jae .dch_next_row

    ; Shift highest bit out into Carry Flag
    shl r12b, 1
    jnc .dch_next_col

    lea eax, [r9d + ecx]              ; cur_x = x + col
    cmp eax, 0
    jl .dch_next_col
    cmp eax, CLIENT_WIDTH
    jge .dch_next_col

    ; Plot pixel
    mov [rdi + rax*4], r11d

.dch_next_col:
    inc ecx
    jmp .dch_col_loop

.dch_next_row:
    inc edx
    jmp .dch_row_loop

.dch_check_lower:
    ; Convert lowercase 'a'..'z' (97..122) to uppercase
    cmp esi, 97
    jb .dch_done
    cmp esi, 122
    ja .dch_done
    sub esi, 32
    jmp .dch_has_glyph

.dch_done:
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
; itoa: Convert unsigned integer to null-terminated decimal string
; EAX = integer, RDI = output buffer
; ==============================================================================
itoa:
    push rbx
    push rcx
    push rdx
    push rdi

    mov rbx, rdi
    test eax, eax
    jnz .it_convert
    mov byte [rdi], '0'
    mov byte [rdi + 1], 0
    jmp .it_done

.it_convert:
    mov ecx, 10
    mov r8, rdi
.it_div_loop:
    test eax, eax
    jz .it_reverse
    xor edx, edx
    div ecx
    add dl, '0'
    mov [r8], dl
    inc r8
    jmp .it_div_loop

.it_reverse:
    mov byte [r8], 0                  ; Null terminator
    dec r8                            ; r8 points to last char
    mov rsi, rbx                      ; rsi points to first char
.it_rev_loop:
    cmp rsi, r8
    jae .it_done
    mov al, [rsi]
    mov bl, [r8]
    mov [rsi], bl
    mov [r8], al
    inc rsi
    dec r8
    jmp .it_rev_loop

.it_done:
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    ret

segment readable writeable
win_id          dd 0
canvas_ptr      dq 0
click_count     dd 0
frame_counter   dd 0

align 16
event_buffer:   rb 16                 ; 16-byte event struct
num_buf:        rb 32

win_title       db "Ring 3 Demo App", 0

msg_launch      db 10, "[gui_demo.elf] Launching Ring 3 Native GUI demo...", 10, 0
msg_launch_len  = $ - msg_launch - 1

msg_created     db "[gui_demo.elf] Native window created successfully via SYS_GUI_CREATE_WIN!", 10, 0
msg_created_len = $ - msg_created - 1

msg_click       db "[gui_demo.elf] Mouse event received! Repainting canvas ripple...", 10, 0
msg_click_len   = $ - msg_click - 1

msg_exit        db "[gui_demo.elf] Window closed. Clean exit via sys_exit(0).", 10, 0
msg_exit_len    = $ - msg_exit - 1

msg_err         db "[gui_demo.elf] Error: Failed to create window (no free slots).", 10, 0
msg_err_len     = $ - msg_err - 1

str_hdr1        db "OPENSWEET OS - RING 3 DEMO", 0
str_hdr2        db "Native ELF64 App via SYSCALL API", 0

str_card1_title db "EVENT MONITOR", 0
str_clicks_lbl  db "Clicks: ", 0
str_x_lbl       db "Last X: ", 0
str_y_lbl       db "Last Y: ", 0
str_status_val  db "Status: ACTIVE", 0

str_card2_title db "CANVAS PAD", 0
str_pad_msg1    db "Click anywhere", 0
str_pad_msg2    db "inside window to", 0
str_pad_msg3    db "draw ripple spots!", 0

str_foot1       db "Direct 32bpp ARGB Canvas Access", 0
str_foot2       db "60 FPS Compositor | CPL=3 Isolation", 0

; 8x8 Monochrome Bitmap Font for ASCII 32 (' ') to 90 ('Z')
align 16
font8x8_data:
    ; 32 ' '
    db 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    ; 33 '!'
    db 0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00
    ; 34 '"'
    db 0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00
    ; 35 '#'
    db 0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00
    ; 36 '$'
    db 0x18,0x7E,0xD8,0x7C,0x1B,0x7E,0x18,0x00
    ; 37 '%'
    db 0x00,0xC6,0xCC,0x18,0x30,0x66,0xC6,0x00
    ; 38 '&'
    db 0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00
    ; 39 '''
    db 0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00
    ; 40 '('
    db 0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00
    ; 41 ')'
    db 0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00
    ; 42 '*'
    db 0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00
    ; 43 '+'
    db 0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00
    ; 44 ','
    db 0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30
    ; 45 '-'
    db 0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00
    ; 46 '.'
    db 0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00
    ; 47 '/'
    db 0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00
    ; 48 '0'
    db 0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00
    ; 49 '1'
    db 0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00
    ; 50 '2'
    db 0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0x00
    ; 51 '3'
    db 0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00
    ; 52 '4'
    db 0x0C,0x1C,0x3C,0x6C,0xFE,0x0C,0x0C,0x00
    ; 53 '5'
    db 0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00
    ; 54 '6'
    db 0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00
    ; 55 '7'
    db 0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x00
    ; 56 '8'
    db 0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00
    ; 57 '9'
    db 0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00
    ; 58 ':'
    db 0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00
    ; 59 ';'
    db 0x00,0x18,0x18,0x00,0x18,0x18,0x30,0x00
    ; 60 '<'
    db 0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00
    ; 61 '='
    db 0x00,0x7E,0x00,0x00,0x7E,0x00,0x00,0x00
    ; 62 '>'
    db 0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x00
    ; 63 '?'
    db 0x3C,0x66,0x06,0x0C,0x18,0x00,0x18,0x00
    ; 64 '@'
    db 0x3C,0x66,0x6E,0x6A,0x6E,0x60,0x3C,0x00
    ; 65 'A'
    db 0x18,0x3C,0x66,0x7E,0x66,0x66,0x66,0x00
    ; 66 'B'
    db 0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00
    ; 67 'C'
    db 0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00
    ; 68 'D'
    db 0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00
    ; 69 'E'
    db 0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00
    ; 70 'F'
    db 0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00
    ; 71 'G'
    db 0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0x00
    ; 72 'H'
    db 0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00
    ; 73 'I'
    db 0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00
    ; 74 'J'
    db 0x0E,0x06,0x06,0x06,0x06,0x66,0x3C,0x00
    ; 75 'K'
    db 0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00
    ; 76 'L'
    db 0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00
    ; 77 'M'
    db 0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00
    ; 78 'N'
    db 0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00
    ; 79 'O'
    db 0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00
    ; 80 'P'
    db 0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00
    ; 81 'Q'
    db 0x3C,0x66,0x66,0x66,0x6E,0x3C,0x0E,0x00
    ; 82 'R'
    db 0x7C,0x66,0x66,0x7C,0x78,0x6C,0x66,0x00
    ; 83 'S'
    db 0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00
    ; 84 'T'
    db 0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00
    ; 85 'U'
    db 0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00
    ; 86 'V'
    db 0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00
    ; 87 'W'
    db 0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00
    ; 88 'X'
    db 0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00
    ; 89 'Y'
    db 0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00
    ; 90 'Z'
    db 0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00
