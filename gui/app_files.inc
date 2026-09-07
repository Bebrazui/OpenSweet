; ==============================================================================
; Opensweet OS - ext4 File Explorer Application (app_files.inc)
; Interactive directory browser, file inspection, row selection, and metadata.
; ==============================================================================

align 16
files_selected_row dd 1                 ; 0 = lost+found, 1 = hello.txt, etc.

str_files_path     db "/ rootfs", 0
str_files_ref      db "Refresh", 0
str_files_vol      db "ext4 | ATA Drive 1", 0

str_files_col_t    db "TYPE", 0
str_files_col_n    db "NAME", 0
str_files_col_s    db "SIZE", 0
str_files_col_i    db "INODE", 0

str_files_dir_tag  db "[DIR]", 0
str_files_file_tag db "[FILE]", 0

str_files_n0       db "lost+found", 0
str_files_s0       db "4096 B", 0
str_files_i0       db "inode 11", 0

str_files_n1       db "hello.txt", 0
str_files_s1       db "26 B", 0
str_files_i1       db "inode 12", 0

str_files_n2       db "big.txt", 0
str_files_s2       db "4096 B", 0
str_files_i2       db "inode 13", 0

str_files_n3       db "kernel.bin", 0
str_files_s3       db "82,128 B", 0
str_files_i3       db "inode 14", 0

str_files_stat0    db "Selected: lost+found (Directory, 4096 B)", 0
str_files_stat1    db "Selected: hello.txt (ASCII text, 26 B)", 0
str_files_stat2    db "Selected: big.txt (Benchmark text, 4096 B)", 0
str_files_stat3    db "Selected: kernel.bin (ELF64 Executable, 82 KB)", 0

; ==============================================================================
; app_files_render: Render File Explorer application contents
; ECX = wx, EDX = wy, ESI = ww, EDI = wh, EAX = is_focused
; ==============================================================================
app_files_render:
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
    mov r10d, esi                       ; ww
    mov r11d, edi                       ; wh

    ; --- 1. Toolbar / Breadcrumbs Area ---
    ; Breadcrumb capsule
    mov ecx, r8d
    add ecx, 16
    mov edx, r9d
    add edx, 42
    mov esi, 130
    mov edi, 26
    mov ebx, 6
    mov eax, 0xFF1E293B                 ; Slate 800
    call r2d_draw_rounded_rect_aa

    mov ecx, r8d
    add ecx, 32
    mov edx, r9d
    add edx, 48
    mov eax, 0xFF38BDF8                 ; Sky Blue
    lea rsi, [r15 + str_files_path - kmain]
    call r2d_draw_text_ui_aa

    ; Refresh button capsule
    mov ecx, r8d
    add ecx, 156
    mov edx, r9d
    add edx, 42
    mov esi, 74
    mov edi, 26
    mov ebx, 6
    mov eax, 0xFF1E293B
    call r2d_draw_rounded_rect_aa

    mov ecx, r8d
    add ecx, 168
    mov edx, r9d
    add edx, 48
    mov eax, 0xFF94A3B8
    lea rsi, [r15 + str_files_ref - kmain]
    call r2d_draw_text_ui_aa

    ; Volume label
    mov ecx, r8d
    add ecx, 250
    mov edx, r9d
    add edx, 48
    mov eax, 0xFF64748B
    lea rsi, [r15 + str_files_vol - kmain]
    call r2d_draw_text_ui_aa

    ; Toolbar divider line
    mov ecx, r8d
    add ecx, 16
    mov edx, r9d
    add edx, 78
    mov esi, r10d
    sub esi, 32
    mov edi, 1
    mov eax, 0x40FFFFFF
    call r2d_fill_rect_alpha

    ; --- 2. Table Header ---
    mov ecx, r8d
    add ecx, 24
    mov edx, r9d
    add edx, 88
    mov eax, 0xFF64748B
    lea rsi, [r15 + str_files_col_t - kmain]
    call r2d_draw_text_ui_aa

    mov ecx, r8d
    add ecx, 90
    mov edx, r9d
    add edx, 88
    lea rsi, [r15 + str_files_col_n - kmain]
    call r2d_draw_text_ui_aa

    mov ecx, r8d
    add ecx, 250
    mov edx, r9d
    add edx, 88
    lea rsi, [r15 + str_files_col_s - kmain]
    call r2d_draw_text_ui_aa

    mov ecx, r8d
    add ecx, 370
    mov edx, r9d
    add edx, 88
    lea rsi, [r15 + str_files_col_i - kmain]
    call r2d_draw_text_ui_aa

    ; --- 3. Directory Rows (4 rows) ---
    xor r12d, r12d                      ; row = 0..3
