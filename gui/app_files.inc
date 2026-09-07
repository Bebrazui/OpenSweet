; ==============================================================================
; Opensweet OS - ext4 File Explorer Application (app_files.inc)
; Dynamic directory browser reading directly from ext4 VFS disk structures:
; real directory entries, real file sizes from inode extents, and real inodes.
; ==============================================================================

FILES_MAX_ROWS = 8

align 16
files_count        dd 0
files_selected_row dd 0
files_inited       db 0

; Dynamic File Entry Table (up to FILES_MAX_ROWS rows)
files_entry_inode  dd FILES_MAX_ROWS dup(0)
files_entry_size   dq FILES_MAX_ROWS dup(0)
files_entry_type   db FILES_MAX_ROWS dup(0)    ; 1 = file, 2 = dir
files_entry_name   rb FILES_MAX_ROWS * 32      ; 32 bytes ASCIIZ per name
files_entry_str_sz rb FILES_MAX_ROWS * 16      ; "4096 B", "26 B", etc.
files_entry_str_in rb FILES_MAX_ROWS * 16      ; "inode 11", etc.

files_stat_buf     rb 96                       ; "Selected: <name> (Directory, 4096 B, inode 11)"
files_cwd_disp     rb 64                       ; breadcrumb string e.g. "/ rootfs"

str_files_path_def db "/ rootfs", 0
str_files_ref      db "Refresh", 0
str_files_vol_ok   db "ext4 | ATA Drive 1 (Mounted)", 0
str_files_vol_none db "ext4 | No Volume", 0

str_files_col_t    db "TYPE", 0
str_files_col_n    db "NAME", 0
str_files_col_s    db "SIZE", 0
str_files_col_i    db "INODE", 0

str_files_dir_tag  db "[DIR]", 0
str_files_file_tag db "[FILE]", 0
str_files_empty    db "Directory is empty or ext4 not mounted", 0

; ==============================================================================
; app_files_u64_to_dec: Format integer in RAX into buffer RDI (null terminated)
; Advances RDI to the null terminator. Preserves RBX, RCX, RDX.
; ==============================================================================
app_files_u64_to_dec:
    push rbx
    push rcx
    push rdx

    test rax, rax
    jnz .non_zero
    mov byte [rdi], '0'
    inc rdi
    mov byte [rdi], 0
    jmp .done

.non_zero:
    mov rbx, 10
    xor ecx, ecx
.div_loop:
    xor edx, edx
    div rbx
    push rdx
    inc ecx
    test rax, rax
    jnz .div_loop

.pop_loop:
    pop rax
    add al, '0'
    mov [rdi], al
    inc rdi
    loop .pop_loop
    mov byte [rdi], 0

.done:
    pop rdx
    pop rcx
    pop rbx
    ret

; ==============================================================================
; app_files_format_size: Format file byte size RAX into RDI (e.g. "26 B", "4 KB")
; ==============================================================================
app_files_format_size:
    push rax
    push rbx
    push rcx
    push rdx

    cmp rax, 1048576
    jae .fmt_mb
    cmp rax, 1024
    jae .fmt_kb

    ; Bytes (< 1024)
    call app_files_u64_to_dec
    mov byte [rdi], ' '
    mov byte [rdi+1], 'B'
    mov byte [rdi+2], 0
    jmp .fmt_done

.fmt_kb:
    shr rax, 10
    call app_files_u64_to_dec
    mov byte [rdi], ' '
    mov byte [rdi+1], 'K'
    mov byte [rdi+2], 'B'
    mov byte [rdi+3], 0
    jmp .fmt_done

.fmt_mb:
    shr rax, 20
    call app_files_u64_to_dec
    mov byte [rdi], ' '
    mov byte [rdi+1], 'M'
    mov byte [rdi+2], 'B'
    mov byte [rdi+3], 0

.fmt_done:
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; ==============================================================================
; app_files_update_stat: Construct status bar string for currently selected row
; ==============================================================================
app_files_update_stat:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    lea rdi, [r15 + files_stat_buf - kmain]

    cmp dword [r15 + files_count - kmain], 0
    jne .has_files
    mov byte [rdi], 0
    jmp .stat_done

.has_files:
    mov eax, [r15 + files_selected_row - kmain]
    cmp eax, [r15 + files_count - kmain]
    jb @f
    xor eax, eax
    mov [r15 + files_selected_row - kmain], eax
@@:
    mov ebx, eax                        ; selected row index

    ; Prefix: "Selected: "
    mov dword [rdi], 'Sele'
    mov dword [rdi+4], 'cted'
    mov word [rdi+8], ': '
    add rdi, 10

    ; Copy filename
    imul edx, ebx, 32
    lea rsi, [r15 + files_entry_name - kmain + rdx]
.cp_sname:
    lodsb
    test al, al
    jz .done_sname
    stosb
    jmp .cp_sname
.done_sname:

    ; " ("
    mov word [rdi], ' ('
    add rdi, 2

    ; "Directory" or "File"
    cmp byte [r15 + files_entry_type - kmain + rbx], 2
    jne .stat_file
    mov dword [rdi], 'Dire'
    mov dword [rdi+4], 'ctor'
    mov byte [rdi+8], 'y'
    add rdi, 9
    jmp .stat_sz
