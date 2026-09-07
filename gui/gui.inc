; ==============================================================================
; Opensweet OS - Micro-GUI Subsystem (LVGL-inspired Architecture)
; High-performance 32bpp Linear Framebuffer + Double Buffering (RAM Backbuffer)
; ==============================================================================

include 'D:\Opensweet\gui\font8x16.inc'

GUI_BACKBUFFER    = 0x02000000   ; 4MB RAM Backbuffer (Page 0x2000..0x23FF)

; --- Modern Dark Palette (Tailwind / Slate / LVGL theme) ---
GUI_CLR_BG        = 0x000F172A   ; Slate 900 (Main desktop backdrop)
GUI_CLR_PANEL     = 0x001E293B   ; Slate 800 (Windows, dock, containers)
GUI_CLR_CARD      = 0x00172033   ; Deep Slate (Inner cards)
GUI_CLR_TOPBAR    = 0x000B0F19   ; Slate 950 (Top status bar)
GUI_CLR_BORDER    = 0x00334155   ; Slate 700 (Subtle dividers & borders)
GUI_CLR_BORDER_HI = 0x00475569   ; Slate 600 (Focused borders)
GUI_CLR_TEXT_HI   = 0x00F8FAFC   ; Slate 50 (Primary crisp text)
GUI_CLR_TEXT_MID  = 0x00CBD5E1   ; Slate 300 (Secondary text)
GUI_CLR_TEXT_MUT  = 0x0064748B   ; Slate 500 (Muted / labels)
GUI_CLR_ACCENT    = 0x003B82F6   ; Blue 500 (Primary highlights / buttons)
GUI_CLR_ACCENT_DK = 0x001D4ED8   ; Blue 700 (Button borders)
GUI_CLR_SUCCESS   = 0x0010B981   ; Emerald 500 (OK badges / active)
GUI_CLR_WARNING   = 0x00F59E0B   ; Amber 500 (Warning badges)
GUI_CLR_DANGER    = 0x00EF4444   ; Red 500 (Close / errors)
GUI_CLR_CYAN      = 0x0006B6D4   ; Cyan 500 (File previews)
GUI_CLR_PURPLE    = 0x008B5CF6   ; Purple 500 (Architecture tag)
GUI_CLR_NONE      = 0xFFFFFFFF   ; Transparent background flag

; Traffic light buttons
GUI_DOT_CLOSE     = 0x00FF5F56
GUI_DOT_MIN       = 0x00FFBD2E
GUI_DOT_MAX       = 0x0027C93F

; ==============================================================================
; gui_init: Initialize GUI variables from VBE BIOS data
; ==============================================================================
gui_init:
    cmp byte [r14 + VBS_OK], 1
    jne .err
    movzx eax, word [r14 + VBS_PITCH]
    mov [r15 + vbe_pitch - kmain], eax
    movzx eax, word [r14 + VBS_WIDTH]
    mov [r15 + vbe_width - kmain], eax
    movzx eax, word [r14 + VBS_HEIGHT]
    mov [r15 + vbe_height - kmain], eax
    mov eax, dword [r14 + VBS_LFB]
    mov [r15 + vbe_lfb - kmain], eax
    clc
    ret
.err:
    stc
    ret

; ==============================================================================
; gui_flip: Mtx copy RAM backbuffer -> VRAM LFB via rep movsq (instant, no tear)
; ==============================================================================
gui_flip:
    push rsi
    push rdi
    push rcx
    push rax

    cmp byte [r14 + VBS_OK], 1
    jne .done

    mov edi, [r15 + vbe_lfb - kmain]
    test edi, edi
    jz .done

    mov rsi, GUI_BACKBUFFER
    mov eax, [r15 + vbe_width - kmain]
    imul eax, [r15 + vbe_height - kmain]
    shr eax, 1                        ; divide by 2 for qwords
    mov ecx, eax
    rep movsq

.done:
    pop rax
    pop rcx
    pop rdi
    pop rsi
    ret

