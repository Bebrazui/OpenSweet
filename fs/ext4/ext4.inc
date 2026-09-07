; Opensweet OS - minimal ext4 read-only driver (own implementation).
; Design notes: docs/EXT4.md. Requires ATA driver included first.
; Conventions: R14=0 base for low consts, R15=image base.
; v1 scope: extents only (depth 0..1), linear dirs, no journal/CRC.

E4_INCOMPAT_MASK = 0x2 or 0x40 or 0x80 or 0x200   ; FILETYPE|EXTENTS|64BIT|FLEX_BG

SB_MAGIC    = 0x38
SB_FDB      = 0x14
SB_LOG_BS   = 0x18
SB_BPG      = 0x20
SB_IPG      = 0x28
SB_ISIZE    = 0x58
SB_INCOMPAT = 0x60

IN_OFF_MODE   = 0
IN_OFF_SIZE   = 4
IN_OFF_BLOCKS = 28
IN_OFF_FLAGS  = 32
IN_OFF_BLOCK  = 40
EXT_FLAG      = 0x80000
EH_MAGIC      = 0xF30A

; ================= mount =================
ext4_mount:                       ; CF=0 ok
    push rdi
    push rcx
    lea rdi, [r15 + ext4_sb - kmain]
    mov eax, 2                    ; superblock at byte 1024 -> LBA 2
    mov ecx, 2
    call disk_read_blocks
    jc .fail

    cmp word [r15 + ext4_sb - kmain + SB_MAGIC], 0xEF53
    jne .fail

    mov eax, dword [r15 + ext4_sb - kmain + SB_INCOMPAT]
    mov ecx, E4_INCOMPAT_MASK
    not ecx                       ; ecx = unsupported bits mask
    and eax, ecx
    jnz .fail

    mov eax, dword [r15 + ext4_sb - kmain + SB_LOG_BS]
    cmp eax, 6
    ja .fail
    mov dword [r15 + ext4_lbs - kmain], eax
    mov eax, dword [r15 + ext4_sb - kmain + SB_BPG]
    test eax, eax
    jz .fail
    mov dword [r15 + ext4_bpg - kmain], eax
    mov eax, dword [r15 + ext4_sb - kmain + SB_IPG]
    test eax, eax
    jz .fail
    mov dword [r15 + ext4_ipg - kmain], eax
    movzx eax, word [r15 + ext4_sb - kmain + SB_ISIZE]
    test eax, eax
    jz .fail
    mov dword [r15 + ext4_isize - kmain], eax
    mov eax, dword [r15 + ext4_sb - kmain + SB_FDB]
    inc eax
    mov [r15 + ext4_gdtb - kmain], eax   ; GDT block (v1: one block)
    mov byte [r15 + ext4_ok - kmain], 1
    clc
.pop:
    pop rcx
    pop rdi
    ret
.fail:
    stc
    jmp .pop

; ================= read one FS block =================
; RAX = block, RSI = dest buffer. Preserves rbx,rsi,rdi,r8+.
ext4_blk_read:
    push rcx
    push rdx
    push rdi
    push rax
    push rsi

    mov rdi, rsi                  ; destination buffer
    movzx ecx, byte [r15 + ext4_lbs - kmain]
    inc ecx                       ; shift count = lbs + 1
    shl rax, cl                   ; LBA = block << (lbs+1)
    mov edx, 1
    shl edx, cl                   ; sectors per block = 1 << (lbs+1)
    mov ecx, edx                  ; count
    call disk_read_blocks

    pop rsi
    pop rax
    pop rdi
    pop rdx
    pop rcx
    ret

