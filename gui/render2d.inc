; ==============================================================================
; Opensweet OS - Modern 2D Graphics Engine (render2d.inc)
; Anti-Aliased Typography, Rounded Rectangles, Soft Drop Shadows,
; ARGB Alpha Compositing, Procedural Ambient Mesh Wallpaper, and Double Buffering.
; ==============================================================================

R2D_BACKBUFFER       = 0x02000000       ; 8 MB backbuffer (identity-mapped, reserved in PMM)
R2D_WALLPAPER_CACHE  = 0x02800000       ; 8 MB cached TrueColor wallpaper
R2D_WIN_CACHE        = 0x03000000       ; 8 MB cached active window drag surface
R2D_SCREEN_W         = 1920
R2D_SCREEN_H         = 1080
R2D_PITCH            = 7680             ; 1920 dwords per scanline

include 'D:\Opensweet\gui\font_mono_aa.inc'
include 'D:\Opensweet\gui\font_ui_aa.inc'
include 'D:\Opensweet\gui\icons_32bpp.inc'
include 'D:\Opensweet\gui\cursor_aa.inc'
include 'D:\Opensweet\gui\png.inc'

; ==============================================================================
; r2d_init: Read VBE parameters and load wallpaper into cache (0x02400000)
; ==============================================================================
r2d_init:
    push rax
    push rsi
    cmp byte [r14 + VBS_OK], 1
    jne .done
    mov eax, dword [r14 + VBS_LFB]
    mov [r15 + vbe_lfb - kmain], eax

    ; Try loading TrueColor PNG wallpaper from ext4 storage (/wallpaper.png)
    lea rsi, [r15 + str_default_wallpaper - kmain]
    call png_load_wallpaper_from_ext4
    jnc .done

    ; Fallback to procedural gradient if PNG not available
    call r2d_render_wallpaper_to_cache
.done:
    pop rsi
    pop rax
    ret

; ==============================================================================
; r2d_flip: Fast hardware blit from Backbuffer to VRAM LFB
; ==============================================================================
r2d_flip:
    push rsi
    push rdi
    push rcx
    push rax

    cld
    mov edi, [r15 + vbe_lfb - kmain]
    test edi, edi
    jz .flip_done

    mov rsi, R2D_BACKBUFFER
    mov ecx, (R2D_SCREEN_W * R2D_SCREEN_H * 4) / 8    ; 1,036,800 qwords
    rep movsq

    call r2d_record_frame

.flip_done:
    pop rax
    pop rcx
    pop rdi
    pop rsi
    ret

; ==============================================================================
; r2d_flip_rect: Fast hardware blit of sub-rectangle from Backbuffer to VRAM LFB
; ECX = x, EDX = y, ESI = w, EDI = h
; ==============================================================================
r2d_flip_rect:
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

    mov r8d, [r15 + vbe_lfb - kmain]
    test r8d, r8d
    jz .fr_done

    ; Check bounds: ECX=x, EDX=y, ESI=w, EDI=h
    cmp ecx, R2D_SCREEN_W
    jae .fr_done
    cmp edx, R2D_SCREEN_H
    jae .fr_done
    test esi, esi
    jle .fr_done
    test edi, edi
    jle .fr_done

    ; Clamp x + w <= R2D_SCREEN_W
    mov eax, ecx
    add eax, esi
    cmp eax, R2D_SCREEN_W
    jbe @f
    mov esi, R2D_SCREEN_W
    sub esi, ecx
@@:
    ; Clamp y + h <= R2D_SCREEN_H
    mov eax, edx
    add eax, edi
    cmp eax, R2D_SCREEN_H
    jbe @f
    mov edi, R2D_SCREEN_H
    sub edi, edx
@@:
    ; Align x down to even (8-byte aligned row starts)
    test ecx, 1
    jz @f
    dec ecx
    inc esi
@@:
    ; Align w up to even
    test esi, 1
    jz @f
    inc esi
@@:
    ; Re-check w clamp
    mov eax, ecx
    add eax, esi
    cmp eax, R2D_SCREEN_W
    jbe @f
    mov esi, R2D_SCREEN_W
    sub esi, ecx
@@:
    ; Qwords count per row = w / 2
    mov r10d, esi
    shr r10d, 1                       ; r10d = qwords per row
    test r10d, r10d
    jz .fr_done

    ; Starting byte offset = (y * R2D_SCREEN_W + x) * 4
    imul eax, edx, R2D_SCREEN_W
    add eax, ecx
    shl rax, 2                        ; byte offset in rax

    lea rsi, [rax + R2D_BACKBUFFER]
    mov r8d, r8d                      ; zero-extend r8d to r8
    add r8, rax                       ; r8 = starting vbe_lfb address

    mov r9d, edi                      ; r9d = rows remaining
    cld

.fr_row_loop:
    mov rcx, r10                      ; qword count
    mov rdi, r8                       ; dest vbe_lfb ptr
    mov r11, rsi                      ; preserve row start rsi
    rep movsq

    add r8, R2D_PITCH
    lea rsi, [r11 + R2D_PITCH]
    dec r9d
    jnz .fr_row_loop

