; ==============================================================================
; Opensweet OS - High-Resolution Framebuffer Text Console (1024x768 @ 32bpp)
; 128 Columns x 48 Rows, Fast Hardware-Accelerated rep movsq Scrolling,
; Dual-Output Mirroring to COM1 Serial, Active Cursor, and Modern Colors.
; ==============================================================================

include 'D:\Opensweet\gui\font8x16.inc'

FB_CONSOLE_COLS = 128
FB_CONSOLE_ROWS = 48

; Modern Palette Constants
FB_CLR_BG       = 0x000F172A   ; Slate 900
FB_CLR_DEFAULT  = 0x00CBD5E1   ; Slate 300
FB_CLR_PROMPT   = 0x0010B981   ; Emerald 500
FB_CLR_DIR      = 0x0038BDF8   ; Sky Blue 400
FB_CLR_FILE     = 0x00F8FAFC   ; White 50
FB_CLR_SIZE     = 0x00F59E0B   ; Amber 500
FB_CLR_HEADER   = 0x00A855F7   ; Purple 500
FB_CLR_LABEL    = 0x0094A3B8   ; Slate 400
FB_CLR_SUCCESS  = 0x0022C55E   ; Green 500
FB_CLR_ERROR    = 0x00EF4444   ; Red 500
FB_CLR_NUMBER   = 0x00EAB308   ; Yellow 500
FB_CLR_MUTED    = 0x0064748B   ; Slate 500

; ==============================================================================
; fb_console_init: Read VBE parameters from BIOS table & clear screen
; ==============================================================================
fb_console_init:
    push rax

    cmp byte [r14 + VBS_OK], 1
    jne .done

    movzx eax, word [r14 + VBS_PITCH]
    mov [r15 + vbe_pitch - kmain], eax
    movzx eax, word [r14 + VBS_WIDTH]
    mov [r15 + vbe_width - kmain], eax
    movzx eax, word [r14 + VBS_HEIGHT]
    mov [r15 + vbe_height - kmain], eax
    mov eax, dword [r14 + VBS_LFB]
    mov [r15 + vbe_lfb - kmain], eax

    mov dword [r15 + fb_fg_color - kmain], FB_CLR_DEFAULT
    mov dword [r15 + fb_bg_color - kmain], FB_CLR_BG

    call fb_console_clear

.done:
    pop rax
    ret

; ==============================================================================
; fb_console_clear: Clear screen to fb_bg_color and reset cursor
; ==============================================================================
fb_console_clear:
    push rdi
    push rcx
    push rax
    push rdx

    cld
    mov edi, [r15 + vbe_lfb - kmain]
    test edi, edi
    jz .clear_done

    mov eax, [r15 + vbe_width - kmain]
    imul eax, [r15 + vbe_height - kmain]
    mov ecx, eax                      ; total pixels
    mov eax, [r15 + fb_bg_color - kmain]
    rep stosd

.clear_done:
    mov dword [r15 + fb_cur_col - kmain], 0
    mov dword [r15 + fb_cur_row - kmain], 0

    mov al, 1
    call fb_console_draw_cursor

    pop rdx
    pop rax
    pop rcx
    pop rdi
    ret

; ==============================================================================
; fb_console_set_color: Sets current foreground color (EAX = 32-bit color)
; ==============================================================================
fb_console_set_color:
    mov [r15 + fb_fg_color - kmain], eax
    ret

; ==============================================================================
; fb_console_draw_cursor: Draws or erases underline cursor (AL=1 draw, AL=0 erase)
; ==============================================================================
fb_console_draw_cursor:
    push rax
    push rbx
    push rcx
    push rdx
    push rdi
    push rsi

    mov ecx, [r15 + fb_cur_col - kmain]
    mov edx, [r15 + fb_cur_row - kmain]
    cmp ecx, FB_CONSOLE_COLS
    jae .cur_done
    cmp edx, FB_CONSOLE_ROWS
    jae .cur_done

    test al, al
    jz .cur_erase
    mov ebx, [r15 + fb_fg_color - kmain]
    jmp .cur_calc