; ================= load inode =================
; RAX = inode number -> ext4_inode filled. CF=1 on error.
ext4_inode_load:
    push rcx
    push rdx
    push rsi
    push rdi
    dec eax
    xor edx, edx
    div dword [r15 + ext4_ipg - kmain]      ; eax = group, edx = index
    mov dword [r15 + e4t_group - kmain], eax
    mov dword [r15 + e4t_index - kmain], edx

    mov eax, dword [r15 + ext4_gdtb - kmain]
    lea rsi, [r15 + ext4_nbuf - kmain]
    call ext4_blk_read
    jc .err

    mov ecx, dword [r15 + e4t_group - kmain]
    shl ecx, 5                              ; group * 32B descriptor
    lea rsi, [r15 + ext4_nbuf - kmain]
    mov eax, [rsi + rcx + 8]                ; bg_inode_table_lo
    mov dword [r15 + e4t_itable - kmain], eax

    mov eax, dword [r15 + e4t_index - kmain]
    mul dword [r15 + ext4_isize - kmain]    ; edx:eax = byteoff in table
    test edx, edx
    jnz .err
    mov dword [r15 + e4t_byteoff - kmain], eax

    mov ecx, dword [r15 + ext4_lbs - kmain]
    add ecx, 10
    shr eax, cl                             ; blk_in_table
    add eax, [r15 + e4t_itable - kmain]
    lea rsi, [r15 + ext4_nbuf - kmain]
    call ext4_blk_read
    jc .err

    mov ecx, dword [r15 + ext4_lbs - kmain]
    mov edx, 1024
    shl edx, cl                             ; block size in bytes (1024 << lbs)
    dec edx                                 ; mask (e.g. 1023)
    and edx, [r15 + e4t_byteoff - kmain]    ; within
    lea rsi, [r15 + ext4_nbuf - kmain]
    add rsi, rdx
    lea rdi, [r15 + ext4_inode - kmain]
    mov ecx, dword [r15 + ext4_isize - kmain]
    rep movsb
    clc
.eil_pop:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    ret
.err:
    stc
    jmp .eil_pop

; ================= map logical -> physical block =================
; RAX = logical block, uses loaded ext4_inode.
; Out: RAX = phys or -1. Preserves rbx,rsi,rdi,rdx,rcx,r8-r15.
ext4_map:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    test rax, rax
    js .none
    mov edx, dword [r15 + ext4_inode - kmain + IN_OFF_FLAGS]
    test edx, EXT_FLAG
    jz .none                                ; legacy indirect: not in v1
    mov r9d, eax                            ; r9d = logical block (keep safe!)
    lea rsi, [r15 + ext4_inode - kmain + IN_OFF_BLOCK]
.tree:
    cmp word [rsi], EH_MAGIC
    jne .none
    movzx r8d, word [rsi + 2]               ; eh_entries
    test r8d, r8d
    jz .none
    cmp word [rsi + 6], 0                   ; eh_depth
    je .leaf
    lea rdi, [rsi + 12]
    mov dword [r15 + e4t_have - kmain], 0
.scan_idx:
    test r8d, r8d
    jz .descend_chk
    cmp [rdi], r9d                          ; ee_block <= L ?
    ja .idx_skip
    mov [r15 + e4t_best - kmain], rdi
    mov dword [r15 + e4t_have - kmain], 1
.idx_skip:
    add rdi, 12
    dec r8d
    jnz .scan_idx
.descend_chk:
    cmp dword [r15 + e4t_have - kmain], 0
    je .none
    mov dword [r15 + e4t_have - kmain], 0
    mov rdi, [r15 + e4t_best - kmain]
    mov eax, [rdi + 4]                      ; ei_leaf_lo
    lea rsi, [r15 + ext4_nbuf - kmain]
    call ext4_blk_read
    jc .none
    jmp .tree
.leaf:
    lea rdi, [rsi + 12]
.map_scan_leaf:
    test r8d, r8d
    jz .none
    mov edx, [rdi]                          ; ee_block
    movzx ecx, word [rdi + 4]               ; ee_len
    cmp r9d, edx                            ; L < ee_block ?
    jb .leaf_next
    mov r8d, edx                            ; temp: end = block+len
    add r8d, ecx
    cmp r9d, r8d                            ; L >= end ?
    jae .leaf_next
    mov eax, r9d
    sub eax, edx                            ; off = L - ee_block
    mov edx, [rdi + 8]                      ; ee_start_lo (32-bit)
    add rax, rdx                            ; phys = start_lo + off
    jmp .done
.leaf_next:
    add rdi, 12
    dec r8d
    jnz .map_scan_leaf
.none:
    mov rax, -1
.done:
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