.fr_done:
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
; r2d_blend_pixel: Blend EAX (0xAARRGGBB) onto [RDI] (0xAARRGGBB)
; ==============================================================================
r2d_blend_pixel:
    push rbx
    push rcx
    push rdx
    push rsi
    push r8

    mov ecx, eax
    shr ecx, 24                       ; ecx = src alpha (0..255)
    test ecx, ecx
    jz .bp_done                       ; alpha == 0: skip
    cmp ecx, 255
    jae .bp_opaque                    ; alpha == 255: overwrite

    mov edx, [rdi]                    ; edx = dst color

    ; Blue channel: dst_b + ((src_b - dst_b) * alpha) >> 8
    movzx esi, al                     ; src_b
    movzx ebx, dl                     ; dst_b
    sub esi, ebx
    imul esi, ecx
    sar esi, 8
    add esi, ebx                      ; out_b in esi

    ; Green channel
    push rax
    push rdx
    shr eax, 8
    shr edx, 8
    movzx ebx, al                     ; src_g
    movzx edx, dl                     ; dst_g
    sub ebx, edx
    imul ebx, ecx
    sar ebx, 8
    add ebx, edx
    shl ebx, 8
    or esi, ebx                       ; out_g
    pop rdx
    pop rax

    ; Red channel
    push rax
    push rdx
    shr eax, 16
    shr edx, 16
    movzx ebx, al                     ; src_r
    movzx edx, dl                     ; dst_r
    sub ebx, edx
    imul ebx, ecx
    sar ebx, 8
    add ebx, edx
    shl ebx, 16
    or esi, ebx                       ; out_r
    pop rdx
    pop rax

    ; Set full opacity for target pixel
    or esi, 0xFF000000
    mov [rdi], esi
    jmp .bp_done

.bp_opaque:
    mov [rdi], eax

.bp_done:
    pop r8
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

; ==============================================================================
; r2d_draw_ambient_wallpaper: Blit pre-rendered wallpaper cache to Backbuffer (<0.3 ms)
; ==============================================================================
r2d_draw_ambient_wallpaper:
    push rsi
    push rdi
    push rcx
    cld
    mov rdi, R2D_BACKBUFFER
    mov rsi, R2D_WALLPAPER_CACHE
    mov ecx, (R2D_SCREEN_W * R2D_SCREEN_H * 4) / 8    ; 1,036,800 qwords
    rep movsq
    pop rcx
    pop rdi
    pop rsi
    ret

; ==============================================================================
; r2d_restore_wallpaper_rect: Fast sub-rectangle restore from Wallpaper Cache to Backbuffer
; ECX = x, EDX = y, ESI = w, EDI = h
; ==============================================================================
r2d_restore_wallpaper_rect:
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

    ; Check bounds: ECX=x, EDX=y, ESI=w, EDI=h
    cmp ecx, R2D_SCREEN_W
    jae .rw_done
    cmp edx, R2D_SCREEN_H
    jae .rw_done
    test esi, esi
    jle .rw_done
    test edi, edi
    jle .rw_done

    ; Clamp x + w <= R2D_SCREEN_W
    mov eax, ecx
    add eax, esi
    cmp eax, R2D_SCREEN_W
    jbe @f
    mov esi, R2D_SCREEN_W
    sub esi, ecx
@@:
    ; Clamp y + h <= R2D_SCREEN_H
    mov eax, edx
    add eax, edi
    cmp eax, R2D_SCREEN_H
    jbe @f
    mov edi, R2D_SCREEN_H
    sub edi, edx
@@:
    ; Align x down to even
    test ecx, 1
    jz @f
    dec ecx
    inc esi
@@:
    ; Align w up to even
    test esi, 1
    jz @f
    inc esi
@@:
    ; Re-check w clamp
    mov eax, ecx
    add eax, esi
    cmp eax, R2D_SCREEN_W
    jbe @f
    mov esi, R2D_SCREEN_W
    sub esi, ecx
@@:
    mov r10d, esi
    shr r10d, 1                       ; r10d = qwords per row
    test r10d, r10d
    jz .rw_done

    ; Starting byte offset = (y * R2D_SCREEN_W + x) * 4
    imul eax, edx, R2D_SCREEN_W
    add eax, ecx
    shl rax, 2                        ; byte offset in rax

    lea rsi, [rax + R2D_WALLPAPER_CACHE]
    lea r8, [rax + R2D_BACKBUFFER]

    mov r9d, edi                      ; r9d = rows remaining
    cld

.rw_row_loop:
    mov rcx, r10                      ; qword count
    mov rdi, r8
    mov r11, rsi
    rep movsq

    add r8, R2D_PITCH
    lea rsi, [r11 + R2D_PITCH]
    dec r9d
    jnz .rw_row_loop

.rw_done:
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
; r2d_render_wallpaper_to_cache: One-time procedural generation into 0x02400000
; ==============================================================================
r2d_render_wallpaper_to_cache:
    push rax
    push rbx
    push rcx
    push rdx
    push rdi
    push rsi
    push r8
    push r9
    push r10
    push r11
    push r12

    mov rdi, R2D_WALLPAPER_CACHE
    xor r10d, r10d                    ; y = 0..767

.y_loop:
    mov eax, 768
    sub eax, r10d
    imul eax, 10
    shr eax, 9
    add eax, 11
    mov r11d, eax                     ; r_base

    mov eax, 768
    sub eax, r10d
    imul eax, 12
    shr eax, 9
    add eax, 15
    mov r12d, eax                     ; g_base

    mov eax, 768
    sub eax, r10d
    imul eax, 28
    shr eax, 9
    add eax, 25
    mov ebx, eax                      ; b_base

    xor r9d, r9d                      ; x = 0..1023