.cur_erase:
    mov ebx, [r15 + fb_bg_color - kmain]

.cur_calc:
    shl edx, 4                        ; row * 16
    add edx, 14                       ; scanline 14
    mov edi, [r15 + vbe_pitch - kmain]
    imul edi, edx
    add edi, [r15 + vbe_lfb - kmain]
    shl ecx, 3                        ; col * 8
    lea rdi, [rdi + rcx*4]

    ; 2 scanlines of 8 pixels
    mov eax, ebx
    mov [rdi + 0*4], eax
    mov [rdi + 1*4], eax
    mov [rdi + 2*4], eax
    mov [rdi + 3*4], eax
    mov [rdi + 4*4], eax
    mov [rdi + 5*4], eax
    mov [rdi + 6*4], eax
    mov [rdi + 7*4], eax

    mov esi, [r15 + vbe_pitch - kmain]
    add rdi, rsi
    mov [rdi + 0*4], eax
    mov [rdi + 1*4], eax
    mov [rdi + 2*4], eax
    mov [rdi + 3*4], eax
    mov [rdi + 4*4], eax
    mov [rdi + 5*4], eax
    mov [rdi + 6*4], eax
    mov [rdi + 7*4], eax

.cur_done:
    pop rsi
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; ==============================================================================
; fb_console_draw_char_at: Draw 8x16 glyph (AL=char, ECX=col, EDX=row, EBX=fg, ESI=bg)
; ==============================================================================
fb_console_draw_char_at:
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

    cmp ecx, FB_CONSOLE_COLS
    jae .char_done
    cmp edx, FB_CONSOLE_ROWS
    jae .char_done

    ; Y pixel = row * 16
    shl edx, 4
    mov edi, [r15 + vbe_pitch - kmain]
    imul edi, edx
    add edi, [r15 + vbe_lfb - kmain]
    ; X pixel = col * 8
    shl ecx, 3
    lea rdi, [rdi + rcx*4]

    ; Font glyph pointer
    movzx eax, al
    shl eax, 4                        ; char * 16
    lea r8, [r15 + font8x16 - kmain]
    add r8, rax

    mov r11d, [r15 + vbe_pitch - kmain]
    xor r9d, r9d                      ; row 0..15

.row_loop:
    movzx r10d, byte [r8 + r9]        ; bitmask

    mov eax, esi
    test r10b, 0x80
    jz @f
    mov eax, ebx
@@: mov [rdi + 0*4], eax

    mov eax, esi
    test r10b, 0x40
    jz @f
    mov eax, ebx
@@: mov [rdi + 1*4], eax

    mov eax, esi
    test r10b, 0x20
    jz @f
    mov eax, ebx
@@: mov [rdi + 2*4], eax

    mov eax, esi
    test r10b, 0x10
    jz @f
    mov eax, ebx
@@: mov [rdi + 3*4], eax

    mov eax, esi
    test r10b, 0x08
    jz @f
    mov eax, ebx
@@: mov [rdi + 4*4], eax

    mov eax, esi
    test r10b, 0x04
    jz @f
    mov eax, ebx
@@: mov [rdi + 5*4], eax

    mov eax, esi
    test r10b, 0x02
    jz @f
    mov eax, ebx
@@: mov [rdi + 6*4], eax

    mov eax, esi
    test r10b, 0x01
    jz @f
    mov eax, ebx
@@: mov [rdi + 7*4], eax

    add rdi, r11                      ; next scanline
    inc r9d
    cmp r9d, 16
    jb .row_loop

