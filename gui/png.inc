; ==============================================================================
; Opensweet OS - High-Performance PNG Wallpaper Decoder (gui/png.inc)
; Decodes standard TrueColor PNG wallpapers directly from ext4 filesystem
; into the 4MB R2D_WALLPAPER_CACHE (0x02400000).
; Supports RFC 1950 (Zlib), RFC 1951 (Deflate), and RFC 2083 (PNG filters 0..4).
; ==============================================================================

PNG_FILE_BUF    = 0x03800000       ; 7 MB temporary disk read buffer (no collision with 8MB R2D_WIN_CACHE @ 0x03000000)
PNG_IDAT_BUF    = 0x03F00000       ; 7 MB concatenated IDAT zlib payload
PNG_RAW_BUF     = 0x04600000       ; 7 MB decompressed scanlines (1080 * 5761 = 6,221,880 bytes)
PNG_MAX_READ    = 0x00700000       ; 7 MB max read from ext4

PNG_IMG_W       = 1920
PNG_IMG_H       = 1080
PNG_LINE_RGB    = PNG_IMG_W * 3               ; 5760 bytes
PNG_LINE_RAW    = PNG_LINE_RGB + 1            ; 5761 bytes
PNG_TOTAL_RAW   = PNG_LINE_RAW * PNG_IMG_H   ; 6,221,880 bytes
PNG_CACHE_PITCH = PNG_IMG_W * 4               ; 7680 bytes

str_default_wallpaper db "/wallpaper.png", 0

; ==============================================================================
; png_load_wallpaper_from_ext4: Load and decode wallpaper from ext4
; RSI = file path (ASCIIZ)
; Outputs: CF = 0 on success, 1 on error
; ==============================================================================
png_load_wallpaper_from_ext4:
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

    mov rdi, PNG_FILE_BUF
    mov rcx, PNG_MAX_READ
    call ext4_read_file_to_mem
    jc .png_load_err
    cmp rax, 64
    jb .png_load_err

    ; RAX = file size in bytes
    mov rsi, PNG_FILE_BUF
    mov rdx, rax
    call png_decode_wallpaper
    jc .png_load_err

    clc
.png_load_done:
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

.png_load_err:
    stc
    jmp .png_load_done

; ==============================================================================
; png_decode_wallpaper: Parse PNG chunks, decompress IDAT, unfilter scanlines
; RSI = buffer with PNG file data, RDX = file size in bytes
; Outputs: CF = 0 on success, 1 on error
; ==============================================================================
png_decode_wallpaper:
    push rbp
    mov rbp, rsp
    sub rsp, 32                         ; local scratch vars

    ; 1. Check PNG Signature (8 bytes: 89 50 4E 47 0D 0A 1A 0A)
    cmp rdx, 32
    jb .dec_err
    mov rax, 0x0A1A0A0D474E5089
    cmp qword [rsi], rax
    jne .dec_err

    ; Setup chunk parse pointers
    lea r12, [rsi + 8]                  ; r12 = current file pointer
    lea r13, [rsi + rdx]                ; r13 = end of file pointer
    mov r14, PNG_IDAT_BUF               ; r14 = IDAT concat pointer

.chunk_loop:
    lea rax, [r12 + 8]
    cmp rax, r13
    ja .chunks_done

    ; Read 4-byte big-endian chunk length
    mov eax, [r12]
    bswap eax
    mov ebx, eax                        ; ebx = chunk_length

    ; Read 4-byte chunk type
    mov edx, [r12 + 4]                  ; edx = chunk type

    add r12, 8                          ; r12 now points to chunk data

    ; Bounds check: r12 + ebx + 4 (crc) <= r13
    lea rax, [r12 + rbx + 4]
    cmp rax, r13
    ja .dec_err

    ; Check chunk type
    cmp edx, 0x52444849                 ; 'IHDR' in little-endian
    je .handle_ihdr

    cmp edx, 0x54414449                 ; 'IDAT' in little-endian
    je .handle_idat

    cmp edx, 0x444E4549                 ; 'IEND' in little-endian
    je .chunks_done

    jmp .next_chunk

.handle_ihdr:
    ; Verify 1024x768, 8-bit depth, RGB or RGBA, deflate
    cmp ebx, 13
    jb .dec_err

    mov eax, [r12]                      ; width
    bswap eax
    cmp eax, PNG_IMG_W
    jne .dec_err

    mov eax, [r12 + 4]                  ; height
    bswap eax
    cmp eax, PNG_IMG_H
    jne .dec_err

    cmp byte [r12 + 8], 8               ; bit depth = 8
    jne .dec_err

    ; Color type: 2 (TrueColor RGB) or 6 (RGBA)
    mov al, byte [r12 + 9]
    cmp al, 2
    je @f
    cmp al, 6
    jne .dec_err
