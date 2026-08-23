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

IN_OFF_FLAGS = 32
IN_OFF_SIZE  = 4
IN_OFF_BLOCK = 40
EXT_FLAG     = 0x80000
EH_MAGIC     = 0xF30A

; ================= mount =================
ext4_mount:                       ; CF=0 ok
    push rdi
    push rcx
    lea rdi, [r15 + ext4_sb - kmain]
    mov eax, 2                    ; superblock at byte 1024 -> LBA 2
    mov ecx, 2
    call disk_read_blocks
    jc .fail
    ; DEBUG: magic word -> COM1 as 2 hex digits + raw bytes
    mov al, 'B'
    call e4_putc
    mov bl, byte [r15 + ext4_sb - kmain + SB_MAGIC]
    mov cl, byte [r15 + ext4_sb - kmain + SB_MAGIC + 1]
    mov al, bl
    call e4_putc
    mov al, cl
    call e4_putc
    mov al, ' '
    call e4_putc
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
    mov al, '4'
    call e4_putc
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
    mov ecx, 1
    mov cl, byte [r15 + ext4_lbs - kmain]
    inc cl
    shl ecx, cl                   ; sectors per block = 2 << lbs
    push rcx
    shl rax, cl                   ; LBA = block << (lbs+1)
    pop rcx
    pop rdi                       ; dest
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

    mov rax, qword [r15 + ext4_gdtb - kmain]
    lea rsi, [r15 + ext4_nbuf - kmain]
    call ext4_blk_read
    jc .err

    mov ecx, dword [r15 + e4t_group - kmain]
    shl ecx, 5                              ; group * 32B descriptor
    lea rsi, [r15 + ext4_nbuf - kmain]
    mov eax, [rsi + rcx + 8]                ; bg_inode_table_lo
    mov dword [r15 + e4t_itable - kmain], eax

    mov al, 'g'
    call e4_putc
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
    inc ecx
    mov edx, 1
    shl edx, cl                             ; blocksize
    dec edx                                 ; mask
    and edx, [r15 + e4t_byteoff - kmain]    ; within
    lea rsi, [r15 + ext4_nbuf - kmain]
    add rsi, rdx
    lea rdi, [r15 + ext4_inode - kmain]
    mov ecx, dword [r15 + ext4_isize - kmain]
    rep movsb
    mov al, 'L'
    call e4_putc
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
; Out: RAX = phys or -1, RDX = run. Preserves rbx,rsi,rdi,r8-r15.
ext4_map:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    test rax, rax
    js .none
    mov edx, dword [r15 + ext4_inode - kmain + IN_OFF_FLAGS]
    test edx, EXT_FLAG
    jz .none                                ; legacy indirect: not in v1
    mov al, 'M'
    call e4_putc
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
    cmp dword [rdi], eax                    ; ee_block <= L ?
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
    cmp eax, edx                            ; L < ee_block ?
    jb .leaf_next
    mov r8d, edx                            ; temp: end = block+len
    add r8d, ecx
    cmp eax, r8d                            ; L >= end ?
    jae .leaf_next
    sub eax, edx                            ; off = L - ee_block
    mov rdx, rcx                            ; len
    sub rdx, rax                            ; run = len - off
    add rax, [rdi + 8]                      ; phys = start_lo + off
    jmp .done
.leaf_next:
    add rdi, 12
    dec r8d
    jnz .map_scan_leaf
.none:
    mov al, 'X'
    call e4_putc
    mov rax, -1
    xor edx, edx
.done:
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
    inc ecx
    mov edx, 1
    shl edx, cl
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
.cmp_loop:
    test ecx, ecx
    jz .matched
    mov al, [r10]
    cmp al, [r11]
    jne .cmp_diff
    inc r10
    inc r11
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
    mov ebx, 2                              ; cur inode = root
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
    mov eax, ebx
    call ext4_inode_load                    ; load current dir
    jc .fail0
    lea rsi, [r15 + e4t_name - kmain]
    call ext4_dir_find
    test eax, eax
    jz .fail0
    mov ebx, eax                            ; new cur
    call ext4_inode_load
    jc .fail0
    jmp .skip_slash                         ; handles '/' and end-of-path
.done:
    mov eax, ebx
.pop3:
    pop rdi
    pop rsi
    pop rbx
    ret
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
    inc ecx
    mov edx, 1
    shl edx, cl
    mov r9, r8
    add r9, rdx
.lent:
    cmp r8, r9
    jae .lnext
    mov eax, [r8]
    test eax, eax
    jmp .lnext
    movzx ecx, byte [r8 + 6]                ; name length
    lea rsi, [r8 + 8]
.lname:
    push rcx
    lodsb
    call e4_putc
    pop rcx
    dec ecx
    jnz .lname
    mov al, ' '
    call e4_putc
    jmp .lnext
.lnext:
    movzx eax, word [r8 + 4]
    test ax, ax
    jz .lsdone
    add r8, rax
    jmp .lent
.lsdone:
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
    inc ecx
    mov edx, 1
    shl edx, cl                             ; blocksize
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


; ================= serial wrappers (resolve from this file scope) =================
e4_putc:
    jmp putc
e4_puts:
    jmp puts