; ================= find name in loaded directory inode =================
; RSI = name ptr, RCX = len -> RAX = inode or 0. Preserves rsi,rdi,rbx,r10,r11.
ext4_dir_find:
    push rbx
    push rsi
    push rdi
    push r10
    push r11
    mov ebx, ecx                            ; name length
    mov r11, rsi                            ; keep name pointer safe
    mov dword [r15 + e4t_lb - kmain], 0
.dfb:
    mov eax, dword [r15 + e4t_lb - kmain]
    mov ecx, [r15 + ext4_lbs - kmain]
    add ecx, 10
    shl rax, cl                             ; logical byte offset
    cmp rax, qword [r15 + ext4_inode - kmain + IN_OFF_SIZE]
    jae .notfound
    mov eax, dword [r15 + e4t_lb - kmain]
    call ext4_map
    cmp rax, -1
    je .notfound
    lea rsi, [r15 + ext4_bbuf - kmain]
    call ext4_blk_read
    lea r8, [r15 + ext4_bbuf - kmain]
    mov ecx, [r15 + ext4_lbs - kmain]
    mov edx, 1024
    shl edx, cl                             ; block size in bytes
    mov r9, r8
    add r9, rdx                             ; block end
.ent:
    cmp r8, r9
    jae .next_blk
    mov eax, [r8]                           ; entry inode
    test eax, eax
    jz .skip
    movzx eax, byte [r8 + 6]                ; name_len
    cmp eax, ebx
    jne .skip
    lea r10, [r8 + 8]                       ; entry name
    mov rcx, rbx                            ; compare length
    xor edx, edx                            ; match flag
    mov rsi, r11                            ; rsi = wanted name pointer
.cmp_loop:
    test ecx, ecx
    jz .matched
    mov al, [r10]
    cmp al, [rsi]
    jne .cmp_diff
    inc r10
    inc rsi
    dec ecx
    jmp .cmp_loop
.matched:
    mov edx, 1
.cmp_diff:
    test edx, edx
    jnz .found
.skip:
    movzx eax, word [r8 + 4]                ; rec_len
    test ax, ax
    jz .next_blk
    add r8, rax
    jmp .ent
.next_blk:
    inc dword [r15 + e4t_lb - kmain]
    jmp .dfb
.found:
    mov eax, [r8]                           ; entry inode
.pop_all:
    pop r11
    pop r10
    pop rdi
    pop rsi
    pop rbx
    ret
.notfound:
    xor eax, eax
    jmp .pop_all

; ================= resolve path "/a/b/c" =================
; RSI = ASCIIZ path -> RAX = inode or 0. Path buffer is not modified.
ext4_lookup:
    push rbx
    push rsi
    push rdi
    mov ebx, [r15 + ext4_cwd_inode - kmain] ; default to cwd
    cmp byte [rsi], '/'
    jne .have_comp
    mov ebx, 2                              ; absolute -> root inode
.skip_slash:
    cmp byte [rsi], '/'
    jne .have_comp
    inc rsi
    jmp .skip_slash
.have_comp:
    cmp byte [rsi], 0
    je .done
    ; extract component into e4t_name
    lea rdi, [r15 + e4t_name - kmain]
    xor ecx, ecx
.extract:
    mov al, [rsi]
    test al, al
    je .extracted
    cmp al, '/'
    je .extracted
    mov [rdi], al
    inc rdi
    inc rsi
    inc ecx
    cmp ecx, 255
    jb .extract
.extracted:
    push rsi                                ; preserve path pointer!
    mov eax, ebx
    call ext4_inode_load                    ; load current dir
    jc .fail_pop
    lea rsi, [r15 + e4t_name - kmain]
    call ext4_dir_find
    test eax, eax
    jz .fail_pop
    mov ebx, eax                            ; new cur
    call ext4_inode_load
    jc .fail_pop
    pop rsi                                 ; restore path pointer!
    jmp .skip_slash                         ; handles '/' and end-of-path
.done:
    mov eax, ebx
.pop3:
    pop rdi
    pop rsi
    pop rbx
    ret
.fail_pop:
    pop rsi
.fail0:
    xor eax, eax
    jmp .pop3

; ================= print names of loaded directory =================
ext4_inode_load_auto:
    mov eax, 2
    jmp ext4_inode_load