.row_loop:
    cmp r12d, 4
    jae .rows_done

    ; Calculate row Y
    mov edx, r9d
    add edx, 110
    mov eax, r12d
    imul eax, 32
    add edx, eax                        ; row_y in edx

    ; Check if row is selected
    cmp r12d, [r15 + files_selected_row - kmain]
    jne .not_selected

    ; Draw selection highlight bar
    push rdx
    mov ecx, r8d
    add ecx, 16
    sub edx, 4
    mov esi, r10d
    sub esi, 32
    mov edi, 28
    mov ebx, 6
    mov eax, 0xFF1E293B                 ; Highlight background
    call r2d_draw_rounded_rect_aa

    ; Active selection left indicator strip
    mov ecx, r8d
    add ecx, 16
    mov esi, 4
    mov edi, 28
    mov ebx, 2
    mov eax, 0xFF38BDF8                 ; Sky Blue accent strip
    call r2d_draw_rounded_rect_aa
    pop rdx

.not_selected:
    ; Draw row contents based on r12d
    cmp r12d, 0
    je .draw_r0
    cmp r12d, 1
    je .draw_r1
    cmp r12d, 2
    je .draw_r2
    jmp .draw_r3

.draw_r0:
    push rdx
    mov ecx, r8d
    add ecx, 24
    mov eax, 0xFF38BDF8                 ; [DIR] in Sky Blue
    lea rsi, [r15 + str_files_dir_tag - kmain]
    call r2d_draw_text_mono_aa

    mov ecx, r8d
    add ecx, 90
    mov eax, 0xFFF8FAFC
    lea rsi, [r15 + str_files_n0 - kmain]
    call r2d_draw_text_ui_aa

    mov ecx, r8d
    add ecx, 250
    mov eax, 0xFF94A3B8
    lea rsi, [r15 + str_files_s0 - kmain]
    call r2d_draw_text_mono_aa

    mov ecx, r8d
    add ecx, 370
    mov eax, 0xFF94A3B8
    lea rsi, [r15 + str_files_i0 - kmain]
    call r2d_draw_text_mono_aa
    pop rdx
    jmp .next_row

.draw_r1:
    push rdx
    mov ecx, r8d
    add ecx, 24
    mov eax, 0xFF10B981                 ; [FILE] in Emerald
    lea rsi, [r15 + str_files_file_tag - kmain]
    call r2d_draw_text_mono_aa

    mov ecx, r8d
    add ecx, 90
    mov eax, 0xFFF8FAFC
    lea rsi, [r15 + str_files_n1 - kmain]
    call r2d_draw_text_ui_aa

    mov ecx, r8d
    add ecx, 250
    mov eax, 0xFF94A3B8
    lea rsi, [r15 + str_files_s1 - kmain]
    call r2d_draw_text_mono_aa

    mov ecx, r8d
    add ecx, 370
    mov eax, 0xFF94A3B8
    lea rsi, [r15 + str_files_i1 - kmain]
    call r2d_draw_text_mono_aa
    pop rdx
    jmp .next_row

