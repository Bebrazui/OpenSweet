; ==============================================================================
; Opensweet OS - PS/2 Mouse Driver (i8042 Auxiliary Device)
; Handles IRQ12, 3-byte packet decoding, screen bounds clamping, and
; flicker-free 16x16 arrow cursor rendering with background save/restore.
; ==============================================================================

MOUSE_DATA_PORT = 0x60
MOUSE_CMD_PORT  = 0x64

; --- Wait for i8042 input buffer empty (safe to write) ---
mouse_wait_write:
    push rcx
    push rax
    mov ecx, 100000
.loop:
    in al, MOUSE_CMD_PORT
    test al, 0x02              ; bit 1: 1 = input buffer full
    jz .ok
    dec ecx
    jnz .loop
.ok:
    pop rax
    pop rcx
    ret

; --- Wait for i8042 output buffer full (safe to read) ---
mouse_wait_read:
    push rcx
    push rax
    mov ecx, 100000
.loop:
    in al, MOUSE_CMD_PORT
    test al, 0x01              ; bit 0: 1 = output buffer has data
    jnz .ok
    dec ecx
    jnz .loop
.ok:
    pop rax
    pop rcx
    ret

; --- Send byte AL to mouse device via 0xD4 command ---
mouse_write:
    push rax
    call mouse_wait_write
    mov al, 0xD4               ; route next byte to aux device (mouse)
    out MOUSE_CMD_PORT, al
    call mouse_wait_write
    pop rax
    out MOUSE_DATA_PORT, al
    ret

; --- Read data byte from mouse into AL ---
mouse_read:
    call mouse_wait_read
    in al, MOUSE_DATA_PORT
    ret

; ==============================================================================
; mouse_init: Initialize PS/2 mouse controller, enable IRQ12 & streaming mode
; ==============================================================================
mouse_init:
    push rax
    push rbx
    push rcx

    ; 1. Enable auxiliary device (mouse port)
    call mouse_wait_write
    mov al, 0xA8
    out MOUSE_CMD_PORT, al

    ; 2. Read Compaq status byte / command byte
    call mouse_wait_write
    mov al, 0x20
    out MOUSE_CMD_PORT, al
    call mouse_read            ; AL = current command byte

    ; Enable IRQ1 (bit 0), IRQ12 (bit 1) and enable both clocks (clear bits 4 and 5)
    or al, 0x03
    and al, not 0x30
    mov bl, al

    ; Write back modified command byte
    call mouse_wait_write
    mov al, 0x60
    out MOUSE_CMD_PORT, al
    call mouse_wait_write
    mov al, bl
    out MOUSE_DATA_PORT, al

    ; 3. Set mouse defaults (command 0xF6)
    mov al, 0xF6
    call mouse_write
    call mouse_read            ; ACK (0xFA)

    ; 3b. Set sample rate to 200 Hz (command 0xF3, arg 200)
    mov al, 0xF3
    call mouse_write
    call mouse_read            ; ACK (0xFA)
    mov al, 200
    call mouse_write
    call mouse_read            ; ACK (0xFA)

    ; 3c. Set resolution to maximum 8 counts/mm (command 0xE8, arg 3)
    mov al, 0xE8
    call mouse_write
    call mouse_read            ; ACK (0xFA)
    mov al, 3
    call mouse_write
    call mouse_read            ; ACK (0xFA)

    ; 3d. Set scaling 1:1 (command 0xE6)
    mov al, 0xE6
    call mouse_write
    call mouse_read            ; ACK (0xFA)

    ; 4. Enable data reporting / streaming (command 0xF4)
    mov al, 0xF4
    call mouse_write
    call mouse_read            ; ACK (0xFA)

    ; 5. Drain any residual bytes in output buffer
    in al, MOUSE_CMD_PORT
    test al, 0x01
    jz .drained
    in al, MOUSE_DATA_PORT