ext4_ls_print:                    ; loaded dir inode
    push rbx
    push rsi
    push rdi
    push r8
    push r9
    push r12
    push r13

    mov dword [r15 + e4t_lb - kmain], 0
.lsb:
    mov eax, dword [r15 + e4t_lb - kmain]
    mov ecx, dword [r15 + ext4_lbs - kmain]
    add ecx, 10
    shl rax, cl
    cmp rax, qword [r15 + ext4_inode - kmain + IN_OFF_SIZE]
    jae .lsdone
    mov eax, dword [r15 + e4t_lb - kmain]
    call ext4_map
    cmp rax, -1
    je .lsdone
    lea rsi, [r15 + ext4_bbuf - kmain]
    call ext4_blk_read
    lea r8, [r15 + ext4_bbuf - kmain]
    mov ecx, dword [r15 + ext4_lbs - kmain]
    mov edx, 1024
    shl edx, cl                             ; block size in bytes
    mov r9, r8
    add r9, rdx
.lent:
    cmp r8, r9
    jae .lnext_blk
    mov eax, [r8]                           ; entry inode
    test eax, eax
    jz .lnext
    movzx ecx, byte [r8 + 6]                ; name length
    test ecx, ecx
    jz .lnext
    movzx ebx, byte [r8 + 7]                ; file_type: 1=file, 2=dir

    ; Print prefix: [DIR] in Cyan or [FILE] in White
    cmp bl, 2
    je .is_dir
    mov eax, FB_CLR_FILE
    call fb_console_set_color
    lea rsi, [r15 + str_ls_file - kmain]
    call puts
    jmp .print_name

.is_dir:
    mov eax, FB_CLR_DIR
    call fb_console_set_color
    lea rsi, [r15 + str_ls_dir - kmain]
    call puts

.print_name:
    movzx ecx, byte [r8 + 6]
    lea rsi, [r8 + 8]
.lname:
    push rcx
    lodsb
    call putc
    pop rcx
    dec ecx
    jnz .lname

    mov al, 10
    call putc

.lnext:
    movzx eax, word [r8 + 4]                ; rec_len
    test ax, ax
    jz .lsdone
    add r8, rax
    jmp .lent
.lnext_blk:
    inc dword [r15 + e4t_lb - kmain]
    jmp .lsb
.lsdone:
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    pop r13
    pop r12
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbx
    ret

; ================= print file content (loaded inode) =================
ext4_cat_print:
    push rbx
    push rcx
    push rdx
    push rsi
    mov eax, dword [r15 + ext4_inode - kmain + IN_OFF_SIZE]
    test eax, eax
    jz .cat_done
    cmp eax, 4096
    jbe .sz_ok
    mov eax, 4096
.sz_ok:
    mov [r15 + e4t_left - kmain], eax       ; bytes to print
    mov dword [r15 + e4t_lb - kmain], 0
.cat_blk:
    mov eax, dword [r15 + e4t_left - kmain]
    test eax, eax
    jz .cat_done
    mov eax, dword [r15 + e4t_lb - kmain]
    call ext4_map
    cmp rax, -1
    je .cat_done
    lea rsi, [r15 + ext4_bbuf - kmain]
    call ext4_blk_read
    mov ecx, dword [r15 + ext4_lbs - kmain]
    mov edx, 1024
    shl edx, cl                             ; blocksize in bytes
    mov ebx, dword [r15 + e4t_left - kmain]
    cmp edx, ebx
    jbe .cap
    mov edx, ebx
.cap:
    lea rsi, [r15 + ext4_bbuf - kmain]
    sub [r15 + e4t_left - kmain], edx
.cat_chars:
    lodsb
    call e4_putc
    dec edx
    jnz .cat_chars
    inc dword [r15 + e4t_lb - kmain]
    jmp .cat_blk
.cat_done:
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