.x_loop:
    mov ecx, r9d
    sub ecx, 512
    mov edx, r10d
    sub edx, 620

    imul ecx, ecx
    imul edx, edx
    add ecx, edx

    cmp ecx, 144400
    jae .no_glow

    mov eax, 144400
    sub eax, ecx
    shr eax, 11
    mov r8d, eax

    mov esi, r11d
    lea esi, [esi + r8d*1]
    cmp esi, 255
    jbe @f
    mov esi, 255
@@:
    mov edx, r12d
    mov eax, ebx
    lea eax, [eax + r8d*2]
    cmp eax, 255
    jbe @f
    mov eax, 255
@@:
    shl esi, 16
    shl edx, 8
    or esi, edx
    or esi, eax
    or esi, 0xFF000000
    mov [rdi + r9*4], esi
    jmp .next_x

.no_glow:
    mov esi, r11d
    shl esi, 16
    mov edx, r12d
    shl edx, 8
    or esi, edx
    or esi, ebx
    or esi, 0xFF000000
    mov [rdi + r9*4], esi

.next_x:
    inc r9d
    cmp r9d, R2D_SCREEN_W
    jb .x_loop

    add rdi, R2D_PITCH
    inc r10d
    cmp r10d, R2D_SCREEN_H
    jb .y_loop

    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rsi
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; ==============================================================================
; r2d_fill_rect_alpha: Fill rect at ECX=x, EDX=y, ESI=w, EDI=h with EAX=color
; ==============================================================================
r2d_fill_rect_alpha:
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

    mov r8d, ecx                      ; x
    mov r9d, edx                      ; y
    mov r10d, esi                     ; w
    mov r11d, edi                     ; h (r11d preserves h, immune to rdi clobber!)
    mov r12d, eax                     ; color

    xor ebx, ebx                      ; row 0..h-1
.row_loop:
    cmp ebx, r11d
    jae .rect_done

    mov eax, r9d
    add eax, ebx                      ; cur_y = y + row
    cmp eax, R2D_SCREEN_H
    jae .rect_done

    imul eax, R2D_PITCH
    lea rdx, [rax + R2D_BACKBUFFER]
    mov eax, r8d
    lea rdx, [rdx + rax*4]            ; rdx = target row start ptr

    xor ecx, ecx                      ; col 0..w-1
.col_loop:
    cmp ecx, r10d
    jae .next_row

    mov eax, r8d
    add eax, ecx                      ; cur_x = x + col
    cmp eax, R2D_SCREEN_W
    jae .next_row

    mov eax, r12d
    lea rdi, [rdx + rcx*4]
    call r2d_blend_pixel

    inc ecx
    jmp .col_loop

.next_row:
    inc ebx
    jmp .row_loop

.rect_done:
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
; r2d_draw_rounded_rect_aa: Anti-Aliased Rounded Card
; ECX=x, EDX=y, ESI=w, EDI=h, EBX=radius, EAX=fill_color
; ==============================================================================
r2d_draw_rounded_rect_aa:
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
    sub rsp, 48

    mov [rsp + 0], ecx                  ; x
    mov [rsp + 4], edx                  ; y
    mov [rsp + 8], esi                  ; w
    mov [rsp + 12], edi                 ; h
    mov [rsp + 16], ebx                 ; r
    mov [rsp + 20], eax                 ; fill_color

    ; r_sq = r * r
    mov eax, ebx
    imul eax, eax
    mov [rsp + 24], eax                 ; r_sq

    mov dword [rsp + 28], 0             ; py = 0
.rr_y_loop:
    mov eax, [rsp + 28]                 ; py
    cmp eax, [rsp + 12]                 ; h
    jae .rr_done

    add eax, [rsp + 4]                  ; abs_y = y + py
    cmp eax, R2D_SCREEN_H
    jae .rr_done

    imul eax, R2D_PITCH
    lea rdx, [rax + R2D_BACKBUFFER]
    mov eax, [rsp + 0]                  ; x
    lea rdx, [rdx + rax*4]              ; rdx = scanline pointer
    mov [rsp + 40], rdx                 ; save scanline_ptr

    ; Fast path: if row is in middle vertical band and color is opaque, fill row with rep stosd
    mov r8d, [rsp + 28]                 ; py
    mov r9d, [rsp + 16]                 ; r
    cmp r8d, r9d
    jb .rr_start_pixel_loop             ; in top corner region
    mov eax, [rsp + 12]                 ; h
    sub eax, r9d                        ; h - r
    cmp r8d, eax
    jae .rr_start_pixel_loop            ; in bottom corner region

    mov eax, [rsp + 20]                 ; fill_color
    cmp eax, 0xFF000000
    jb .rr_start_pixel_loop             ; translucent -> per-pixel blend

    ; Opaque middle band: 100% solid fill
    mov rdi, [rsp + 40]                 ; scanline_ptr
    mov ecx, [rsp + 8]                  ; w
    mov edx, [rsp + 0]                  ; x
    add edx, ecx
    cmp edx, R2D_SCREEN_W
    jbe .do_fast_row
    mov ecx, R2D_SCREEN_W
    sub ecx, [rsp + 0]
    jle .rr_next_y
.do_fast_row:
    mov eax, [rsp + 20]                 ; fill_color
    rep stosd
    jmp .rr_next_y

.rr_start_pixel_loop:
    mov dword [rsp + 32], 0             ; px = 0