; ==============================================================================
; gui_fill_rect: Fills rectangle in RAM Backbuffer
; Inputs: ECX=x, EDX=y, ESI=w, EBP=h, EAX=color
; ==============================================================================
gui_fill_rect:
    push rbx
    push r12
    push r13
    push rdi
    push rcx
    push rdx
    push rsi
    push rbp

    cmp byte [r14 + VBS_OK], 1
    jne .done

    mov r12d, [r15 + vbe_width - kmain]
    mov r13d, [r15 + vbe_height - kmain]

    cmp ecx, r12d
    jae .done
    cmp edx, r13d
    jae .done

    ; clip width
    lea ebx, [rcx + rsi]
    cmp ebx, r12d
    jbe .w_ok
    mov esi, r12d
    sub esi, ecx
.w_ok:
    ; clip height
    lea ebx, [rdx + rbp]
    cmp ebx, r13d
    jbe .h_ok
    mov ebp, r13d
    sub ebp, edx
.h_ok:
    test esi, esi
    jle .done
    test ebp, ebp
    jle .done

    mov r13d, [r15 + vbe_pitch - kmain]

.row_loop:
    mov edi, r13d
    imul edi, edx
    add edi, GUI_BACKBUFFER
    lea rdi, [rdi + rcx*4]

    push rcx
    mov ecx, esi
    rep stosd
    pop rcx

    inc edx
    dec ebp
    jnz .row_loop

.done:
    pop rbp
    pop rsi
    pop rdx
    pop rcx
    pop rdi
    pop r13
    pop r12
    pop rbx
    ret

; ==============================================================================
; gui_draw_rect: 1px outline rectangle (ecx=x, edx=y, esi=w, ebp=h, eax=color)
; ==============================================================================
gui_draw_rect:
    push rcx
    push rdx
    push rsi
    push rbp
    push rax

    ; top line (h=1)
    push rbp
    mov ebp, 1
    call gui_fill_rect
    pop rbp

    ; bottom line (y = y + h - 1, h=1)
    push rdx
    push rbp
    lea edx, [rdx + rbp - 1]
    mov ebp, 1
    call gui_fill_rect
    pop rbp
    pop rdx

    ; left line (w=1)
    push rsi
    mov esi, 1
    call gui_fill_rect
    pop rsi

    ; right line (x = x + w - 1, w=1)
    push rcx
    push rsi
    lea ecx, [rcx + rsi - 1]
    mov esi, 1
    call gui_fill_rect
    pop rsi
    pop rcx

    pop rax
    pop rbp
    pop rsi
    pop rdx
    pop rcx
    ret

; ==============================================================================
; gui_blend_pixel: Alpha blend single pixel into backbuffer
; Inputs: ECX = x, EDX = y, EAX = 0x00RRGGBB, EBX = alpha (0..255)
; ==============================================================================
gui_blend_pixel:
    push rcx
    push rdx
    push rdi
    push r8
    push r9
    push r10

    cmp ecx, [r15 + vbe_width - kmain]
    jae .done
    cmp edx, [r15 + vbe_height - kmain]
    jae .done

    mov edi, [r15 + vbe_pitch - kmain]
    imul edi, edx
    add edi, GUI_BACKBUFFER
    lea rdi, [rdi + rcx*4]

    mov r8d, [rdi]                    ; background pixel: 0x00RRGGBB
    mov r10d, 255
    sub r10d, ebx                     ; inv_alpha

    ; Blue
    movzx ecx, al                     ; fg B
    imul ecx, ebx
    movzx edx, r8b                    ; bg B
    imul edx, r10d
    add ecx, edx
    shr ecx, 8
    and ecx, 0xFF
    mov r9d, ecx

    ; Green
    mov ecx, eax
    shr ecx, 8
    and ecx, 0xFF                     ; fg G
    imul ecx, ebx
    mov edx, r8d
    shr edx, 8
    and edx, 0xFF                     ; bg G
    imul edx, r10d
    add ecx, edx
    shr ecx, 8
    and ecx, 0xFF
    shl ecx, 8
    or r9d, ecx

    ; Red
    mov ecx, eax
    shr ecx, 16
    and ecx, 0xFF                     ; fg R
    imul ecx, ebx
    mov edx, r8d
    shr edx, 16
    and edx, 0xFF                     ; bg R
    imul edx, r10d
    add ecx, edx
    shr ecx, 8
    and ecx, 0xFF
    shl ecx, 16
    or r9d, ecx

    mov [rdi], r9d

.done:
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rdx
    pop rcx
    ret