@@:
    cmp byte [r12 + 10], 0              ; compression method = 0 (deflate)
    jne .dec_err
    jmp .next_chunk

.handle_idat:
    ; Concatenate IDAT chunk payload into PNG_IDAT_BUF
    test ebx, ebx
    jz .next_chunk

    push rsi
    push rdi
    push rcx
    mov rdi, r14
    mov rsi, r12
    mov ecx, ebx
    rep movsb
    mov r14, rdi                        ; update write pointer
    pop rcx
    pop rdi
    pop rsi

.next_chunk:
    add r12, rbx                        ; skip data
    add r12, 4                          ; skip 4-byte CRC
    jmp .chunk_loop

.chunks_done:
    ; Check total concatenated IDAT size
    mov rax, r14
    sub rax, PNG_IDAT_BUF
    cmp rax, 32
    jb .dec_err

    ; 2. Deflate Decompression of concatenated zlib stream in PNG_IDAT_BUF
    mov rsi, PNG_IDAT_BUF
    mov r8, r14                         ; r8 = end of IDAT stream
    mov rdi, PNG_RAW_BUF                ; rdi = destination for scanlines

    ; Check Zlib header (2 bytes)
    movzx eax, byte [rsi]
    and al, 0x0F
    cmp al, 8                           ; Compression Method 8 = Deflate
    jne .dec_err
    add rsi, 2                          ; skip CMF and FLG

.deflate_block_loop:
    cmp rsi, r8
    jae .dec_err

    ; Read 1-byte block header
    lodsb                               ; al = BFINAL(bit 0) | BTYPE(bits 1..2)
    movzx ebx, al
    and ebx, 1                          ; ebx = BFINAL (1 = last block)
    shr al, 1
    and al, 3                           ; al = BTYPE
    cmp al, 0                           ; BTYPE 0 = Uncompressed / Stored block
    jne .dec_err

    ; Stored Block: next 2 bytes = LEN, next 2 bytes = NLEN
    lea rax, [rsi + 4]
    cmp rax, r8
    ja .dec_err

    movzx ecx, word [rsi]               ; ecx = LEN
    movzx edx, word [rsi + 2]           ; edx = NLEN
    add rsi, 4

    ; Verify LEN ^ NLEN == 0xFFFF
    mov eax, ecx
    xor eax, edx
    cmp eax, 0xFFFF
    jne .dec_err

    ; Copy LEN uncompressed bytes directly to PNG_RAW_BUF
    test ecx, ecx
    jz .check_bfinal

    lea rax, [rsi + rcx]
    cmp rax, r8
    ja .dec_err

    push rcx
    rep movsb
    pop rcx

.check_bfinal:
    test ebx, ebx
    jz .deflate_block_loop

    ; Verify decompressed scanline buffer size: must be >= PNG_TOTAL_RAW
    mov rax, rdi
    sub rax, PNG_RAW_BUF
    cmp rax, PNG_TOTAL_RAW
    jb .dec_err

    ; 3. Scanline Unfiltering & ARGB conversion into R2D_WALLPAPER_CACHE
    ; Total 1080 scanlines, each scanline has 1 filter byte + 5760 RGB bytes = 5761 bytes
    xor r8d, r8d                        ; r8d = y (0..PNG_IMG_H-1)

.unfilter_y_loop:
    imul rax, r8, PNG_LINE_RAW
    lea rsi, [PNG_RAW_BUF + rax]        ; rsi points to filter byte
    movzx r12d, byte [rsi]              ; r12d = filter type (0..4)
    inc rsi                             ; rsi points to scanline byte 0

    ; Setup prior scanline pointer in rdx (NULL for y == 0)
    test r8d, r8d
    jz .no_prior_line
    lea rdx, [rsi - PNG_LINE_RAW]       ; rdx points to previous scanline (already unfiltered in-place!)
    jmp .have_prior_line
.no_prior_line:
    xor edx, edx
.have_prior_line:

    ; Unfilter scanline in-place: byte indices 0..PNG_LINE_RGB-1
    xor r9d, r9d                        ; r9d = byte index x (0..PNG_LINE_RGB-1)
.unfilter_x_loop:
    ; a = (x >= 3) ? rsi[x - 3] : 0
    xor ecx, ecx
    cmp r9d, 3
    jb @f
    movzx ecx, byte [rsi + r9 - 3]