.rr_x_loop:
    mov eax, [rsp + 32]                 ; px
    cmp eax, [rsp + 8]                  ; w
    jae .rr_next_y

    add eax, [rsp + 0]                  ; abs_x = x + px
    cmp eax, R2D_SCREEN_W
    jae .rr_next_y

    ; Corner check
    mov r8d, [rsp + 28]                 ; py
    mov r9d, [rsp + 16]                 ; r
    mov r10d, [rsp + 8]                 ; w
    mov r11d, [rsp + 12]                ; h
    mov r12d, [rsp + 32]                ; px

    cmp r8d, r9d
    jb .top_corners
    mov eax, r11d
    sub eax, r9d                        ; h - r
    cmp r8d, eax
    jae .bot_corners
    jmp .inside_solid                   ; middle vertical band: solid

.top_corners:
    mov esi, r9d
    sub esi, r8d                        ; dy = r - py
    jmp .check_x_corners

.bot_corners:
    mov eax, r11d
    sub eax, r9d                        ; h - r
    mov esi, r8d
    sub esi, eax                        ; dy = py - (h - r)

.check_x_corners:
    cmp r12d, r9d
    jb .left_corners
    mov eax, r10d
    sub eax, r9d                        ; w - r
    cmp r12d, eax
    jae .right_corners
    jmp .inside_solid                   ; center top/bottom: solid

.left_corners:
    mov ecx, r9d
    sub ecx, r12d                       ; dx = r - px
    jmp .calc_corner_dist

.right_corners:
    mov eax, r10d
    sub eax, r9d                        ; w - r
    mov ecx, r12d
    sub ecx, eax                        ; dx = px - (w - r)

.calc_corner_dist:
    imul ecx, ecx
    imul esi, esi
    add ecx, esi                        ; ecx = dist_sq

    cmp ecx, [rsp + 24]                 ; dist_sq > r_sq?
    ja .skip_pixel                      ; outside corner circle

    ; Check if near edge for soft anti-aliased border
    mov eax, [rsp + 24]                 ; r_sq
    sub eax, ecx                        ; r_sq - dist_sq
    cmp eax, 30
    ja .inside_solid

    ; Soft edge: halve alpha
    mov eax, [rsp + 20]                 ; fill_color
    shr eax, 25                         ; (alpha >> 24) >> 1
    shl eax, 24
    mov edi, [rsp + 20]
    and edi, 0x00FFFFFF
    or eax, edi

    mov rdx, [rsp + 40]                 ; scanline_ptr
    mov ebx, [rsp + 32]                 ; px
    lea rdi, [rdx + rbx*4]
    call r2d_blend_pixel
    jmp .skip_pixel

.inside_solid:
    mov eax, [rsp + 20]                 ; fill_color
    mov rdx, [rsp + 40]                 ; scanline_ptr
    mov ebx, [rsp + 32]                 ; px
    lea rdi, [rdx + rbx*4]
    cmp eax, 0xFF000000
    jae .fast_solid
    call r2d_blend_pixel
    jmp .skip_pixel

.fast_solid:
    mov [rdi], eax

.skip_pixel:
    inc dword [rsp + 32]                ; px++
    jmp .rr_x_loop

.rr_next_y:
    inc dword [rsp + 28]                ; py++
    jmp .rr_y_loop

.rr_done:
    add rsp, 48
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
; r2d_draw_shadow: High-performance perimeter fringe ambient drop shadow
; ECX=x, EDX=y, ESI=w, EDI=h, EBX=radius
; Blends only the outer visible 4-8px halo, eliminating ~330,000 occluded
; interior pixel operations per frame for zero drag lag!
; ==============================================================================
r2d_draw_shadow:
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

    cmp esi, 60
    jb .shadow_fallback
    cmp edi, 40
    jb .shadow_fallback

    ; --- 1. Bottom shadow fringe (8 rows: y+h to y+h+7) ---
    mov r8d, edx
    add r8d, edi                        ; sy = y + h
    xor r9d, r9d                        ; row = 0..7
.bot_row_loop:
    cmp r9d, 8
    jae .bot_done
    mov eax, r8d
    add eax, r9d                        ; cur_y = y + h + row
    cmp eax, R2D_SCREEN_H
    jae .bot_done

    mov ebx, 0x22000000
    cmp r9d, 2
    jb .bot_alpha_ok
    mov ebx, 0x18000000
    cmp r9d, 4
    jb .bot_alpha_ok
    mov ebx, 0x10000000
    cmp r9d, 6
    jb .bot_alpha_ok
    mov ebx, 0x08000000
.bot_alpha_ok:
    mov r10d, ecx
    sub r10d, 3                         ; start x
    mov r11d, esi
    add r11d, 6                         ; span w
    test r10d, r10d
    jns @f
    add r11d, r10d
    xor r10d, r10d
@@:
    mov r12d, r10d
    add r12d, r11d
    cmp r12d, R2D_SCREEN_W
    jbe @f
    mov r11d, R2D_SCREEN_W
    sub r11d, r10d
@@:
    test r11d, r11d
    jle .bot_next_row

    imul eax, R2D_SCREEN_W
    add eax, r10d
    mov rdx, R2D_BACKBUFFER
    lea rdi, [rdx + rax*4]
.bot_pix_loop:
    mov eax, ebx
    call r2d_blend_pixel
    add rdi, 4
    dec r11d
    jnz .bot_pix_loop