; ================= read file into memory =================
; Inputs:
;   RSI = ASCIIZ file path
;   RDI = destination buffer
;   RCX = max bytes to read
; Outputs:
;   RAX = bytes read (0 on error)
;   CF  = 0 on success, 1 on error
ext4_read_file_to_mem:
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push rcx                        ; [rsp] = max bytes

    ; Step 1: lookup file path
    call ext4_lookup
    test eax, eax
    jz .rf_err_pop

    ; Step 2: load inode
    call ext4_inode_load
    jc .rf_err_pop

    ; Step 3: verify it is a regular file (i_mode & 0xF000 == 0x8000)
    movzx edx, word [r15 + ext4_inode - kmain + IN_OFF_MODE]
    and edx, 0xF000
    cmp edx, 0x8000
    jne .rf_err_pop

    ; Step 4: get file size
    mov rax, qword [r15 + ext4_inode - kmain + IN_OFF_SIZE]
    test rax, rax
    jz .rf_zero

    pop rcx                         ; rcx = max bytes
    push rcx
    cmp rax, rcx
    jbe .rf_sz_ok
    mov rax, rcx                    ; clamp to max bytes
.rf_sz_ok:
    mov r12, rax                    ; r12 = total bytes to read
    mov r13, 0                      ; r13 = bytes read so far
    mov dword [r15 + e4t_lb - kmain], 0

    ; Block size in bytes = 1024 << ext4_lbs
    mov ecx, dword [r15 + ext4_lbs - kmain]
    mov r14d, 1024
    shl r14d, cl                    ; r14d = block size

.rf_blk_loop:
    cmp r13, r12
    jae .rf_success

    ; map logical block to physical block
    mov eax, dword [r15 + e4t_lb - kmain]
    call ext4_map
    cmp rax, -1
    je .rf_sparse

    ; Calculate remaining bytes to read
    mov rbx, r12
    sub rbx, r13                    ; rbx = bytes left to read

    ; If bytes left >= block size, read directly into [rdi + r13]
    cmp rbx, r14
    jb .rf_tail

    ; Direct read into destination!
    lea rsi, [rdi + r13]
    call ext4_blk_read
    jc .rf_err_pop
    add r13, r14
    inc dword [r15 + e4t_lb - kmain]
    jmp .rf_blk_loop

.rf_tail:
    ; Read block into temporary bounce buffer, then copy remainder
    lea rsi, [r15 + ext4_bbuf - kmain]
    call ext4_blk_read
    jc .rf_err_pop

    ; Copy rbx bytes from ext4_bbuf to [rdi + r13]
    push rdi
    push rsi
    lea rdi, [rdi + r13]
    lea rsi, [r15 + ext4_bbuf - kmain]
    mov rcx, rbx
    rep movsb
    pop rsi
    pop rdi
    add r13, rbx
    jmp .rf_success

.rf_sparse:
    ; Hole/sparse block: zero-fill block size or remainder
    mov rbx, r12
    sub rbx, r13
    cmp rbx, r14
    jbe .rf_sparse_tail
    mov rbx, r14
.rf_sparse_tail:
    push rdi
    lea rdi, [rdi + r13]
    xor eax, eax
    mov rcx, rbx
    rep stosb
    pop rdi
    add r13, rbx
    inc dword [r15 + e4t_lb - kmain]
    jmp .rf_blk_loop

.rf_success:
    pop rcx
    mov rax, r12                    ; return total bytes read
    clc
    jmp .rf_done

.rf_zero:
    pop rcx
    xor eax, eax
    clc
    jmp .rf_done

.rf_err_pop:
    pop rcx
.rf_err:
    xor eax, eax
    stc
.rf_done:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

; ================= change working directory =================
; RSI = path string (e.g. "/" or "subdir" or ".." or empty for root)
ext4_cd:
    push rbx
    push rsi
    push rdi

    ; Skip leading spaces
.cd_skip_sp:
    cmp byte [rsi], ' '
    jne .cd_sp_done
    inc rsi
    jmp .cd_skip_sp
.cd_sp_done:

    ; If empty or "~", cd to root
    cmp byte [rsi], 0
    je .cd_root
    cmp byte [rsi], '~'
    je .cd_root

    call ext4_lookup
    test eax, eax
    jz .cd_not_found

    ; Verify it is a directory (i_mode & 0xF000 == 0x4000)
    push rax
    call ext4_inode_load
    jc .cd_err_pop
    movzx edx, word [r15 + ext4_inode - kmain + IN_OFF_MODE]
    and edx, 0xF000
    cmp edx, 0x4000
    jne .cd_not_dir
    pop rax

    ; It is a directory! Update cwd_inode
    mov [r15 + ext4_cwd_inode - kmain], eax

    ; If target was absolute (starts with '/')
    cmp byte [rsi], '/'
    je .set_abs_path
    ; If target is "."
    cmp word [rsi], 0x002E
    je .cd_done
    ; If target is ".."
    cmp word [rsi], 0x2E2E
    jne .append_rel_path

    ; Handle ".." path truncation
    lea rdi, [r15 + ext4_cwd_path - kmain]
    xor ecx, ecx