; ==============================================================================
; gui_draw_shadow: Renders multi-layer soft alpha drop shadow around window
; Inputs: ECX = x, EDX = y, ESI = w, EBP = h
; ==============================================================================
gui_draw_shadow:
    push rcx
    push rdx
    push rsi
    push rbp
    push rax
    push rbx
    push r8

    ; Outer layer: 6px offset, alpha ~ 24
    push rcx
    push rdx
    push rsi
    push rbp
    add ecx, 3
    add edx, 3
    add esi, 6
    add ebp, 6
    mov eax, 0x00020408
    mov ebx, 30
    call gui_draw_shadow_box
    pop rbp
    pop rsi
    pop rdx
    pop rcx

    ; Middle layer: 4px offset, alpha ~ 50
    push rcx
    push rdx
    push rsi
    push rbp
    add ecx, 2
    add edx, 2
    add esi, 4
    add ebp, 4
    mov eax, 0x00020408
    mov ebx, 50
    call gui_draw_shadow_box
    pop rbp
    pop rsi
    pop rdx
    pop rcx

    pop r8
    pop rbx
    pop rax
    pop rbp
    pop rsi
    pop rdx
    pop rcx
    ret

gui_draw_shadow_box:
    ; Outline blending box
    ; Top & bottom edges
    push rcx
    push rdx
    push rsi
    push rbp
    xor r8d, r8d
.sh_h:
    push rcx
    push rdx
    add ecx, r8d
    call gui_blend_pixel
    pop rdx
    pop rcx

    push rcx
    push rdx
    add ecx, r8d
    lea edx, [rdx + rbp - 1]
    call gui_blend_pixel
    pop rdx
    pop rcx

    inc r8d
    cmp r8d, esi
    jb .sh_h

    ; Left & right edges
    xor r8d, r8d
.sh_v:
    push rcx
    push rdx
    add edx, r8d
    call gui_blend_pixel
    pop rdx
    pop rcx

    push rcx
    push rdx
    lea ecx, [rcx + rsi - 1]
    add edx, r8d
    call gui_blend_pixel
    pop rdx
    pop rcx

    inc r8d
    cmp r8d, ebp
    jb .sh_v

    pop rbp
    pop rsi
    pop rdx
    pop rcx
    ret

; ==============================================================================
; gui_draw_char: Renders 8x16 glyph to RAM backbuffer
; ==============================================================================
gui_draw_char:
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

    cmp byte [r14 + VBS_OK], 1
    jne .done

    cmp ecx, [r15 + vbe_width - kmain]
    jae .done
    cmp edx, [r15 + vbe_height - kmain]
    jae .done

    movzx eax, al
    shl eax, 4                        ; char * 16
    lea r8, [r15 + font8x16 - kmain + rax]

    mov r11d, [r15 + vbe_pitch - kmain]

    xor r9d, r9d                      ; row = 0..15
.row_loop:
    lea eax, [edx + r9d]
    cmp eax, [r15 + vbe_height - kmain]
    jae .next_row

    mov edi, r11d
    imul edi, eax
    add edi, GUI_BACKBUFFER
    lea rdi, [rdi + rcx*4]

    movzx r10d, byte [r8 + r9]        ; font glyph byte for row
    
    test r10b, 0x80
    jz @f
    mov [rdi], ebx
    jmp .c1
@@: cmp esi, GUI_CLR_NONE
    je .c1
    mov [rdi], esi
.c1:
    test r10b, 0x40
    jz @f
    mov [rdi+4], ebx
    jmp .c2
@@: cmp esi, GUI_CLR_NONE
    je .c2
    mov [rdi+4], esi
.c2:
    test r10b, 0x20
    jz @f
    mov [rdi+8], ebx
    jmp .c3
@@: cmp esi, GUI_CLR_NONE
    je .c3
    mov [rdi+8], esi
.c3:
    test r10b, 0x10
    jz @f
    mov [rdi+12], ebx
    jmp .c4
@@: cmp esi, GUI_CLR_NONE
    je .c4
    mov [rdi+12], esi
.c4:
    test r10b, 0x08
    jz @f
    mov [rdi+16], ebx
    jmp .c5
@@: cmp esi, GUI_CLR_NONE
    je .c5
    mov [rdi+16], esi
.c5:
    test r10b, 0x04
    jz @f
    mov [rdi+20], ebx
    jmp .c6
@@: cmp esi, GUI_CLR_NONE
    je .c6
    mov [rdi+20], esi
.c6:
    test r10b, 0x02
    jz @f
    mov [rdi+24], ebx
    jmp .c7