.drained:

    ; 6. Set initial state: center of screen (512, 384)
    mov dword [r15 + mouse_x - kmain], 512
    mov dword [r15 + mouse_y - kmain], 384
    mov dword [r15 + mouse_prev_x - kmain], 512
    mov dword [r15 + mouse_prev_y - kmain], 384
    mov byte [r15 + mouse_buttons - kmain], 0
    mov byte [r15 + mouse_cycle - kmain], 0
    mov byte [r15 + mouse_bg_valid - kmain], 0

    ; 7. Initial cursor draw
    call mouse_update_cursor

    pop rcx
    pop rbx
    pop rax
    ret

; ==============================================================================
; mouse_irq: IRQ12 handler called from common_irq
; Reads 1 byte, updates 3-byte packet state machine
; ==============================================================================
mouse_irq:
    push rax
    push rbx
    push rcx
    push rdx
    push rdi
    push rsi
    push r8

    ; Verify byte is from mouse (bit 5 in status port 0x64)
    in al, MOUSE_CMD_PORT
    test al, 0x20
    jz .done
    test al, 0x01
    jz .done
    in al, MOUSE_DATA_PORT     ; AL = packet byte

    movzx ecx, byte [r15 + mouse_cycle - kmain]
    cmp ecx, 0
    je .byte0
    cmp ecx, 1
    je .byte1
    cmp ecx, 2
    je .byte2

    ; Invalid cycle, reset
    mov byte [r15 + mouse_cycle - kmain], 0
    jmp .done

.byte0:
    ; Bit 3 of byte 0 in standard PS/2 mouse packet is ALWAYS 1.
    ; If not, packets are out of sync -> discard byte and stay at cycle 0.
    test al, 0x08
    jz .done
    mov [r15 + mouse_packet - kmain], al
    mov byte [r15 + mouse_cycle - kmain], 1
    jmp .done

.byte1:
    mov [r15 + mouse_packet - kmain + 1], al
    mov byte [r15 + mouse_cycle - kmain], 2
    jmp .done

.byte2:
    mov [r15 + mouse_packet - kmain + 2], al
    mov byte [r15 + mouse_cycle - kmain], 0

    ; Full 3-byte packet received! Decode it:
    call mouse_process_packet

.done:
    pop r8
    pop rsi
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; ==============================================================================
; mouse_process_packet: Decode buttons and dx, dy; clamp to screen bounds
; ==============================================================================
mouse_process_packet:
    push rax
    push rbx
    push rcx
    push rdx
    push r8

    ; 1. Buttons (bits 0..2: Left, Right, Middle)
    mov al, [r15 + mouse_packet - kmain]
    and al, 0x07
    mov [r15 + mouse_buttons - kmain], al

    ; 2. Delta X (byte 1, signed via bit 4 of byte 0)
    movzx eax, byte [r15 + mouse_packet - kmain + 1]
    test byte [r15 + mouse_packet - kmain], 0x10   ; X sign bit
    jz .dx_pos
    or eax, 0xFFFFFF00                             ; sign-extend negative
.dx_pos:

    ; 3. Delta Y (byte 2, signed via bit 5 of byte 0)
    movzx edx, byte [r15 + mouse_packet - kmain + 2]
    test byte [r15 + mouse_packet - kmain], 0x20   ; Y sign bit
    jz .dy_pos
    or edx, 0xFFFFFF00                             ; sign-extend negative
.dy_pos:

    ; 4. Update and clamp mouse_x: [0, vbe_width - 1]
    mov ecx, [r15 + mouse_x - kmain]
    add ecx, eax                                   ; x + dx
    test ecx, ecx
    jns .cx_ge0
    xor ecx, ecx
.cx_ge0:
    mov r8d, [r15 + vbe_width - kmain]
    test r8d, r8d
    jz .x_saved
    dec r8d
    cmp ecx, r8d
    jle .cx_le_max
    mov ecx, r8d
.cx_le_max:
.x_saved:
    mov [r15 + mouse_x - kmain], ecx

    ; 5. Update and clamp mouse_y: [0, vbe_height - 1]
    ; In PS/2, positive dy is UP, but screen Y goes DOWN -> sub dy!
    mov ecx, [r15 + mouse_y - kmain]
    sub ecx, edx                                   ; y - dy
    test ecx, ecx
    jns .cy_ge0
    xor ecx, ecx