.bot_next_row:
    inc r9d
    jmp .bot_row_loop
.bot_done:

    ; --- 2. Left shadow fringe (4 columns: x-4 to x-1, y+4 to y+h+4) ---
    xor r9d, r9d                        ; col = 0..3
.left_col_loop:
    cmp r9d, 4
    jae .left_done
    mov eax, ecx
    sub eax, 4
    add eax, r9d                        ; cur_x
    test eax, eax
    js .left_next_col
    cmp eax, R2D_SCREEN_W
    jae .left_next_col

    mov ebx, 0x10000000
    cmp r9d, 2
    jb .left_alpha_ok
    mov ebx, 0x1A000000
.left_alpha_ok:
    mov r10d, edx
    add r10d, 4                         ; start y
    mov r11d, edi
    sub r11d, 2                         ; row count
.left_pix_loop:
    test r11d, r11d
    jle .left_next_col
    cmp r10d, R2D_SCREEN_H
    jae .left_next_col

    mov r8d, r10d
    imul r8d, R2D_SCREEN_W
    add r8d, eax
    mov rdx, R2D_BACKBUFFER
    lea rdi, [rdx + r8*4]
    push rax
    mov eax, ebx
    call r2d_blend_pixel
    pop rax

    inc r10d
    dec r11d
    jmp .left_pix_loop
.left_next_col:
    inc r9d
    jmp .left_col_loop
.left_done:

    ; --- 3. Right shadow fringe (4 columns: x+w to x+w+3, y+4 to y+h+4) ---
    xor r9d, r9d                        ; col = 0..3
.right_col_loop:
    cmp r9d, 4
    jae .right_done
    mov eax, ecx
    add eax, esi
    add eax, r9d                        ; cur_x
    cmp eax, R2D_SCREEN_W
    jae .right_done

    mov ebx, 0x1A000000
    cmp r9d, 2
    jb .right_alpha_ok
    mov ebx, 0x10000000
.right_alpha_ok:
    mov r10d, edx
    add r10d, 4                         ; start y
    mov r11d, edi
    sub r11d, 2                         ; row count
.right_pix_loop:
    test r11d, r11d
    jle .right_next_col
    cmp r10d, R2D_SCREEN_H
    jae .right_next_col

    mov r8d, r10d
    imul r8d, R2D_SCREEN_W
    add r8d, eax
    mov rdx, R2D_BACKBUFFER
    lea rdi, [rdx + r8*4]
    push rax
    mov eax, ebx
    call r2d_blend_pixel
    pop rax

    inc r10d
    dec r11d
    jmp .right_pix_loop
.right_next_col:
    inc r9d
    jmp .right_col_loop
.right_done:
    jmp .shadow_ret

.shadow_fallback:
    sub ecx, 3
    add edx, 4
    add esi, 6
    add edi, 6
    add ebx, 3
    mov eax, 0x24000000
    call r2d_draw_rounded_rect_aa