.find_end:
    cmp byte [rdi + rcx], 0
    je .found_end
    inc ecx
    jmp .find_end
.found_end:
.scan_back:
    cmp ecx, 1
    jbe .set_root_str
    dec ecx
    cmp byte [rdi + rcx], '/'
    jne .scan_back
    mov byte [rdi + rcx], 0
    jmp .cd_done

.set_root_str:
    mov word [r15 + ext4_cwd_path - kmain], 0x002F ; "/"
    jmp .cd_done

.set_abs_path:
    lea rdi, [r15 + ext4_cwd_path - kmain]
    xor ecx, ecx
.copy_abs:
    lodsb
    stosb
    test al, al
    jz .cd_done
    inc ecx
    cmp ecx, 60
    jb .copy_abs
    mov byte [rdi], 0
    jmp .cd_done

.append_rel_path:
    lea rdi, [r15 + ext4_cwd_path - kmain]
    xor ecx, ecx
.find_rel_end:
    cmp byte [rdi + rcx], 0
    je .rel_end_found
    inc ecx
    jmp .find_rel_end
.rel_end_found:
    cmp ecx, 1
    je .just_copy
    mov byte [rdi + rcx], '/'
    inc ecx
.just_copy:
    lea rdi, [rdi + rcx]
.copy_rel:
    lodsb
    stosb
    test al, al
    jz .cd_done
    inc ecx
    cmp ecx, 60
    jb .copy_rel
    mov byte [rdi], 0
    jmp .cd_done

.cd_root:
    mov dword [r15 + ext4_cwd_inode - kmain], 2
    mov word [r15 + ext4_cwd_path - kmain], 0x002F ; "/"
.cd_done:
    pop rdi
    pop rsi
    pop rbx
    clc
    ret

.cd_not_dir:
    pop rax
    mov eax, FB_CLR_ERROR
    call fb_console_set_color
    lea rsi, [r15 + str_cd_notdir - kmain]
    call puts
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    pop rdi
    pop rsi
    pop rbx
    stc
    ret

.cd_err_pop:
    pop rax
.cd_not_found:
    mov eax, FB_CLR_ERROR
    call fb_console_set_color
    lea rsi, [r15 + str_cd_notfound - kmain]
    call puts
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    pop rdi
    pop rsi
    pop rbx
    stc
    ret

; ================= print working directory =================
ext4_pwd:
    push rsi
    lea rsi, [r15 + ext4_cwd_path - kmain]
    call puts
    mov al, 10
    call putc
    pop rsi
    ret

; ================= display detailed file stat =================
ext4_stat_print:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

.st_skip_sp:
    cmp byte [rsi], ' '
    jne .st_sp_done
    inc rsi
    jmp .st_skip_sp