@@:
    ; b = (prior != NULL) ? prior[x] : 0
    ; c = (prior != NULL && x >= 3) ? prior[x - 3] : 0
    xor ebx, ebx                        ; ebx = b
    xor eax, eax                        ; eax = c
    test rdx, rdx
    jz .have_abc
    movzx ebx, byte [rdx + r9]
    cmp r9d, 3
    jb .have_abc
    movzx eax, byte [rdx + r9 - 3]
.have_abc:
    ; ecx = a, ebx = b, eax = c

    ; Dispatch filter type (r12d)
    test r12d, r12d
    jz .filter_done                     ; Filter 0: None

    cmp r12d, 1
    je .filter_sub                      ; Filter 1: Sub (byte + a)

    cmp r12d, 2
    je .filter_up                       ; Filter 2: Up (byte + b)

    cmp r12d, 3
    je .filter_avg                      ; Filter 3: Average (byte + ((a + b) >> 1))

    cmp r12d, 4
    je .filter_paeth                    ; Filter 4: Paeth (byte + paeth(a, b, c))

    jmp .dec_err                        ; Invalid filter type

.filter_sub:
    add byte [rsi + r9], cl
    jmp .filter_done

.filter_up:
    add byte [rsi + r9], bl
    jmp .filter_done

.filter_avg:
    add ecx, ebx
    shr ecx, 1
    add byte [rsi + r9], cl
    jmp .filter_done

.filter_paeth:
    ; Calculate PaethPredictor(a=ecx, b=ebx, c=eax)
    push rsi
    push rdx
    mov edx, ebx                        ; edx = b
    mov esi, eax                        ; esi = c
    call png_calc_paeth                 ; returns predictor in al
    pop rdx
    pop rsi
    add byte [rsi + r9], al

.filter_done:
    inc r9d
    cmp r9d, PNG_LINE_RGB
    jb .unfilter_x_loop

    ; 4. Convert unfiltered 24bpp RGB scanline to 32bpp ARGB in R2D_WALLPAPER_CACHE
    imul rax, r8, PNG_CACHE_PITCH       ; y * 1920 * 4
    lea rdi, [R2D_WALLPAPER_CACHE + rax]
    xor ecx, ecx                        ; ecx = pixel col (0..PNG_IMG_W-1)

.to_argb_loop:
    lea rax, [rcx + rcx*2]              ; rax = col * 3
    movzx r10d, byte [rsi + rax]        ; r
    movzx r11d, byte [rsi + rax + 1]    ; g
    movzx eax, byte [rsi + rax + 2]     ; b
    shl r10d, 16
    shl r11d, 8
    or eax, r10d
    or eax, r11d
    or eax, 0xFF000000                  ; Opaque 0xFFRRGGBB
    mov [rdi + rcx*4], eax
    inc ecx
    cmp ecx, PNG_IMG_W
    jb .to_argb_loop

    ; Next scanline y
    inc r8d
    cmp r8d, PNG_IMG_H
    jb .unfilter_y_loop

    ; Success!
    mov rsp, rbp
    pop rbp
    clc
    ret

.dec_err:
    mov rsp, rbp
    pop rbp
    stc
    ret

; ==============================================================================
; png_calc_paeth: Computes Paeth predictor
; Inputs: ECX = a, EDX = b, ESI = c
; Output: AL = predictor
; Preserves: rcx, rdx, rsi, r8, r9
; ==============================================================================
png_calc_paeth:
    ; p = a + b - c
    mov eax, ecx
    add eax, edx
    sub eax, esi                        ; eax = p

    ; pa = abs(p - a)
    mov r10d, eax
    sub r10d, ecx
    mov r11d, r10d
    sar r11d, 31
    xor r10d, r11d
    sub r10d, r11d                      ; r10d = pa

    ; pb = abs(p - b)
    mov r13d, eax
    sub r13d, edx
    mov r11d, r13d
    sar r11d, 31
    xor r13d, r11d
    sub r13d, r11d                      ; r13d = pb

    ; pc = abs(p - c)
    mov r14d, eax
    sub r14d, esi
    mov r11d, r14d
    sar r11d, 31
    xor r14d, r11d
    sub r14d, r11d                      ; r14d = pc

    ; if pa <= pb and pa <= pc: return a
    cmp r10d, r13d
    ja .p_chk_b
    cmp r10d, r14d
    ja .p_chk_b
    mov eax, ecx
    ret

.p_chk_b:
    ; else if pb <= pc: return b
    cmp r13d, r14d
    ja .p_ret_c
    mov eax, edx
    ret

.p_ret_c:
    mov eax, esi
    ret