.cy_ge0:
    mov r8d, [r15 + vbe_height - kmain]
    test r8d, r8d
    jz .y_saved
    dec r8d
    cmp ecx, r8d
    jle .cy_le_max
    mov ecx, r8d
.cy_le_max:
.y_saved:
    mov [r15 + mouse_y - kmain], ecx

    ; 6. Coordinates updated in mouse_x, mouse_y
    pop r8
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; ==============================================================================
; mouse_update_cursor: Flicker-free 16x16 cursor render with background buffer
; ==============================================================================
mouse_update_cursor:
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

    ; Verify VBE mode active
    cmp byte [r14 + VBS_OK], 1
    jne .done

    mov r12d, [r15 + vbe_lfb - kmain]
    mov r13d, [r15 + vbe_pitch - kmain]
    test r12d, r12d
    jz .done

    ; --------------------------------------------------------------------------
    ; Step 1: Restore previous background if valid
    ; --------------------------------------------------------------------------
    cmp byte [r15 + mouse_bg_valid - kmain], 1
    jne .save_new

    mov ebx, [r15 + mouse_prev_x - kmain]          ; prev_x
    mov edx, [r15 + mouse_prev_y - kmain]          ; prev_y

    xor r8d, r8d                                   ; row = 0..15
.restore_row:
    lea eax, [edx + r8d]
    cmp eax, [r15 + vbe_height - kmain]
    jae .restore_next_row

    ; dst = lfb + (prev_y + row) * pitch + prev_x * 4
    mov edi, r13d
    imul edi, eax
    add edi, r12d
    lea rdi, [rdi + rbx*4]

    ; src = mouse_cursor_bg + row * 16 * 4
    mov esi, r8d
    shl esi, 6                                     ; row * 64 bytes
    lea rsi, [r15 + mouse_cursor_bg - kmain + rsi]

    ; copy up to 16 dwords (clipped by screen width)
    mov ecx, 16
    lea eax, [ebx + 16]
    cmp eax, [r15 + vbe_width - kmain]
    jbe .r_full
    mov ecx, [r15 + vbe_width - kmain]
    sub ecx, ebx
    test ecx, ecx
    jle .restore_next_row
.r_full:
    rep movsd

.restore_next_row:
    inc r8d
    cmp r8d, 16
    jb .restore_row

    ; --------------------------------------------------------------------------
    ; Step 2: Save background at new (mouse_x, mouse_y)
    ; --------------------------------------------------------------------------
.save_new:
    mov ebx, [r15 + mouse_x - kmain]               ; new_x
    mov edx, [r15 + mouse_y - kmain]               ; new_y

    xor r8d, r8d                                   ; row = 0..15
.save_row:
    lea eax, [edx + r8d]
    cmp eax, [r15 + vbe_height - kmain]
    jae .save_next_row

    ; src = lfb + (new_y + row) * pitch + new_x * 4
    mov esi, r13d
    imul esi, eax
    add esi, r12d
    lea rsi, [rsi + rbx*4]

    ; dst = mouse_cursor_bg + row * 16 * 4
    mov edi, r8d
    shl edi, 6
    lea rdi, [r15 + mouse_cursor_bg - kmain + rdi]

    mov ecx, 16
    lea eax, [ebx + 16]
    cmp eax, [r15 + vbe_width - kmain]
    jbe .s_full
    mov ecx, [r15 + vbe_width - kmain]
    sub ecx, ebx
    test ecx, ecx
    jle .save_next_row
.s_full:
    rep movsd

.save_next_row:
    inc r8d
    cmp r8d, 16
    jb .save_row

    mov byte [r15 + mouse_bg_valid - kmain], 1

    ; --------------------------------------------------------------------------
    ; Step 3: Draw cursor sprite at (mouse_x, mouse_y)
    ; --------------------------------------------------------------------------
    mov ebx, [r15 + mouse_x - kmain]
    mov edx, [r15 + mouse_y - kmain]

    xor r8d, r8d                                   ; row = 0..15