.st_sp_done:

    cmp byte [rsi], 0
    je .st_usage

    call ext4_lookup
    test eax, eax
    jz .st_notfound

    mov ebx, eax                      ; inode number
    call ext4_inode_load
    jc .st_notfound

    mov eax, FB_CLR_HEADER
    call fb_console_set_color
    lea rsi, [r15 + str_stat_hdr - kmain]
    call puts

    mov eax, FB_CLR_LABEL
    call fb_console_set_color
    lea rsi, [r15 + str_stat_inode - kmain]
    call puts
    mov eax, FB_CLR_NUMBER
    call fb_console_set_color
    mov eax, ebx
    call putdec64
    mov al, 10
    call putc

    mov eax, FB_CLR_LABEL
    call fb_console_set_color
    lea rsi, [r15 + str_stat_size - kmain]
    call puts
    mov eax, FB_CLR_NUMBER
    call fb_console_set_color
    mov rax, qword [r15 + ext4_inode - kmain + IN_OFF_SIZE]
    call putdec64
    lea rsi, [r15 + str_stat_bytes - kmain]
    call puts

    mov eax, FB_CLR_LABEL
    call fb_console_set_color
    lea rsi, [r15 + str_stat_blocks - kmain]
    call puts
    mov eax, FB_CLR_NUMBER
    call fb_console_set_color
    mov eax, dword [r15 + ext4_inode - kmain + IN_OFF_BLOCKS]
    call putdec64
    lea rsi, [r15 + str_stat_sectors - kmain]
    call puts

    mov eax, FB_CLR_LABEL
    call fb_console_set_color
    lea rsi, [r15 + str_stat_mode - kmain]
    call puts
    mov eax, FB_CLR_NUMBER
    call fb_console_set_color
    movzx eax, word [r15 + ext4_inode - kmain + IN_OFF_MODE]
    call puthex16
    mov al, 10
    call putc

    ; Extents details
    lea rsi, [r15 + ext4_inode - kmain + IN_OFF_BLOCK]
    cmp word [rsi], 0xF30A            ; ext4 extent magic
    jne .st_no_ext
    mov eax, FB_CLR_SUCCESS
    call fb_console_set_color
    lea rsi, [r15 + str_stat_ext_magic - kmain]
    call puts
    mov eax, FB_CLR_LABEL
    call fb_console_set_color
    lea rsi, [r15 + str_stat_ext_depth - kmain]
    call puts
    mov eax, FB_CLR_NUMBER
    call fb_console_set_color
    movzx eax, word [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 6]
    call putdec64
    lea rsi, [r15 + str_stat_ext_entries - kmain]
    call puts
    mov eax, FB_CLR_NUMBER
    call fb_console_set_color
    movzx eax, word [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 2]
    call putdec64
    mov al, 10
    call putc
    jmp .st_done

.st_no_ext:
    mov eax, FB_CLR_MUTED
    call fb_console_set_color
    lea rsi, [r15 + str_stat_no_ext - kmain]
    call puts

.st_done:
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

.st_usage:
    mov eax, FB_CLR_LABEL
    call fb_console_set_color
    lea rsi, [r15 + str_stat_usage - kmain]
    call puts
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

.st_notfound:
    mov eax, FB_CLR_ERROR
    call fb_console_set_color
    lea rsi, [r15 + str_stat_notfound - kmain]
    call puts
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; ================= data =================
align 16
ext4_sb     rb 1024
ext4_inode  rb 128
ext4_nbuf   rb 1024
ext4_bbuf   rb 1024
ext4_ok     db 0
ext4_lbs    dd 0
ext4_bpg    dd 0
ext4_ipg    dd 0
ext4_isize  dd 0
ext4_fdb    dd 0
ext4_gdtb   dd 0
e4t_group   dd 0
e4t_index   dd 0
e4t_itable  dd 0
e4t_byteoff dd 0
e4t_lb      dd 0
e4t_have    dd 0
e4t_best    dq 0
e4t_left    dd 0
e4t_name    rb 256
e4t_path    rb 256

ext4_cwd_inode dd 2
ext4_cwd_path  db "/", 0
               rb 62

str_cd_notdir   db "cd: not a directory", 10, 0
str_cd_notfound db "cd: no such file or directory", 10, 0
str_ls_dir      db "  [DIR]   ", 0
str_ls_file     db "  [FILE]  ", 0

str_stat_hdr    db "--- ext4 File Information ---", 10, 0
str_stat_inode  db "  Inode:   ", 0
str_stat_size   db "  Size:    ", 0
str_stat_bytes  db " bytes", 10, 0
str_stat_blocks db "  Blocks:  ", 0
str_stat_sectors db " (512-byte sectors)", 10, 0
str_stat_mode   db "  Mode:    ", 0
str_stat_ext_magic db "  Extents: Magic=0xF30A (OK)", 10, 0
str_stat_ext_depth db "           Depth=", 0
str_stat_ext_entries db " Entries=", 0
str_stat_no_ext db "  Extents: none", 10, 0
str_stat_usage  db "Usage: stat <file>", 10, 0
str_stat_notfound db "stat: file not found", 10, 0


; ================= serial wrappers (resolve from this file scope) =================
e4_putc:
    jmp putc
e4_puts:
    jmp puts