@@: cmp esi, GUI_CLR_NONE
    je .c7
    mov [rdi+24], esi
.c7:
    test r10b, 0x01
    jz @f
    mov [rdi+28], ebx
    jmp .next_row
@@: cmp esi, GUI_CLR_NONE
    je .next_row
    mov [rdi+28], esi

.next_row:
    inc r9d
    cmp r9d, 16
    jb .row_loop

.done:
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
; gui_draw_str: Renders null-terminated ASCIIZ string
; ==============================================================================
gui_draw_str:
    push rcx
    push rdx
    push rdi
    push rax
    push r8
    mov r8d, ecx                      ; remember start_x
.loop:
    mov al, [rdi]
    inc rdi
    test al, al
    jz .done
    cmp al, 10                        ; newline
    je .newline
    call gui_draw_char
    add ecx, 8
    jmp .loop
.newline:
    mov ecx, r8d
    add edx, 18
    jmp .loop
.done:
    pop r8
    pop rax
    pop rdi
    pop rdx
    pop rcx
    ret

; ==============================================================================
; gui_draw_text: Draw string with transparent background
; Inputs: ECX = x, EDX = y, RDI = str_ptr, EBX = fg
; ==============================================================================
gui_draw_text:
    push rsi
    mov esi, GUI_CLR_NONE
    call gui_draw_str
    pop rsi
    ret

; ==============================================================================
; gui_draw_hex: Draws hexadecimal value (does not clobber ECX)
; Inputs: RAX = value, R8D = num_digits, ECX = x, EDX = y, EBX = fg, ESI = bg
; ==============================================================================
gui_draw_hex:
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11

    mov r11d, ecx                     ; keep x in r11d
    mov r9d, r8d                      ; digits remaining
.loop:
    test r9d, r9d
    jz .done
    dec r9d
    push rcx
    mov cl, r9b
    shl cl, 2                         ; * 4 bits
    mov r10, rax
    shr r10, cl
    pop rcx
    and r10b, 0x0F
    cmp r10b, 9
    jbe .digit
    add r10b, 'A' - 10
    jmp .draw
.digit:
    add r10b, '0'
.draw:
    push rax
    mov al, r10b
    mov ecx, r11d
    call gui_draw_char
    pop rax
    add r11d, 8
    jmp .loop

.done:
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    ret

; ==============================================================================
; gui_draw_badge: Small pill/tag (ecx=x, edx=y, rdi=text, eax=bg, ebx=fg)
; ==============================================================================
gui_draw_badge:
    push rcx
    push rdx
    push rsi
    push rbp
    push rbx
    push rax

    push rdi
    xor esi, esi
.len_loop:
    cmp byte [rdi + rsi], 0
    je .len_done
    inc esi
    jmp .len_loop
.len_done:
    pop rdi

    shl esi, 3                        ; text width = len * 8
    add esi, 12                       ; padding (6px left, 6px right)
    mov ebp, 20                       ; badge height = 20px

    call gui_fill_rect

    pop rax                           ; discard bg
    pop rbx                           ; text color
    push rbx
    push rcx
    push rdx
    add ecx, 6
    add edx, 2
    call gui_draw_text
    pop rdx
    pop rcx
    pop rbx

    pop rbp
    pop rsi
    pop rdx
    pop rcx
    ret

; ==============================================================================
; gui_draw_button: (ecx=x, edx=y, esi=w, ebp=h, rdi=label, eax=bg, ebx=fg)
; ==============================================================================
gui_draw_button:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8

    ; Fill button background
    call gui_fill_rect

    ; Outline border
    mov eax, GUI_CLR_BORDER_HI
    call gui_draw_rect

    ; Measure string length in RDI
    push rdi
    xor eax, eax
.blen:
    cmp byte [rdi + rax], 0
    je .blen_done
    inc eax
    jmp .blen