.stat_file:
    mov dword [rdi], 'File'
    add rdi, 4

.stat_sz:
    ; ", "
    mov word [rdi], ', '
    add rdi, 2

    ; Copy formatted size
    imul edx, ebx, 16
    lea rsi, [r15 + files_entry_str_sz - kmain + rdx]
.cp_ssz:
    lodsb
    test al, al
    jz .done_ssz
    stosb
    jmp .cp_ssz
.done_ssz:

    ; ", "
    mov word [rdi], ', '
    add rdi, 2

    ; Copy formatted inode
    imul edx, ebx, 16
    lea rsi, [r15 + files_entry_str_in - kmain + rdx]
.cp_sin:
    lodsb
    test al, al
    jz .done_sin
    stosb
    jmp .cp_sin
.done_sin:

    ; ")"
    mov byte [rdi], ')'
    mov byte [rdi+1], 0

.stat_done:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; ==============================================================================
; app_files_refresh_ext4: Scan real directory from ext4 filesystem on disk
; ==============================================================================
app_files_refresh_ext4:
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

    mov dword [r15 + files_count - kmain], 0

    ; Verify filesystem is mounted
    cmp byte [r15 + ext4_ok - kmain], 1
    jne .scan_done

    ; Update breadcrumb display: "/ rootfs"
    lea rsi, [r15 + ext4_cwd_path - kmain]
    lea rdi, [r15 + files_cwd_disp - kmain]
.cp_path:
    lodsb
    stosb
    test al, al
    jnz .cp_path

    ; Load current directory inode into ext4_inode
    mov eax, [r15 + ext4_cwd_inode - kmain]
    test eax, eax
    jnz @f
    mov eax, 2                          ; Root directory inode
    mov [r15 + ext4_cwd_inode - kmain], eax
@@:
    call ext4_inode_load
    jc .scan_done

    ; r13 = directory size in bytes
    mov r13, qword [r15 + ext4_inode - kmain + IN_OFF_SIZE]

    ; Walk directory logical blocks
    mov dword [r15 + e4t_lb - kmain], 0

.next_lb:
    mov eax, dword [r15 + e4t_lb - kmain]
    mov ecx, dword [r15 + ext4_lbs - kmain]
    add ecx, 10
    shl rax, cl                         ; logical byte offset = lb << (lbs + 10)
    cmp rax, r13
    jae .scan_done

    mov eax, dword [r15 + e4t_lb - kmain]
    call ext4_map
    cmp rax, -1
    je .scan_done

    ; Read directory block into ext4_bbuf
    lea rsi, [r15 + ext4_bbuf - kmain]
    call ext4_blk_read

    ; Parse entries in ext4_bbuf
    lea r8, [r15 + ext4_bbuf - kmain]
    mov ecx, dword [r15 + ext4_lbs - kmain]
    mov edx, 1024
    shl edx, cl                         ; block size in bytes
    mov r9, r8
    add r9, rdx                         ; block end

.parse_ent:
    cmp r8, r9
    jae .lb_done

    mov eax, [r8]                       ; entry inode
    test eax, eax
    jz .skip_ent

    movzx ecx, byte [r8 + 6]            ; name length
    test ecx, ecx
    jz .skip_ent

    movzx ebx, byte [r8 + 7]            ; file_type: 1=file, 2=dir

    ; Skip "." entry
    cmp ecx, 1
    jne @f
    cmp byte [r8 + 8], '.'
    je .skip_ent
@@:
    ; Skip ".." entry
    cmp ecx, 2
    jne @f
    cmp word [r8 + 8], '..'
    je .skip_ent
@@:

    mov r12d, [r15 + files_count - kmain]
    cmp r12d, FILES_MAX_ROWS
    jae .scan_done

    ; Store inode and type
    mov [r15 + files_entry_inode - kmain + r12*4], eax
    mov [r15 + files_entry_type - kmain + r12], bl

    ; Copy name (up to 30 chars) into files_entry_name + r12*32
    imul edx, r12d, 32
    lea rdi, [r15 + files_entry_name - kmain + rdx]
    cmp ecx, 30
    jbe @f
    mov ecx, 30
@@:
    lea rsi, [r8 + 8]
    push rcx
    rep movsb
    mov byte [rdi], 0
    pop rcx

    ; Read file size by loading its inode (reads into ext4_nbuf, preserves ext4_bbuf!)
    push r8
    push r9
    mov eax, [r8]                       ; file inode number
    call ext4_inode_load
    jc .sz_err
    mov rax, qword [r15 + ext4_inode - kmain + IN_OFF_SIZE]
    jmp .sz_ok
.sz_err:
    xor eax, eax