.draw_r2:
    push rdx
    mov ecx, r8d
    add ecx, 24
    mov eax, 0xFF10B981
    lea rsi, [r15 + str_files_file_tag - kmain]
    call r2d_draw_text_mono_aa

    mov ecx, r8d
    add ecx, 90
    mov eax, 0xFFF8FAFC
    lea rsi, [r15 + str_files_n2 - kmain]
    call r2d_draw_text_ui_aa

    mov ecx, r8d
    add ecx, 250
    mov eax, 0xFF94A3B8
    lea rsi, [r15 + str_files_s2 - kmain]
    call r2d_draw_text_mono_aa

    mov ecx, r8d
    add ecx, 370
    mov eax, 0xFF94A3B8
    lea rsi, [r15 + str_files_i2 - kmain]
    call r2d_draw_text_mono_aa
    pop rdx
    jmp .next_row

.draw_r3:
    push rdx
    mov ecx, r8d
    add ecx, 24
    mov eax, 0xFFEAB308                 ; Amber [FILE] for kernel
    lea rsi, [r15 + str_files_file_tag - kmain]
    call r2d_draw_text_mono_aa

    mov ecx, r8d
    add ecx, 90
    mov eax, 0xFFF8FAFC
    lea rsi, [r15 + str_files_n3 - kmain]
    call r2d_draw_text_ui_aa

    mov ecx, r8d
    add ecx, 250
    mov eax, 0xFF94A3B8
    lea rsi, [r15 + str_files_s3 - kmain]
    call r2d_draw_text_mono_aa

    mov ecx, r8d
    add ecx, 370
    mov eax, 0xFF94A3B8
    lea rsi, [r15 + str_files_i3 - kmain]
    call r2d_draw_text_mono_aa
    pop rdx

.next_row:
    inc r12d
    jmp .row_loop

.rows_done:
    ; --- 4. Status Bar at bottom of window ---
    mov ecx, r8d
    add ecx, 16
    mov edx, r9d
    add edx, r11d
    sub edx, 34
    mov esi, r10d
    sub esi, 32
    mov edi, 1
    mov eax, 0x40FFFFFF
    call r2d_fill_rect_alpha

    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, r11d
    sub edx, 24
    mov eax, 0xFF64748B

    mov ebx, [r15 + files_selected_row - kmain]
    cmp ebx, 0
    je .st0
    cmp ebx, 1
    je .st1
    cmp ebx, 2
    je .st2
    lea rsi, [r15 + str_files_stat3 - kmain]
    jmp .do_stat_draw
.st0:
    lea rsi, [r15 + str_files_stat0 - kmain]
    jmp .do_stat_draw
.st1:
    lea rsi, [r15 + str_files_stat1 - kmain]
    jmp .do_stat_draw
.st2:
    lea rsi, [r15 + str_files_stat2 - kmain]
.do_stat_draw:
    call r2d_draw_text_ui_aa

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
; app_files_click: Handle mouse click inside File Explorer
; ECX = mouse_x, EDX = mouse_y, E8D = wx, E9D = wy
; Returns: AL = 1 if handled/selection changed, 0 otherwise
; ==============================================================================
app_files_click:
    push rbx
    push rcx
    push rdx

    ; Check if within row list area:
    ; Y between wy + 104 and wy + 104 + 4*32 (wy + 232)
    mov eax, edx
    sub eax, r9d                        ; rel_y = mouse_y - wy
    cmp eax, 104
    jb .click_none
    cmp eax, 232
    jae .click_none

    sub eax, 104
    shr eax, 5                          ; divide by 32 -> row index (0..3)
    cmp eax, 3
    jbe .row_ok
    mov eax, 3
.row_ok:
    mov [r15 + files_selected_row - kmain], eax
    mov al, 1
    jmp .click_done

.click_none:
    xor al, al
.click_done:
    pop rdx
    pop rcx
    pop rbx
    ret