.blen_done:
    pop rdi
    shl eax, 3                        ; text_w = len * 8

    ; Center text in button:
    mov r8d, esi
    sub r8d, eax
    shr r8d, 1
    add ecx, r8d

    mov r8d, ebp
    sub r8d, 16
    shr r8d, 1
    add edx, r8d

    ; Draw centered label (ecx=x, edx=y, rdi=label, ebx=fg)
    call gui_draw_text

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
; gui_draw_card: Container card (ecx=x, edx=y, esi=w, ebp=h, rdi=title)
; ==============================================================================
gui_draw_card:
    push rcx
    push rdx
    push rsi
    push rbp
    push rdi

    mov eax, GUI_CLR_CARD
    call gui_fill_rect

    mov eax, GUI_CLR_BORDER
    call gui_draw_rect

    test rdi, rdi
    jz .done

    push rcx
    push rdx
    add ecx, 12
    add edx, 8
    mov ebx, GUI_CLR_TEXT_MID
    call gui_draw_text
    pop rdx
    pop rcx

    push rcx
    push rdx
    push rsi
    push rbp
    add ecx, 8
    add edx, 26
    sub esi, 16
    mov ebp, 1
    mov eax, GUI_CLR_BORDER
    call gui_fill_rect
    pop rbp
    pop rsi
    pop rdx
    pop rcx

.done:
    pop rdi
    pop rbp
    pop rsi
    pop rdx
    pop rcx
    ret

; ==============================================================================
; gui_draw_progress_bar: (ecx=x, edx=y, esi=w, ebp=h, eax=percent, ebx=fill_clr)
; ==============================================================================
gui_draw_progress_bar:
    push rcx
    push rdx
    push rsi
    push rbp
    push rax
    push rbx
    push r8

    push rax
    mov eax, GUI_CLR_PANEL
    call gui_fill_rect
    mov eax, GUI_CLR_BORDER
    call gui_draw_rect
    pop rax

    cmp eax, 100
    jbe .p_ok
    mov eax, 100
.p_ok:
    push rdx
    push rcx
    lea ecx, [esi - 4]
    imul eax, ecx
    xor edx, edx
    mov ecx, 100
    div ecx
    mov r8d, eax                      ; fill width
    pop rcx
    pop rdx

    test r8d, r8d
    jle .done

    push rcx
    push rdx
    push rsi
    push rbp
    add ecx, 2
    add edx, 2
    mov esi, r8d
    sub ebp, 4
    mov eax, ebx
    call gui_fill_rect
    pop rbp
    pop rsi
    pop rdx
    pop rcx

.done:
    pop r8
    pop rbx
    pop rax
    pop rbp
    pop rsi
    pop rdx
    pop rcx
    ret

; ==============================================================================
; gui_draw_window_frame: Window frame with drop shadow, traffic dots, and title
; Inputs: ECX = x, EDX = y, ESI = w, EBP = h, RDI = title, AL = is_focused
; ==============================================================================
gui_draw_window_frame:
    push rcx
    push rdx
    push rsi
    push rbp
    push rdi
    push rax

    ; 1. Soft alpha drop shadow
    call gui_draw_shadow

    ; 2. Window body background
    mov eax, GUI_CLR_BG
    call gui_fill_rect

    ; 3. Title bar (height 28)
    push rbp
    mov ebp, 28
    mov eax, GUI_CLR_PANEL
    call gui_fill_rect
    pop rbp

    ; 4. Traffic light control dots
    push rcx
    push rdx
    push rsi
    push rbp
    add ecx, 12
    add edx, 10
    mov esi, 8
    mov ebp, 8
    mov eax, GUI_DOT_CLOSE
    call gui_fill_rect

    add ecx, 14
    mov eax, GUI_DOT_MIN
    call gui_fill_rect

    add ecx, 14
    mov eax, GUI_DOT_MAX
    call gui_fill_rect
    pop rbp
    pop rsi
    pop rdx
    pop rcx

    ; 5. Window title text
    push rcx
    push rdx
    add ecx, 60
    add edx, 6
    mov ebx, GUI_CLR_TEXT_HI
    call gui_draw_text
    pop rdx
    pop rcx

    ; 6. Title bar separator
    push rcx
    push rdx
    push rbp
    add edx, 28
    mov ebp, 1
    mov eax, GUI_CLR_BORDER
    call gui_fill_rect
    pop rbp
    pop rdx
    pop rcx

    ; 7. Window border (accentuated if focused)
    pop rax                           ; is_focused
    test al, al
    jz .unfocused_border
    mov eax, GUI_CLR_ACCENT
    jmp .draw_b
.unfocused_border:
    mov eax, GUI_CLR_BORDER
.draw_b:
    call gui_draw_rect

    pop rdi
    pop rbp
    pop rsi
    pop rdx
    pop rcx
    ret