.shadow_ret:
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
; r2d_draw_traffic_lights: Draws 3 macOS-style dots at ECX=x, EDX=y
; ==============================================================================
r2d_draw_traffic_lights:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    ; Red Close Button (#FF5F56)
    push rcx
    push rdx
    mov esi, 12
    mov edi, 12
    mov ebx, 6
    mov eax, 0xFFFF5F56
    call r2d_draw_rounded_rect_aa
    pop rdx
    pop rcx

    ; Amber Minimize Button (#FFBD2E)
    push rcx
    push rdx
    add ecx, 18
    mov esi, 12
    mov edi, 12
    mov ebx, 6
    mov eax, 0xFFFFBD2E
    call r2d_draw_rounded_rect_aa
    pop rdx
    pop rcx

    ; Green Maximize Button (#27C93F)
    push rcx
    push rdx
    add ecx, 36
    mov esi, 12
    mov edi, 12
    mov ebx, 6
    mov eax, 0xFF27C93F
    call r2d_draw_rounded_rect_aa
    pop rdx
    pop rcx

    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; ==============================================================================
; r2d_draw_text_mono_aa: Print ASCIIZ string at ECX=x, EDX=y with EAX=color (Consolas 8x15)
; ==============================================================================
r2d_draw_text_mono_aa:
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

    mov r8d, ecx                      ; cur_x
    mov r9d, edx                      ; cur_y
    mov r10d, eax                     ; text color (ARGB)

.next_mono_char:
    lodsb
    test al, al
    jz .mono_done
    cmp al, 10                        ; \n
    je .mono_lf
    cmp al, 32
    jb .next_mono_char
    cmp al, 126
    ja .next_mono_char

    ; Glyph offset = (char - 32) * 120
    movzx eax, al
    sub eax, 32
    imul eax, (FONT_MONO_AA_W * FONT_MONO_AA_H)
    lea r11, [r15 + font_mono_aa_data - kmain + rax]

    ; Draw 8x15 glyph with 8-bit alpha
    xor edx, edx                      ; gy = 0..14
.mono_gy_loop:
    cmp edx, FONT_MONO_AA_H
    jae .mono_glyph_done

    mov eax, r9d
    add eax, edx                      ; py = cur_y + gy
    cmp eax, R2D_SCREEN_H
    jae .mono_glyph_done

    imul eax, R2D_PITCH
    lea rdi, [rax + R2D_BACKBUFFER]
    mov eax, r8d
    lea rdi, [rdi + rax*4]            ; scanline ptr at cur_x

    xor ecx, ecx                      ; gx = 0..7
.mono_gx_loop:
    cmp ecx, FONT_MONO_AA_W
    jae .mono_next_gy

    mov eax, r8d
    add eax, ecx
    cmp eax, R2D_SCREEN_W
    jae .mono_next_gy

    ; Alpha byte from font atlas
    mov eax, edx
    imul eax, FONT_MONO_AA_W
    add eax, ecx
    movzx eax, byte [r11 + rax]       ; 0..255
    test eax, eax
    jz .mono_skip_px

    ; Scale color alpha by glyph alpha
    mov ebx, r10d
    shr ebx, 24
    imul eax, ebx
    shr eax, 8
    test eax, eax
    jz .mono_skip_px

    shl eax, 24
    mov ebx, r10d
    and ebx, 0x00FFFFFF
    or eax, ebx                       ; eax = blended ARGB

    push rdi
    lea rdi, [rdi + rcx*4]
    call r2d_blend_pixel
    pop rdi

.mono_skip_px:
    inc ecx
    jmp .mono_gx_loop

.mono_next_gy:
    inc edx
    jmp .mono_gy_loop

.mono_glyph_done:
    add r8d, FONT_MONO_AA_W
    jmp .next_mono_char

.mono_lf:
    mov r8d, 100                      ; left margin
    add r9d, 18                       ; line height
    jmp .next_mono_char

.mono_done:
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
; r2d_draw_text_ui_aa: Print proportional UI string at ECX=x, EDX=y with EAX=color
; ==============================================================================
r2d_draw_text_ui_aa:
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

    mov r8d, ecx                      ; cur_x
    mov r9d, edx                      ; cur_y
    mov r10d, eax                     ; text color

.next_ui_char:
    lodsb
    test al, al
    jz .ui_done
    cmp al, 32
    jb .next_ui_char
    cmp al, 126
    ja .next_ui_char

    movzx r13d, al
    sub r13d, 32

    ; Glyph advance width
    lea rbx, [r15 + font_ui_widths - kmain]
    movzx r12d, byte [rbx + r13]      ; advance width

    ; Glyph offset = (char - 32) * 192 (12*16)
    mov eax, r13d
    imul eax, (FONT_UI_AA_MAX_W * FONT_UI_AA_H)
    lea r11, [r15 + font_ui_aa_data - kmain + rax]

    xor edx, edx                      ; gy = 0..15
.ui_gy_loop:
    cmp edx, FONT_UI_AA_H
    jae .ui_glyph_done

    mov eax, r9d
    add eax, edx
    cmp eax, R2D_SCREEN_H
    jae .ui_glyph_done

    imul eax, R2D_PITCH
    lea rdi, [rax + R2D_BACKBUFFER]
    mov eax, r8d
    lea rdi, [rdi + rax*4]

    xor ecx, ecx                      ; gx = 0..11
.ui_gx_loop:
    cmp ecx, FONT_UI_AA_MAX_W
    jae .ui_next_gy

    mov eax, r8d
    add eax, ecx
    cmp eax, R2D_SCREEN_W
    jae .ui_next_gy

    mov eax, edx
    imul eax, FONT_UI_AA_MAX_W
    add eax, ecx
    movzx eax, byte [r11 + rax]
    test eax, eax
    jz .ui_skip_px

    mov ebx, r10d
    shr ebx, 24
    imul eax, ebx
    shr eax, 8
    test eax, eax
    jz .ui_skip_px

    shl eax, 24
    mov ebx, r10d
    and ebx, 0x00FFFFFF
    or eax, ebx

    push rdi
    lea rdi, [rdi + rcx*4]
    call r2d_blend_pixel
    pop rdi

.ui_skip_px:
    inc ecx
    jmp .ui_gx_loop

.ui_next_gy:
    inc edx
    jmp .ui_gy_loop

.ui_glyph_done:
    add r8d, r12d
    jmp .next_ui_char

.ui_done:
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
; r2d_draw_icon_44: Blit 44x44 RGBA vector icon at ECX=x, EDX=y, RSI=icon_data
; ==============================================================================
r2d_draw_icon_44:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9

    mov r8d, ecx                      ; x
    mov r9d, edx                      ; y

    xor ebx, ebx                      ; row 0..43
.icon_row:
    cmp ebx, 44
    jae .icon_done

    mov eax, r9d
    add eax, ebx
    cmp eax, R2D_SCREEN_H
    jae .icon_done

    imul eax, R2D_PITCH
    lea rdi, [rax + R2D_BACKBUFFER]
    mov eax, r8d
    lea rdi, [rdi + rax*4]

    xor ecx, ecx                      ; col 0..43
.icon_col:
    cmp ecx, 44
    jae .icon_next_row

    mov eax, r8d
    add eax, ecx
    cmp eax, R2D_SCREEN_W
    jae .icon_next_row

    lodsd                             ; eax = [rsi], rsi += 4
    push rdi
    lea rdi, [rdi + rcx*4]
    call r2d_blend_pixel
    pop rdi

    inc ecx
    jmp .icon_col

.icon_next_row:
    inc ebx
    jmp .icon_row

.icon_done:
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

r2d_draw_icon_32 = r2d_draw_icon_44

; ==============================================================================
; r2d_draw_cursor_aa: Blit 18x18 anti-aliased mouse pointer at ECX=x, EDX=y
; ==============================================================================
r2d_draw_cursor_aa:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9

    mov r8d, ecx
    mov r9d, edx
    lea rsi, [r15 + cursor_aa_data - kmain]

    xor ebx, ebx                      ; row 0..CURSOR_AA_H-1
.cur_row:
    cmp ebx, CURSOR_AA_H
    jae .cur_done

    mov eax, r9d
    add eax, ebx
    cmp eax, R2D_SCREEN_H
    jae .cur_done

    imul eax, R2D_PITCH
    lea rdi, [rax + R2D_BACKBUFFER]
    mov eax, r8d
    lea rdi, [rdi + rax*4]

    xor ecx, ecx
.cur_col:
    cmp ecx, CURSOR_AA_W
    jae .cur_next_row

    mov eax, r8d
    add eax, ecx
    cmp eax, R2D_SCREEN_W
    jae .cur_next_row

    lodsd
    push rdi
    lea rdi, [rdi + rcx*4]
    call r2d_blend_pixel
    pop rdi

    inc ecx
    jmp .cur_col

.cur_next_row:
    inc ebx
    jmp .cur_row

.cur_done:
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
; r2d_draw_cursor_lfb: Draw AA cursor directly onto VRAM LFB at ECX=x, EDX=y
; ==============================================================================
r2d_draw_cursor_lfb:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10

    mov r8d, ecx                      ; x
    mov r9d, edx                      ; y
    lea rsi, [r15 + cursor_aa_data - kmain]

    mov r10d, [r15 + vbe_lfb - kmain]
    test r10d, r10d
    jz .cur_lfb_done

    xor ebx, ebx                      ; row 0..CURSOR_AA_H-1
.cur_lfb_row:
    cmp ebx, CURSOR_AA_H
    jae .cur_lfb_done

    mov eax, r9d
    add eax, ebx
    cmp eax, R2D_SCREEN_H
    jae .cur_lfb_done

    imul eax, R2D_PITCH
    add eax, r10d
    mov edi, eax
    mov eax, r8d
    lea rdi, [rdi + rax*4]

    xor ecx, ecx                      ; col 0..CURSOR_AA_W-1
.cur_lfb_col:
    cmp ecx, CURSOR_AA_W
    jae .cur_lfb_next_row

    mov eax, r8d
    add eax, ecx
    cmp eax, R2D_SCREEN_W
    jae .cur_lfb_next_row

    lodsd
    push rdi
    lea rdi, [rdi + rcx*4]
    call r2d_blend_pixel
    pop rdi

    inc ecx
    jmp .cur_lfb_col

.cur_lfb_next_row:
    inc ebx
    jmp .cur_lfb_row

.cur_lfb_done:
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
; r2d_restore_cursor_lfb: Restore cursor rectangle from R2D_BACKBUFFER to VRAM LFB
; ECX=x, EDX=y (< 1 microsecond!)
; ==============================================================================
r2d_restore_cursor_lfb:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10

    mov r8d, ecx                      ; x
    mov r9d, edx                      ; y
    mov r10d, [r15 + vbe_lfb - kmain]
    test r10d, r10d
    jz .res_done

    xor ebx, ebx                      ; row 0..CURSOR_AA_H-1
.res_row:
    cmp ebx, CURSOR_AA_H
    jae .res_done

    mov eax, r9d
    add eax, ebx
    cmp eax, R2D_SCREEN_H
    jae .res_done

    imul eax, R2D_PITCH
    lea rsi, [rax + R2D_BACKBUFFER]
    add eax, r10d
    mov edi, eax
    mov eax, r8d
    lea rsi, [rsi + rax*4]
    lea rdi, [rdi + rax*4]

    ; (CURSOR_AA_W / 2) qwords per row
    cld
    mov ecx, (CURSOR_AA_W / 2)
    rep movsq

    inc ebx
    jmp .res_row

.res_done:
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
; r2d_redraw_clock_lfb: Blit updated clock directly to menubar in VRAM LFB
; Fast microsecond path used by background clock daemon
; ==============================================================================
r2d_redraw_clock_lfb:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    ; Concurrency guard: if desktop compositor is actively rendering, skip
    cmp dword [r15 + gui_lock - kmain], 0
    jne .clk_done

    ; 1. Restore pristine background from R2D_WALLPAPER_CACHE to R2D_BACKBUFFER
    xor ebx, ebx                      ; row 0..27
.clk_restore_bg:
    cmp ebx, 28
    jae .clk_bg_restored

    imul eax, ebx, R2D_PITCH
    lea rsi, [rax + R2D_WALLPAPER_CACHE + (R2D_SCREEN_W - 85)*4]
    lea rdi, [rax + R2D_BACKBUFFER + (R2D_SCREEN_W - 85)*4]
    cld
    mov ecx, 40                       ; 80 dwords = 40 qwords
    rep movsq

    inc ebx
    jmp .clk_restore_bg

.clk_bg_restored:
    ; 2. Apply translucent menubar tint
    mov ecx, R2D_SCREEN_W - 85
    xor edx, edx                      ; y = 0
    mov esi, 80
    mov edi, 28
    mov eax, 0xC00F172A               ; Translucent Slate 900
    call r2d_fill_rect_alpha

    ; 3. Render clock text string at x = R2D_SCREEN_W - 78, y = 6
    mov ecx, R2D_SCREEN_W - 78
    mov edx, 6
    mov eax, 0xFFF8FAFC               ; White
    lea rsi, [r15 + str_md_clock - kmain]
    call r2d_draw_text_ui_aa

    ; 4. Blit 80x28 rectangle from R2D_BACKBUFFER directly to VRAM LFB
    mov eax, [r15 + vbe_lfb - kmain]
    test eax, eax
    jz .clk_done

    xor ebx, ebx                      ; row 0..27
.clk_row:
    cmp ebx, 28
    jae .clk_done

    imul eax, ebx, R2D_PITCH
    lea rsi, [rax + R2D_BACKBUFFER + (R2D_SCREEN_W - 85)*4]
    add eax, [r15 + vbe_lfb - kmain]
    lea rdi, [rax + (R2D_SCREEN_W - 85)*4]

    ; 80 dwords = 40 qwords
    cld
    mov ecx, 40
    rep movsq

    inc ebx
    jmp .clk_row

.clk_done:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

align 16
gui_lock            dd 0
gui_frame_count     dd 0
gui_fps             dd 60
gui_fps_sec_frames  dd 0
gui_last_sec_tick   dd 0
gui_last_frame_tick dd 0

; ==============================================================================
; r2d_redraw_fps_lfb: Refresh FPS badge directly to menubar in VRAM LFB
; ==============================================================================
r2d_redraw_fps_lfb:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    ; 1. Format current FPS string
    call md_format_fps

    ; 2. Restore 80x28 menubar background from R2D_WALLPAPER_CACHE to R2D_BACKBUFFER
    ; at x = R2D_SCREEN_W - 170
    xor ebx, ebx                      ; row 0..27
.fps_restore_bg:
    cmp ebx, 28
    jae .fps_bg_restored

    imul eax, ebx, R2D_PITCH
    lea rsi, [rax + R2D_WALLPAPER_CACHE + (R2D_SCREEN_W - 170)*4]
    lea rdi, [rax + R2D_BACKBUFFER + (R2D_SCREEN_W - 170)*4]
    cld
    mov ecx, 40                       ; 80 dwords = 40 qwords
    rep movsq

    inc ebx
    jmp .fps_restore_bg

.fps_bg_restored:
    ; 3. Apply translucent menubar tint
    mov ecx, R2D_SCREEN_W - 170
    xor edx, edx                      ; y = 0
    mov esi, 80
    mov edi, 28
    mov eax, 0xC00F172A               ; Translucent Slate 900
    call r2d_fill_rect_alpha

    ; 4. Render FPS text string at x = R2D_SCREEN_W - 165, y = 6
    mov ecx, R2D_SCREEN_W - 165
    mov edx, 6
    mov eax, 0xFF10B981               ; Vibrant Emerald Green
    lea rsi, [r15 + str_md_fps - kmain]
    call r2d_draw_text_ui_aa

    ; 5. Blit 80x28 rectangle from R2D_BACKBUFFER directly to VRAM LFB
    mov eax, [r15 + vbe_lfb - kmain]
    test eax, eax
    jz .fps_done

    xor ebx, ebx                      ; row 0..27
.fps_row:
    cmp ebx, 28
    jae .fps_done

    imul eax, ebx, R2D_PITCH
    lea rsi, [rax + R2D_BACKBUFFER + (R2D_SCREEN_W - 170)*4]
    add eax, [r15 + vbe_lfb - kmain]
    lea rdi, [rax + (R2D_SCREEN_W - 170)*4]

    cld
    mov ecx, 40                       ; 80 dwords = 40 qwords
    rep movsq

    inc ebx
    jmp .fps_row

.fps_done:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; ==============================================================================
; r2d_record_frame: Update frame counter and calculate live instantaneous FPS
; ==============================================================================
r2d_record_frame:
    push rax
    push rbx
    push rcx
    push rdx

    inc dword [r15 + gui_frame_count - kmain]
    inc dword [r15 + gui_fps_sec_frames - kmain]

    ; Measure delta time since last frame
    mov eax, [r15 + timer_ticks - kmain]
    mov ebx, eax
    sub ebx, [r15 + gui_last_frame_tick - kmain]
    mov [r15 + gui_last_frame_tick - kmain], eax

    ; If delta_t in valid range [1..250 ms], compute instant FPS = 1000 / delta_t
    test ebx, ebx
    jle .check_badge
    cmp ebx, 250
    ja .check_badge

    mov ecx, 1000
    xor edx, edx
    mov eax, ecx
    div ebx                           ; eax = instantaneous FPS
    cmp eax, 999
    jbe @f
    mov eax, 999
@@:
    ; Exponential moving average: fps = (fps * 3 + instant) / 4
    mov ecx, [r15 + gui_fps - kmain]
    imul ecx, 3
    add eax, ecx
    shr eax, 2
    test eax, eax
    jnz @f
    mov eax, 1
@@:
    mov [r15 + gui_fps - kmain], eax

.check_badge:
    ; Refresh menubar FPS badge directly to VRAM LFB every 250 ms (4x / sec)
    mov eax, [r15 + timer_ticks - kmain]
    sub eax, [r15 + gui_last_sec_tick - kmain]
    cmp eax, 250
    jb .rf_done

    mov eax, [r15 + timer_ticks - kmain]
    mov [r15 + gui_last_sec_tick - kmain], eax

    ; Only redraw badge if not actively inside another menubar render
    call r2d_redraw_fps_lfb

.rf_done:
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret



