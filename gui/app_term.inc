; ==============================================================================
; Opensweet OS - Terminal Window Module (app_term.inc)
; Real interactive x86_64 kernel shell terminal content renderer.
; ==============================================================================

; ==============================================================================
; app_term_render: Render terminal lines and active block cursor
; ECX = wx, EDX = wy, ESI = ww, EDI = wh, EAX = is_focused
; ==============================================================================
app_term_render:
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

    mov r8d, ecx                        ; wx
    mov r9d, edx                        ; wy
    mov r12d, eax                       ; is_focused

    ; Terminal lines text
    xor r10d, r10d                      ; row index 0..MD_TERM_ROWS-1
.draw_t_lines:
    cmp r10d, MD_TERM_ROWS
    jae .done_t_lines

    imul eax, r10d, 72
    lea rbx, [r15 + md_term_lines - kmain + rax]
    mov edx, [rbx]                      ; color dword
    test edx, edx
    jnz @f
    mov edx, 0xFFCBD5E1                 ; default slate
@@:
    mov eax, edx
    mov ecx, r8d
    add ecx, 16                         ; left text padding
    mov edx, r9d
    add edx, 44                         ; top text padding
    mov esi, r10d
    imul esi, 20                        ; 20px line height
    add edx, esi

    lea rsi, [rbx + 4]
    cmp byte [rsi], 0
    je .skip_line
    call r2d_draw_text_mono_aa

.skip_line:
    inc r10d
    jmp .draw_t_lines

.done_t_lines:
    ; Draw terminal block cursor
    mov ecx, r8d
    add ecx, 16                         ; left text padding
    mov eax, [r15 + md_term_cur_col - kmain]
    imul eax, FONT_MONO_AA_W
    add ecx, eax
    mov edx, r9d
    add edx, 44                         ; top text padding
    mov eax, [r15 + md_term_cur_row - kmain]
    imul eax, 20                        ; line height
    add edx, eax
    mov esi, 8
    mov edi, 15
    mov ebx, 2

    test r12d, r12d
    jz .muted_cursor
    mov eax, 0xC038BDF8                 ; Sky Blue cursor block when focused
    jmp .draw_cursor
.muted_cursor:
    mov eax, 0x6094A3B8                 ; Muted gray cursor when unfocused
.draw_cursor:
    call r2d_draw_rounded_rect_aa

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
