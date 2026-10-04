; ==============================================================================
; Opensweet OS - High-Performance QOI Wallpaper Decoder (gui/qoi.inc)
; Decodes standard Quite OK Image (.qoi) wallpapers directly from ext4 storage
; into the 8MB R2D_WALLPAPER_CACHE (0x02800000) in <3ms.
; ==============================================================================

QOI_FILE_BUF     = 0x03800000       ; 4 MB temporary disk read buffer
QOI_MAX_READ     = 0x00400000       ; 4 MB max read from ext4
str_wallpaper_qoi db "/wallpaper.qoi", 0

; ==============================================================================
; qoi_load_wallpaper_from_ext4: Read and decode /wallpaper.qoi from ext4
; Outputs: CF = 0 on success, 1 on error
; ==============================================================================
qoi_load_wallpaper_from_ext4:
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

    mov rdi, QOI_FILE_BUF
    mov rcx, QOI_MAX_READ
    lea rsi, [r15 + str_wallpaper_qoi - kmain]
    call ext4_read_file_to_mem
    jc .qoi_err
    cmp rax, 22                       ; Must be at least 14-byte header + 8-byte end
    jb .qoi_err

    ; RSI = source buffer, RDX = file length
    mov rsi, QOI_FILE_BUF
    mov rdx, rax
    mov rdi, R2D_WALLPAPER_CACHE
    call qoi_decode_to_cache
    jc .qoi_err

    clc
    jmp .qoi_done

.qoi_err:
    stc

.qoi_done:
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
    ret

; ==============================================================================
; qoi_decode_to_cache: Decode QOI stream directly to 32bpp ARGB cache
; Inputs: RSI = QOI buffer, RDX = QOI file size, RDI = destination buffer
; Outputs: CF = 0 on success, 1 on error
; ==============================================================================
qoi_decode_to_cache:
    push rbx
    push r12
    push r13
    push r14
    push rbp
    sub rsp, 256                      ; 64 dwords index table

    ; Check magic 'qoif' (0x66696F71)
    cmp dword [rsi], 0x66696F71
    jne .dec_err

    ; Check width == 1920
    mov eax, [rsi + 4]
    bswap eax
    cmp eax, R2D_SCREEN_W
    jne .dec_err

    ; Check height == 1080
    mov eax, [rsi + 8]
    bswap eax
    cmp eax, R2D_SCREEN_H
    jne .dec_err

    ; Total pixels = 1920 * 1080 = 2,073,600
    mov r12d, R2D_SCREEN_W * R2D_SCREEN_H

    ; Setup index table at [rsp], initialize to 0xFF000000
    lea rbx, [rsp]
    mov ecx, 64
    mov eax, 0xFF000000
.init_idx:
    mov [rbx + rcx*4 - 4], eax
    dec ecx
    jnz .init_idx

    ; End of input = rsi + rdx - 8 (skip 8-byte QOI end marker)
    lea r14, [rsi + rdx - 8]
    add rsi, 14                       ; Skip 14-byte header

    ; Initialize current color: r=0, g=0, b=0, a=255
    xor r8d, r8d                      ; r8b = r
    xor r9d, r9d                      ; r9b = g
    xor r10d, r10d                    ; r10b = b
    mov r11d, 255                     ; r11b = a

.loop:
    test r12d, r12d
    jle .dec_ok
    cmp rsi, r14
    jae .dec_err                      ; Ran out of input before all pixels decoded

    lodsb                             ; al = tag byte
    cmp al, 0xFE
    je .op_rgb
    cmp al, 0xFF
    je .op_rgba

    mov cl, al
    shr cl, 6
    jz .op_index
    cmp cl, 1
    je .op_diff
    cmp cl, 2
    je .op_luma

    ; cl == 3: QOI_OP_RUN
    and eax, 0x3F
    inc eax                           ; run = 1..62
    cmp eax, r12d
    jbe @f
    mov eax, r12d
@@:
    sub r12d, eax
    ; Current pixel in ARGB: (a << 24) | (r << 16) | (g << 8) | b
    mov edx, r11d
    shl edx, 8
    mov dl, r8b
    shl edx, 8
    mov dl, r9b
    shl edx, 8
    mov dl, r10b
    mov ecx, eax
    mov eax, edx
    cld
    rep stosd
    jmp .loop

.op_rgb:
    mov r8b, [rsi]
    mov r9b, [rsi + 1]
    mov r10b, [rsi + 2]
    add rsi, 3
    jmp .store_and_index

.op_rgba:
    mov r8b, [rsi]
    mov r9b, [rsi + 1]
    mov r10b, [rsi + 2]
    mov r11b, [rsi + 3]
    add rsi, 4
    jmp .store_and_index

.op_index:
    and eax, 0x3F
    mov eax, [rbx + rax*4]
    mov [rdi], eax
    add rdi, 4
    dec r12d

    ; Unpack r, g, b, a from loaded ARGB
    mov r10b, al
    shr eax, 8
    mov r9b, al
    shr eax, 8
    mov r8b, al
    shr eax, 8
    mov r11b, al
    jmp .loop

.op_diff:
    mov cl, al
    shr cl, 4
    and cl, 3
    sub cl, 2
    add r8b, cl

    mov cl, al
    shr cl, 2
    and cl, 3
    sub cl, 2
    add r9b, cl

    and al, 3
    sub al, 2
    add r10b, al
    jmp .store_and_index

.op_luma:
    and al, 0x3F
    sub al, 32                        ; al = dg (-32..31)
    mov dl, al                        ; dl = dg
    lodsb                             ; al = b2
    mov cl, al
    shr cl, 4
    sub cl, 8                         ; cl = dr_dg (-8..7)
    and al, 0x0F
    sub al, 8                         ; al = db_dg (-8..7)

    add cl, dl                        ; cl = dg + dr_dg
    add r8b, cl

    add r9b, dl                       ; g += dg

    add al, dl                        ; al = dg + db_dg
    add r10b, al
    jmp .store_and_index

.store_and_index:
    mov eax, r11d                     ; a
    shl eax, 8
    mov al, r8b                       ; r
    shl eax, 8
    mov al, r9b                       ; g
    shl eax, 8
    mov al, r10b                      ; b

    mov [rdi], eax
    add rdi, 4
    dec r12d

    ; Hash: (r * 3 + g * 5 + b * 7 + a * 11) % 64
    movzx ecx, r8b
    lea ecx, [rcx + rcx*2]            ; r * 3
    movzx edx, r9b
    imul edx, 5
    add ecx, edx
    movzx edx, r10b
    imul edx, 7
    add ecx, edx
    movzx edx, r11b
    imul edx, 11
    add ecx, edx
    and ecx, 63

    mov [rbx + rcx*4], eax
    jmp .loop

.dec_ok:
    add rsp, 256
    pop rbp
    pop r14
    pop r13
    pop r12
    pop rbx
    clc
    ret

.dec_err:
    add rsp, 256
    pop rbp
    pop r14
    pop r13
    pop r12
    pop rbx
    stc
    ret