.draw_row:
    lea eax, [edx + r8d]
    cmp eax, [r15 + vbe_height - kmain]
    jae .draw_next_row

    ; destination scanline pointer
    mov edi, r13d
    imul edi, eax
    add edi, r12d
    lea rdi, [rdi + rbx*4]

    ; load outline and body masks for this row
    movzx r9d, word [r15 + mouse_mask_outline - kmain + r8*2]
    movzx r10d, word [r15 + mouse_mask_body - kmain + r8*2]

    xor ecx, ecx                                   ; col = 0..15
.draw_col:
    lea eax, [ebx + ecx]
    cmp eax, [r15 + vbe_width - kmain]
    jae .draw_next_row

    ; test outline bit (bit 15 - col)
    mov r11d, 15
    sub r11d, ecx
    bt r9d, r11d
    jnc .skip_pixel

    ; bit is set: determine body (white) vs outline (black)
    bt r10d, r11d
    jnc .pixel_outline
    mov dword [rdi + rcx*4], 0x00FFFFFF            ; white body
    jmp .skip_pixel
.pixel_outline:
    mov dword [rdi + rcx*4], 0x000F172A            ; dark slate outline

.skip_pixel:
    inc ecx
    cmp ecx, 16
    jb .draw_col

.draw_next_row:
    inc r8d
    cmp r8d, 16
    jb .draw_row

    ; --------------------------------------------------------------------------
    ; Step 4: Record previous position
    ; --------------------------------------------------------------------------
    mov eax, [r15 + mouse_x - kmain]
    mov [r15 + mouse_prev_x - kmain], eax
    mov eax, [r15 + mouse_y - kmain]
    mov [r15 + mouse_prev_y - kmain], eax

.done:
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
; Mouse Data & Cursor Sprite Masks (16x16)
; ==============================================================================
align 16
mouse_x        dd 400
mouse_y        dd 300
mouse_prev_x   dd 400
mouse_prev_y   dd 300
mouse_buttons  db 0
mouse_cycle    db 0
mouse_bg_valid db 0
mouse_packet   rb 4

align 16
; 16 rows of 16-bit masks: bit 15 is col 0, bit 0 is col 15
mouse_mask_outline:
    dw 0x8000 ; 1000 0000 0000 0000
    dw 0xC000 ; 1100 0000 0000 0000
    dw 0xE000 ; 1110 0000 0000 0000
    dw 0xF000 ; 1111 0000 0000 0000
    dw 0xF800 ; 1111 1000 0000 0000
    dw 0xFC00 ; 1111 1100 0000 0000
    dw 0xFE00 ; 1111 1110 0000 0000
    dw 0xFF00 ; 1111 1111 0000 0000
    dw 0xFF80 ; 1111 1111 1000 0000
    dw 0xFFC0 ; 1111 1111 1100 0000
    dw 0xFFE0 ; 1111 1111 1110 0000
    dw 0xFE00 ; 1111 1110 0000 0000
    dw 0xEF00 ; 1110 1111 0000 0000
    dw 0xCF00 ; 1100 1111 0000 0000
    dw 0x8780 ; 1000 0111 1000 0000
    dw 0x0780 ; 0000 0111 1000 0000

mouse_mask_body:
    dw 0x0000 ; 0000 0000 0000 0000
    dw 0x0000 ; 0000 0000 0000 0000
    dw 0x4000 ; 0100 0000 0000 0000
    dw 0x6000 ; 0110 0000 0000 0000
    dw 0x7000 ; 0111 0000 0000 0000
    dw 0x7800 ; 0111 1000 0000 0000
    dw 0x7C00 ; 0111 1100 0000 0000
    dw 0x7E00 ; 0111 1110 0000 0000
    dw 0x7F00 ; 0111 1111 0000 0000
    dw 0x7F80 ; 0111 1111 1000 0000
    dw 0x7C00 ; 0111 1100 0000 0000
    dw 0x6600 ; 0110 0110 0000 0000
    dw 0x4600 ; 0100 0110 0000 0000
    dw 0x0600 ; 0000 0110 0000 0000
    dw 0x0300 ; 0000 0011 0000 0000
    dw 0x0300 ; 0000 0011 0000 0000

align 16
mouse_cursor_bg rd 16*16                       ; 256 dwords (1024 bytes)