.sz_ok:
    mov [r15 + files_entry_size - kmain + r12*8], rax

    ; Format size string into files_entry_str_sz + r12*16
    imul edx, r12d, 16
    lea rdi, [r15 + files_entry_str_sz - kmain + rdx]
    call app_files_format_size

    ; Format inode string into files_entry_str_in + r12*16: "inode " + dec
    imul edx, r12d, 16
    lea rdi, [r15 + files_entry_str_in - kmain + rdx]
    mov dword [rdi], 'inod'
    mov word [rdi+4], 'e '
    add rdi, 6
    mov eax, [r15 + files_entry_inode - kmain + r12*4]
    call app_files_u64_to_dec

    pop r9
    pop r8

    inc dword [r15 + files_count - kmain]

.skip_ent:
    movzx eax, word [r8 + 4]            ; rec_len
    test ax, ax
    jz .scan_done
    add r8, rax
    jmp .parse_ent

.lb_done:
    inc dword [r15 + e4t_lb - kmain]
    jmp .next_lb

.scan_done:
    call app_files_update_stat

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

    ; Initialize on first run
    cmp byte [r15 + files_inited - kmain], 1
    je @f
    mov byte [r15 + files_inited - kmain], 1
    call app_files_refresh_ext4
@@:

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
    lea rsi, [r15 + files_cwd_disp - kmain]
    cmp byte [rsi], 0
    jne @f
    lea rsi, [r15 + str_files_path_def - kmain]
@@:
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
    cmp byte [r15 + ext4_ok - kmain], 1
    jne .vol_none
    lea rsi, [r15 + str_files_vol_ok - kmain]
    jmp .vol_draw
.vol_none:
    lea rsi, [r15 + str_files_vol_none - kmain]
.vol_draw:
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

    ; --- 3. Directory Rows ---
    mov eax, [r15 + files_count - kmain]
    test eax, eax
    jnz .has_rows

    ; Empty table message
    mov ecx, r8d
    add ecx, 24
    mov edx, r9d
    add edx, 120
    mov eax, 0xFF64748B
    lea rsi, [r15 + str_files_empty - kmain]
    call r2d_draw_text_ui_aa
    jmp .rows_done

.has_rows:
    xor r12d, r12d                      ; row = 0..files_count-1
.row_loop:
    cmp r12d, [r15 + files_count - kmain]
    jae .rows_done
    cmp r12d, FILES_MAX_ROWS
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
    ; Draw TYPE tag
    push rdx
    mov ecx, r8d
    add ecx, 24
    cmp byte [r15 + files_entry_type - kmain + r12], 2
    jne .file_tag
    mov eax, 0xFF38BDF8                 ; Sky Blue [DIR]
    lea rsi, [r15 + str_files_dir_tag - kmain]
    jmp .do_tag
.file_tag:
    mov eax, 0xFF10B981                 ; Emerald [FILE]
    lea rsi, [r15 + str_files_file_tag - kmain]
.do_tag:
    call r2d_draw_text_mono_aa

    ; Draw NAME
    mov ecx, r8d
    add ecx, 90
    imul eax, r12d, 32
    lea rsi, [r15 + files_entry_name - kmain + rax]
    mov eax, 0xFFF8FAFC
    call r2d_draw_text_ui_aa

    ; Draw SIZE
    mov ecx, r8d
    add ecx, 250
    imul eax, r12d, 16
    lea rsi, [r15 + files_entry_str_sz - kmain + rax]
    mov eax, 0xFF94A3B8
    call r2d_draw_text_mono_aa

    ; Draw INODE
    mov ecx, r8d
    add ecx, 370
    imul eax, r12d, 16
    lea rsi, [r15 + files_entry_str_in - kmain + rax]
    mov eax, 0xFF94A3B8
    call r2d_draw_text_mono_aa
    pop rdx

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
    lea rsi, [r15 + files_stat_buf - kmain]
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
; ECX = mouse_x, EDX = mouse_y, R8D = wx, R9D = wy
; Returns: AL = 1 if handled/selection changed, 0 otherwise
; ==============================================================================
app_files_click:
    push rbx
    push rcx
    push rdx

    ; Check if Refresh button clicked (x in [156, 230], y in [42, 68])
    mov eax, ecx
    sub eax, r8d                        ; rel_x = mouse_x - wx
    mov edx, [rsp + 0]                  ; restore mouse_y
    sub edx, r9d                        ; rel_y = mouse_y - wy

    cmp eax, 156
    jb .chk_rows
    cmp eax, 230
    ja .chk_rows
    cmp edx, 42
    jb .chk_rows
    cmp edx, 68
    ja .chk_rows

    ; Refresh clicked!
    call app_files_refresh_ext4
    mov al, 1
    jmp .click_done

.chk_rows:
    ; Row list area: rel_y in [104, 104 + files_count * 32]
    cmp edx, 104
    jb .click_none
    mov eax, [r15 + files_count - kmain]
    test eax, eax
    jz .click_none
    shl eax, 5                          ; files_count * 32
    add eax, 104
    cmp edx, eax
    jae .click_none

    sub edx, 104
    shr edx, 5                          ; divide by 32 -> row index
    mov [r15 + files_selected_row - kmain], edx
    call app_files_update_stat
    mov al, 1
    jmp .click_done

.click_none:
    xor al, al
.click_done:
    pop rdx
    pop rcx
    pop rbx
    ret