.char_done:
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
; fb_console_scroll: Shift rows 1..47 up by 16 scanlines, clear row 47
; ==============================================================================
fb_console_scroll:
    push rsi
    push rdi
    push rcx
    push rax
    push rdx

    cld
    mov edi, [r15 + vbe_lfb - kmain]
    test edi, edi
    jz .scroll_done

    ; Source = LFB + 16 * pitch
    mov edx, [r15 + vbe_pitch - kmain]
    shl edx, 4                        ; 16 * pitch
    mov rsi, rdi
    add rsi, rdx                      ; rsi = LFB + 16*pitch

    ; Count = (height - 16) * pitch / 8 qwords
    mov eax, [r15 + vbe_height - kmain]
    sub eax, 16
    imul eax, [r15 + vbe_pitch - kmain]
    shr eax, 3                        ; qwords
    mov ecx, eax
    rep movsq

    ; Clear bottom 16 scanlines
    mov edx, [r15 + vbe_pitch - kmain]
    shl edx, 4                        ; 16 * pitch bytes
    shr edx, 2                        ; / 4 dwords
    mov ecx, edx
    mov eax, [r15 + fb_bg_color - kmain]
    rep stosd

.scroll_done:
    mov dword [r15 + fb_cur_row - kmain], FB_CONSOLE_ROWS - 1

    pop rdx
    pop rax
    pop rcx
    pop rdi
    pop rsi
    ret

; ==============================================================================
; fb_console_putc: Print char in AL, handle \r, \n, \b, wrap and scroll
; ==============================================================================
fb_console_putc:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push r12

    mov r12b, al                      ; safely hold input char in r12b

    ; Erase cursor at current position
    xor al, al
    call fb_console_draw_cursor

    mov al, r12b
    cmp al, 13                        ; \r
    je .handle_cr
    cmp al, 10                        ; \n
    je .handle_lf
    cmp al, 8                         ; \b backspace
    je .handle_bs

    ; Printable character
    mov ecx, [r15 + fb_cur_col - kmain]
    mov edx, [r15 + fb_cur_row - kmain]
    mov ebx, [r15 + fb_fg_color - kmain]
    mov esi, [r15 + fb_bg_color - kmain]
    call fb_console_draw_char_at

    inc dword [r15 + fb_cur_col - kmain]
    cmp dword [r15 + fb_cur_col - kmain], FB_CONSOLE_COLS
    jb .finish
    mov dword [r15 + fb_cur_col - kmain], 0
    inc dword [r15 + fb_cur_row - kmain]
    cmp dword [r15 + fb_cur_row - kmain], FB_CONSOLE_ROWS
    jb .finish
    call fb_console_scroll
    jmp .finish

.handle_cr:
    mov dword [r15 + fb_cur_col - kmain], 0
    jmp .finish

.handle_lf:
    mov dword [r15 + fb_cur_col - kmain], 0
    inc dword [r15 + fb_cur_row - kmain]
    cmp dword [r15 + fb_cur_row - kmain], FB_CONSOLE_ROWS
    jb .finish
    call fb_console_scroll
    jmp .finish

.handle_bs:
    mov ecx, [r15 + fb_cur_col - kmain]
    test ecx, ecx
    jz .finish
    dec ecx
    mov [r15 + fb_cur_col - kmain], ecx
    mov edx, [r15 + fb_cur_row - kmain]
    mov al, ' '
    mov ebx, [r15 + fb_bg_color - kmain]
    mov esi, [r15 + fb_bg_color - kmain]
    call fb_console_draw_char_at
    jmp .finish

.finish:
    ; Draw cursor at new position
    mov al, 1
    call fb_console_draw_cursor

    pop r12
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; ==============================================================================
; fb_console_puts: Print null-terminated string at RSI
; ==============================================================================
fb_console_puts:
    push rax
    push rsi
.next_ch:
    lodsb
    test al, al
    jz .puts_done
    call putc
    jmp .next_ch
.puts_done:
    pop rsi
    pop rax
    ret

; Console State Variables
align 16
fb_cur_col   dd 0
fb_cur_row   dd 0
fb_fg_color  dd FB_CLR_DEFAULT
fb_bg_color  dd FB_CLR_BG
