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

    ; C2 Mitigation: Only 1024-byte blocks (SB_LOG_BS == 0) are supported
    ; by the static 1024-byte ext4_nbuf/ext4_bbuf/ext4_wbuf buffer design.
    mov eax, dword [r15 + ext4_sb - kmain + SB_LOG_BS]
    test eax, eax
    jnz .fail
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

    ; Calculate groups count: (s_blocks_count_lo - s_first_data_block + bpg - 1) / bpg
    mov eax, dword [r15 + ext4_sb - kmain + 0x04] ; s_blocks_count_lo
    sub eax, dword [r15 + ext4_sb - kmain + SB_FDB]
    add eax, dword [r15 + ext4_bpg - kmain]
    dec eax
    xor edx, edx
    div dword [r15 + ext4_bpg - kmain]
    test eax, eax
    jnz @f
    mov eax, 1
@@:
    mov dword [r15 + ext4_groups_count - kmain], eax

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

; ================= read N contiguous FS blocks =================
; RAX = start block, RCX = block count, RSI = dest buffer. Preserves rbx,rsi,rdi,r8+.
ext4_blks_read:
    test rcx, rcx
    jz .blks_ok
    push rcx
    push rdx
    push rdi
    push rax
    push rsi

    mov rdi, rsi                  ; destination buffer
    push rcx                      ; save block count
    movzx ecx, byte [r15 + ext4_lbs - kmain]
    inc ecx                       ; shift count = lbs + 1
    shl rax, cl                   ; LBA = start block << (lbs+1)
    pop rdx                       ; rdx = block count
    shl rdx, cl                   ; rdx = total sectors = count << (lbs+1)
    mov ecx, edx                  ; rcx = sector count
    call disk_read_blocks

    pop rsi
    pop rax
    pop rdi
    pop rdx
    pop rcx
    ret
.blks_ok:
    clc
    ret

; ================= write one FS block =================
; RAX = block, RSI = src buffer. Preserves rbx,rsi,rdi,r8+.
ext4_blk_write:
    push rcx
    push rdx
    push rax
    push rsi

    movzx ecx, byte [r15 + ext4_lbs - kmain]
    inc ecx                       ; shift count = lbs + 1
    shl rax, cl                   ; LBA = block << (lbs+1)
    mov edx, 1
    shl edx, cl                   ; sectors per block = 1 << (lbs+1)
    mov ecx, edx                  ; count
    call disk_write_blocks

    pop rsi
    pop rax
    pop rdx
    pop rcx
    ret

; ================= write superblock to disk =================
ext4_sb_save:
    push rax
    push rcx
    push rsi
    mov eax, 2                    ; superblock at byte 1024 -> LBA 2
    mov ecx, 2                    ; 2 sectors (1024 bytes)
    lea rsi, [r15 + ext4_sb - kmain]
    call disk_write_blocks
    pop rsi
    pop rcx
    pop rax
    ret

; ================= write GDT block to disk =================
ext4_gdt_save:
    push rax
    push rsi
    mov eax, dword [r15 + ext4_gdtb - kmain]
    lea rsi, [r15 + ext4_nbuf - kmain]
    call ext4_blk_write
    pop rsi
    pop rax
    ret

; ================= save inode =================
; RAX = inode number, uses ext4_inode. CF=1 on error.
ext4_inode_save:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    cmp dword [r15 + ext4_ipg - kmain], 0
    je .eis_err
    test eax, eax
    jle .eis_err
    dec eax
    xor edx, edx
    div dword [r15 + ext4_ipg - kmain]      ; eax = group, edx = index
    mov dword [r15 + e4t_group - kmain], eax
    mov dword [r15 + e4t_index - kmain], edx

    ; Read GDT block
    mov eax, dword [r15 + ext4_gdtb - kmain]
    lea rsi, [r15 + ext4_nbuf - kmain]
    call ext4_blk_read
    jc .eis_err

    ; Get inode table block of group
    mov ecx, dword [r15 + e4t_group - kmain]
    shl ecx, 5                              ; group * 32B descriptor
    lea rsi, [r15 + ext4_nbuf - kmain]
    mov eax, [rsi + rcx + 8]                ; bg_inode_table_lo
    mov dword [r15 + e4t_itable - kmain], eax

    ; Byte offset of inode in table
    mov eax, dword [r15 + e4t_index - kmain]
    mul dword [r15 + ext4_isize - kmain]
    test edx, edx
    jnz .eis_err
    mov dword [r15 + e4t_byteoff - kmain], eax

    ; Block of table
    mov ecx, dword [r15 + ext4_lbs - kmain]
    add ecx, 10
    shr eax, cl                             ; blk_in_table
    add eax, [r15 + e4t_itable - kmain]     ; table block number
    mov dword [r15 + e4t_tblk - kmain], eax

    ; Read table block into ext4_nbuf
    lea rsi, [r15 + ext4_nbuf - kmain]
    call ext4_blk_read
    jc .eis_err

    ; Copy ext4_isize bytes from ext4_inode into ext4_nbuf
    mov ecx, dword [r15 + ext4_lbs - kmain]
    mov edx, 1024
    shl edx, cl                             ; block size
    dec edx
    and edx, [r15 + e4t_byteoff - kmain]    ; byte offset within block
    lea rdi, [r15 + ext4_nbuf - kmain + rdx]
    lea rsi, [r15 + ext4_inode - kmain]
    mov ecx, dword [r15 + ext4_isize - kmain]
    cld
    rep movsb

    ; Write table block back to disk!
    mov eax, dword [r15 + e4t_tblk - kmain]
    lea rsi, [r15 + ext4_nbuf - kmain]
    call ext4_blk_write
    jc .eis_err

    clc
.eis_pop:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret
.eis_err:
    stc
    jmp .eis_pop

; ================= allocate N contiguous free blocks =================
; EDI = count of blocks needed
; Out: RAX = starting physical block number, or -1 on error. CF=0 on success, 1 on error.
ext4_alloc_blocks:
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

    test edi, edi
    jz .abk_zero

    mov r8d, edi                            ; r8d = count needed

    xor r12d, r12d                          ; r12d = current group G
.abk_group_loop:
    cmp r12d, dword [r15 + ext4_groups_count - kmain]
    jae .abk_err                            ; no group has enough contiguous space!

    ; Read GDT block
    mov eax, dword [r15 + ext4_gdtb - kmain]
    lea rsi, [r15 + ext4_nbuf - kmain]
    call ext4_blk_read
    jc .abk_err

    ; Group descriptor offset = G * 32
    mov ecx, r12d
    shl ecx, 5                              ; ecx = G * 32
    ; Check bg_free_blocks_count_lo
    movzx edx, word [r15 + ext4_nbuf - kmain + rcx + 0x0C]
    cmp edx, r8d
    jb .abk_next_group                      ; not enough free blocks in this group

    ; Get this group's block bitmap block
    mov eax, dword [r15 + ext4_nbuf - kmain + rcx + 0x00]
    mov dword [r15 + e4w_bmp_blk - kmain], eax

    ; Read block bitmap into ext4_bbuf
    lea rsi, [r15 + ext4_bbuf - kmain]
    call ext4_blk_read
    jc .abk_next_group

    ; Scan ext4_bbuf for run of r8d consecutive 0-bits
    ; Limit is ext4_bpg (e.g. 8192 bits)
    xor r9d, r9d                            ; bit index (0 .. bpg-1)
    xor r10d, r10d                          ; current run length
    xor r11d, r11d                          ; starting bit of current run

.abk_scan_loop:
    cmp r9d, dword [r15 + ext4_bpg - kmain]
    jae .abk_next_group                     ; no contiguous run in this group

    ; Test bit r9d in ext4_bbuf
    mov eax, r9d
    bt dword [r15 + ext4_bbuf - kmain], eax
    jc .abk_bit_is_one

    ; Bit is 0
    test r10d, r10d
    jnz @f
    mov r11d, r9d                           ; start of new run
@@:
    inc r10d
    cmp r10d, r8d
    je .abk_found_run
    inc r9d
    jmp .abk_scan_loop

.abk_bit_is_one:
    xor r10d, r10d
    inc r9d
    jmp .abk_scan_loop

.abk_next_group:
    inc r12d
    jmp .abk_group_loop

.abk_found_run:
    ; Found run starting at bit r11d in group r12d!
    ; Set r8d bits in ext4_bbuf starting at r11d
    xor ecx, ecx
.abk_set_loop:
    cmp ecx, r8d
    jae .abk_set_done
    lea eax, [r11d + ecx]
    bts dword [r15 + ext4_bbuf - kmain], eax
    inc ecx
    jmp .abk_set_loop
.abk_set_done:

    ; Write updated block bitmap back to disk
    mov eax, dword [r15 + e4w_bmp_blk - kmain]
    lea rsi, [r15 + ext4_bbuf - kmain]
    call ext4_blk_write
    jc .abk_err

    ; Re-read GDT into ext4_nbuf
    mov eax, dword [r15 + ext4_gdtb - kmain]
    lea rsi, [r15 + ext4_nbuf - kmain]
    call ext4_blk_read
    jc .abk_err

    ; Decrement free blocks count in GDT descriptor for group r12d
    mov ecx, r12d
    shl ecx, 5
    sub word [r15 + ext4_nbuf - kmain + rcx + 0x0C], r8w
    call ext4_gdt_save

    ; Decrement free blocks in Superblock
    sub dword [r15 + ext4_sb - kmain + 0x0C], r8d
    call ext4_sb_save

    ; Compute physical block number: RAX = r12d * ext4_bpg + r11d
    mov eax, r12d
    mul dword [r15 + ext4_bpg - kmain]
    add eax, r11d
    clc
.abk_pop:
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

.abk_zero:
    xor eax, eax
    clc
    jmp .abk_pop

.abk_err:
    mov rax, -1
    stc
    jmp .abk_pop

; ================= free N contiguous blocks =================
; EDI = starting physical block number, ESI = count of blocks
; Out: CF=0 on success, 1 on error.
ext4_free_blocks:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r12

    test esi, esi
    jz .fbk_ok
    test edi, edi
    jz .fbk_ok

    mov r9d, esi                            ; r9d = count

    mov eax, edi
    xor edx, edx
    div dword [r15 + ext4_bpg - kmain]      ; eax = group G, edx = bit in group
    mov r12d, eax                           ; r12d = group G
    mov r8d, edx                            ; r8d = start bit in group

    ; Read GDT block to find bg_block_bitmap
    mov eax, dword [r15 + ext4_gdtb - kmain]
    lea rsi, [r15 + ext4_nbuf - kmain]
    call ext4_blk_read
    jc .fbk_err

    ; Get group G's block bitmap
    mov ecx, r12d
    shl ecx, 5
    mov eax, dword [r15 + ext4_nbuf - kmain + rcx + 0x00]
    mov dword [r15 + e4w_bmp_blk - kmain], eax

    ; Read block bitmap into ext4_bbuf
    lea rsi, [r15 + ext4_bbuf - kmain]
    call ext4_blk_read
    jc .fbk_err

    ; Clear r9d bits starting at r8d
    xor ecx, ecx
.fbk_clear_loop:
    cmp ecx, r9d
    jae .fbk_clear_done
    lea eax, [r8d + ecx]
    cmp eax, dword [r15 + ext4_bpg - kmain]
    jae .fbk_clear_done
    btr dword [r15 + ext4_bbuf - kmain], eax
    inc ecx
    jmp .fbk_clear_loop
.fbk_clear_done:

    ; Write block bitmap back to disk
    mov eax, dword [r15 + e4w_bmp_blk - kmain]
    lea rsi, [r15 + ext4_bbuf - kmain]
    call ext4_blk_write
    jc .fbk_err

    ; Increment free blocks in GDT descriptor for group r12d
    mov ecx, r12d
    shl ecx, 5
    add word [r15 + ext4_nbuf - kmain + rcx + 0x0C], r9w
    call ext4_gdt_save

    ; Increment free blocks in Superblock
    add dword [r15 + ext4_sb - kmain + 0x0C], r9d
    call ext4_sb_save

.fbk_ok:
    clc
.fbk_pop:
    pop r12
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

.fbk_err:
    stc
    jmp .fbk_pop

; ================= allocate a single free block =================
; Out: RAX = physical block number, or -1 on error.
ext4_alloc_block:
    mov edi, 1
    jmp ext4_alloc_blocks

; ================= allocate a free inode =================
; Out: RAX = inode number (1-based), or -1 on error.
ext4_alloc_inode:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r12
    push r13

    ; Read GDT block
    mov eax, dword [r15 + ext4_gdtb - kmain]
    lea rsi, [r15 + ext4_nbuf - kmain]
    call ext4_blk_read
    jc .ai_err

    xor r12d, r12d                          ; group G
.ai_group_loop:
    cmp r12d, dword [r15 + ext4_groups_count - kmain]
    jae .ai_err

    ; Check bg_free_inodes_count_lo in group G
    mov ecx, r12d
    shl ecx, 5
    cmp word [r15 + ext4_nbuf - kmain + rcx + 0x0E], 0
    ja .ai_group_has_inodes

    inc r12d
    jmp .ai_group_loop

.ai_group_has_inodes:
    ; Get group G's inode bitmap
    mov eax, dword [r15 + ext4_nbuf - kmain + rcx + 0x04]
    mov dword [r15 + e4w_bmp_blk - kmain], eax

    ; Read inode bitmap into ext4_bbuf
    lea rsi, [r15 + ext4_bbuf - kmain]
    call ext4_blk_read
    jc .ai_err

    ; Scan for first 0-bit
    ; Inodes per group = ext4_ipg, bytes = ext4_ipg / 8
    mov ebx, dword [r15 + ext4_ipg - kmain]
    shr ebx, 3                              ; ebx = bytes to scan
    test ebx, ebx
    jnz @f
    mov ebx, 1
@@:
    xor ecx, ecx                            ; byte index
.ai_scan:
    cmp ecx, ebx
    jae .ai_next_group
    mov al, [r15 + ext4_bbuf - kmain + rcx]
    cmp al, 0xFF
    jne .ai_found_byte
    inc ecx
    jmp .ai_scan

.ai_next_group:
    inc r12d
    jmp .ai_group_loop

.ai_found_byte:
    movzx eax, al
    not eax
    and eax, 0xFF
    bsf edx, eax                            ; bit index 0..7

    ; Set bit in bitmap
    mov al, 1
    push rcx
    mov cl, dl
    shl al, cl
    pop rcx
    or [r15 + ext4_bbuf - kmain + rcx], al

    ; Inode number = r12d * ext4_ipg + (ecx * 8 + edx) + 1
    mov eax, ecx
    shl eax, 3
    add eax, edx
    inc eax                                 ; eax = 1-based inode in group
    push rax
    mov eax, r12d
    mul dword [r15 + ext4_ipg - kmain]
    pop rdx
    add eax, edx                            ; eax = global 1-based inode number
    mov r13d, eax                           ; r13d = allocated inode number

    ; Write inode bitmap back to disk
    mov eax, dword [r15 + e4w_bmp_blk - kmain]
    lea rsi, [r15 + ext4_bbuf - kmain]
    call ext4_blk_write
    jc .ai_err

    ; Re-read GDT into ext4_nbuf
    mov eax, dword [r15 + ext4_gdtb - kmain]
    lea rsi, [r15 + ext4_nbuf - kmain]
    call ext4_blk_read
    jc .ai_err

    ; Decrement free inodes in GDT descriptor for group r12d
    mov ecx, r12d
    shl ecx, 5
    dec word [r15 + ext4_nbuf - kmain + rcx + 0x0E]
    call ext4_gdt_save

    ; Decrement free inodes in Superblock
    dec dword [r15 + ext4_sb - kmain + 0x10]
    call ext4_sb_save

    mov eax, r13d                           ; return allocated inode number
    clc
.ai_pop:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

.ai_err:
    mov rax, -1
    stc
    jmp .ai_pop

; ================= free an inode =================
; EDI = inode number (1-based)
; Out: CF=0 on success, 1 on error.
ext4_free_inode:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r12
    push r13

    cmp edi, 11                             ; inodes 1..10 are reserved
    jb .fi_err
    cmp dword [r15 + ext4_ipg - kmain], 0
    je .fi_err

    dec edi                                 ; 0-based inode index
    mov eax, edi
    xor edx, edx
    div dword [r15 + ext4_ipg - kmain]      ; eax = group G, edx = bit in group
    mov r12d, eax                           ; r12d = group G
    mov r13d, edx                           ; r13d = bit index in group

    ; Read GDT block
    mov eax, dword [r15 + ext4_gdtb - kmain]
    lea rsi, [r15 + ext4_nbuf - kmain]
    call ext4_blk_read
    jc .fi_err

    ; Get group G's inode bitmap
    mov ecx, r12d
    shl ecx, 5
    mov eax, dword [r15 + ext4_nbuf - kmain + rcx + 0x04]
    mov dword [r15 + e4w_bmp_blk - kmain], eax

    ; Read inode bitmap into ext4_bbuf
    lea rsi, [r15 + ext4_bbuf - kmain]
    call ext4_blk_read
    jc .fi_err

    ; Clear bit r13d in bitmap
    mov eax, r13d
    btr dword [r15 + ext4_bbuf - kmain], eax

    ; Write inode bitmap back to disk
    mov eax, dword [r15 + e4w_bmp_blk - kmain]
    lea rsi, [r15 + ext4_bbuf - kmain]
    call ext4_blk_write
    jc .fi_err

    ; Increment free inodes in GDT descriptor for group r12d
    mov ecx, r12d
    shl ecx, 5
    add word [r15 + ext4_nbuf - kmain + rcx + 0x0E], 1
    call ext4_gdt_save

    ; Increment free inodes in Superblock
    inc dword [r15 + ext4_sb - kmain + 0x10]
    call ext4_sb_save

    clc
.fi_pop:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

.fi_err:
    stc
    jmp .fi_pop

; ================= delete file from ext4 filesystem =================
; RSI = filename or path (ASCIIZ).
; Out: RAX = 0 on success, -1 on error. CF=0 on success, 1 on error.
ext4_delete_file:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r12
    push r13
    push r14

    mov r14, rsi

    ; Strip leading slashes to get basename
.df_strip:
    mov al, [rsi]
    inc rsi
    test al, al
    jz .df_strip_done
    cmp al, '/'
    jne .df_strip
    mov r14, rsi
    jmp .df_strip
.df_strip_done:

    cmp byte [r14], 0
    je .df_err

    ; 1. Lookup file to get inode
    mov rsi, r14
    call ext4_lookup
    test eax, eax
    jz .df_err
    cmp eax, 11                             ; protect reserved inodes 1..10
    jb .df_err
    mov r12d, eax                           ; r12d = inode number to delete

    ; 2. Load target inode
    mov eax, r12d
    call ext4_inode_load
    jc .df_err

    ; 3. Free extents/blocks associated with this file
    cmp word [r15 + ext4_inode - kmain + IN_OFF_BLOCK], EH_MAGIC
    jne .df_no_extents
    movzx ecx, word [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 2] ; eh_entries
    test ecx, ecx
    jz .df_no_extents

    xor ebx, ebx                            ; extent index
.df_free_extents:
    cmp ebx, ecx
    jae .df_extents_done
    mov r8d, ebx
    imul r8d, 12
    add r8d, IN_OFF_BLOCK + 12              ; r8d = offset of extent in ext4_inode
    movzx esi, word  [r15 + ext4_inode - kmain + r8 + 4] ; ee_len
    mov   edi, dword [r15 + ext4_inode - kmain + r8 + 8] ; ee_start_lo
    push rcx
    push rbx
    call ext4_free_blocks
    pop rbx
    pop rcx
    inc ebx
    jmp .df_free_extents

.df_extents_done:
.df_no_extents:

    ; 4. Free the inode
    mov edi, r12d
    call ext4_free_inode
    jc .df_err

    ; 5. Find and remove directory entry in parent directory
    ; Load root directory (inode 2)
    mov eax, 2
    call ext4_inode_load
    jc .df_err
    xor eax, eax
    call ext4_map
    cmp rax, -1
    je .df_err
    mov ebx, eax                            ; ebx = dir block

    ; Read directory block into ext4_wbuf
    mov eax, ebx
    lea rsi, [r15 + ext4_wbuf - kmain]
    call ext4_blk_read
    jc .df_err

    ; Scan directory entries in ext4_wbuf
    xor r8d, r8d                            ; byte offset in directory block
.df_scan_dir:
    cmp r8d, 1024
    jae .df_dir_done                        ; not found in root dir block

    mov eax, dword [r15 + ext4_wbuf - kmain + r8] ; entry inode
    cmp eax, r12d
    je .df_found_slot

    movzx eax, word [r15 + ext4_wbuf - kmain + r8 + 4] ; rec_len
    test ax, ax
    jz .df_dir_done
    add r8d, eax
    jmp .df_scan_dir

.df_found_slot:
    ; Set entry inode to 0 to mark as unused / free slot
    mov dword [r15 + ext4_wbuf - kmain + r8], 0

    ; Write modified directory block back to disk
    mov eax, ebx
    lea rsi, [r15 + ext4_wbuf - kmain]
    call ext4_blk_write
    jc .df_err

    call disk_flush_cache

    xor eax, eax                            ; return 0 on success
    clc
.df_pop:
    pop r14
    pop r13
    pop r12
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

.df_dir_done:
    ; Even if dir entry wasn't found in block 0, inode & blocks were freed
    call disk_flush_cache
    xor eax, eax
    clc
    jmp .df_pop

.df_err:
    mov rax, -1
    stc
    jmp .df_pop

; ================= create new empty file =================
; RSI = filename (ASCIIZ). Out: RAX = inode or -1.
ext4_create_file:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r12
    push r13
    push r14

    mov r14, rsi                            ; r14 = filename / path

    ; Strip any leading directory path / slashes to isolate base filename
    mov rsi, r14
.cf_strip:
    mov al, [rsi]
    inc rsi
    test al, al
    jz .cf_strip_done
    cmp al, '/'
    jne .cf_strip
    mov r14, rsi
    jmp .cf_strip
.cf_strip_done:

    cmp byte [r14], 0
    je .cf_fail_A

    ; 1. Check if file already exists
    mov rsi, r14
    call ext4_lookup
    test eax, eax
    jnz .cf_exists

    ; 2. Allocate free inode
    call ext4_alloc_inode
    cmp eax, -1
    je .cf_fail_B
    mov r12d, eax                           ; r12d = new inode

    ; 3. Initialize ext4_inode buffer for new file
    lea rdi, [r15 + ext4_inode - kmain]
    mov ecx, 128 / 8
    xor eax, eax
    cld
    rep stosq

    ; Fill inode fields
    mov word  [r15 + ext4_inode - kmain + IN_OFF_MODE], 0x81A4   ; -rw-r--r--
    mov dword [r15 + ext4_inode - kmain + IN_OFF_SIZE], 0       ; size = 0
    mov word  [r15 + ext4_inode - kmain + 26], 1                 ; i_links_count = 1
    mov dword [r15 + ext4_inode - kmain + IN_OFF_FLAGS], EXT_FLAG ; 0x80000 (extents)

    ; Extent root header in i_block
    lea rdi, [r15 + ext4_inode - kmain + IN_OFF_BLOCK]
    mov word  [rdi + 0], EH_MAGIC           ; 0xF30A
    mov word  [rdi + 2], 0                  ; eh_entries = 0
    mov word  [rdi + 4], 4                  ; eh_max = 4
    mov word  [rdi + 6], 0                  ; eh_depth = 0
    mov dword [rdi + 8], 0                  ; eh_generation = 0

    ; Save new inode to disk!
    mov eax, r12d
    call ext4_inode_save
    jc .cf_fail_C

    ; 4. Add entry to root directory (load inode 2 and map block 0)
    mov eax, 2
    call ext4_inode_load
    jc .cf_fail_D
    xor eax, eax
    call ext4_map
    cmp rax, -1
    je .cf_fail_E
    mov ebx, eax                            ; ebx = root dir physical block

    ; Read root directory block into ext4_wbuf
    mov eax, ebx
    lea rsi, [r15 + ext4_wbuf - kmain]
    call ext4_blk_read
    jc .cf_fail_F

    ; Compute needed size for new entry: (8 + name_len + 3) & ~3
    mov rsi, r14
    xor edx, edx
.cf_nlen:
    cmp byte [rsi + rdx], 0
    je .cf_nlen_done
    inc edx
    cmp edx, 255
    jb .cf_nlen
.cf_nlen_done:
    test edx, edx
    jz .cf_fail_G
    mov r13d, edx                           ; r13d = new name_len

    lea eax, [edx + 8 + 3]
    and eax, not 3
    mov r10d, eax                           ; r10d = needed bytes for new entry

    ; Scan directory block for space
    xor r8d, r8d                            ; r8d = offset in ext4_wbuf
.cf_dir_scan:
    cmp r8d, 1024
    jae .cf_fail_H

    movzx eax, word [r15 + ext4_wbuf - kmain + r8 + 4] ; rec_len
    test ax, ax
    jz .cf_fail_I

    ; Check if unused slot (inode == 0)
    cmp dword [r15 + ext4_wbuf - kmain + r8], 0
    jne .cf_check_split
    cmp eax, r10d
    jae .cf_use_slot

.cf_check_split:
    ; Minimal size of current entry: (8 + entry_name_len + 3) & ~3
    movzx ecx, byte [r15 + ext4_wbuf - kmain + r8 + 6]
    add ecx, 8 + 3
    and ecx, not 3                          ; ecx = min rec_len for current entry

    ; Available spare space in current entry = rec_len - ecx
    mov edx, eax
    sub edx, ecx
    cmp edx, r10d
    jae .cf_split_entry

    add r8d, eax
    jmp .cf_dir_scan

.cf_split_entry:
    ; Shrink current entry to ecx
    mov word [r15 + ext4_wbuf - kmain + r8 + 4], cx
    add r8d, ecx
    ; edx is available size for new entry (at least r10d)
    jmp .cf_fill_new_entry

.cf_use_slot:
    mov edx, eax

.cf_fill_new_entry:
    ; M1 Mitigation: Verify new directory entry fits entirely within 1024-byte wbuf
    lea eax, [r8d + 8]
    add eax, r13d
    cmp eax, 1024
    ja .cf_fail_J

    lea rdi, [r15 + ext4_wbuf - kmain + r8]
    mov dword [rdi + 0], r12d               ; new inode
    mov word  [rdi + 4], dx                 ; rec_len absorbs available space
    mov byte  [rdi + 6], r13b               ; name_len
    mov byte  [rdi + 7], 1                  ; file_type = EXT4_FT_REG_FILE (1)

    ; Copy name characters
    lea rdi, [rdi + 8]
    mov rsi, r14
    mov ecx, r13d
    cld
    rep movsb

    ; Write root dir block back to disk!
    mov eax, ebx
    lea rsi, [r15 + ext4_wbuf - kmain]
    call ext4_blk_write
    jc .cf_fail_J

    call disk_flush_cache

    mov rax, r12                            ; return new inode in RAX
    clc
.cf_pop:
    pop r14
    pop r13
    pop r12
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

.cf_exists:
    clc
    jmp .cf_pop

.cf_fail_A:
    mov al, 'A'
    jmp .cf_fail_print
.cf_fail_B:
    mov al, 'B'
    jmp .cf_fail_print
.cf_fail_C:
    mov al, 'C'
    jmp .cf_fail_print
.cf_fail_D:
    mov al, 'D'
    jmp .cf_fail_print
.cf_fail_E:
    mov al, 'E'
    jmp .cf_fail_print
.cf_fail_F:
    mov al, 'F'
    jmp .cf_fail_print
.cf_fail_G:
    mov al, 'G'
    jmp .cf_fail_print
.cf_fail_H:
    mov al, 'H'
    jmp .cf_fail_print
.cf_fail_I:
    mov al, 'I'
    jmp .cf_fail_print
.cf_fail_J:
    mov al, 'J'
    jmp .cf_fail_print

.cf_fail_print:
    call serial_putc
    mov rax, -1
    stc
    jmp .cf_pop

; ================= write data to file =================
; RAX = inode number, RSI = data_buffer, RDX = count.
; Out: RAX = bytes written (or -1 on error). CF=0 on success, 1 on error.
ext4_write_file:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r12
    push r13
    push r14
    push r15

    mov r12, rax                            ; r12 = inode
    mov r13, rsi                            ; r13 = data buffer
    mov r14, rdx                            ; r14 = count

    ; Load target inode
    mov eax, r12d
    call ext4_inode_load
    jc .wf_fail

    ; Calculate num_blocks needed = (r14 + 1023) / 1024
    mov rax, r14
    add rax, 1023
    shr rax, 10
    mov ebx, eax                            ; ebx = num_blocks needed

    ; Check extent 0 in inode
    movzx edx, word [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 2] ; eh_entries
    test edx, edx
    jz .wf_no_extent

    ; Existing extent 0
    movzx ecx, word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 16] ; ecx = old ee_len
    mov r8d,   dword [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 20] ; r8d = old ee_start_lo

    cmp ebx, ecx
    je .wf_write_blocks                     ; exactly same number of blocks: reuse!

    jb .wf_shrink                           ; ebx < ecx: file shrunk!

    ; ebx > ecx: file grew!
    ; Free old blocks and allocate new chunk
    push rbx
    mov edi, r8d
    mov esi, ecx
    call ext4_free_blocks
    pop rbx

    mov edi, ebx
    call ext4_alloc_blocks
    cmp rax, -1
    je .wf_fail
    mov r8d, eax                            ; r8d = new physical start block

    ; Update extent 0
    mov word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 16], bx
    mov dword [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 20], r8d
    jmp .wf_write_blocks

.wf_shrink:
    ; ebx < ecx
    test ebx, ebx
    jz .wf_shrink_to_zero

    ; Free tail blocks starting at r8d + ebx, count = ecx - ebx
    push rbx
    push r8
    mov edi, r8d
    add edi, ebx
    mov esi, ecx
    sub esi, ebx
    call ext4_free_blocks
    pop r8
    pop rbx

    mov word [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 16], bx
    jmp .wf_write_blocks

.wf_shrink_to_zero:
    ; Free all old blocks
    mov edi, r8d
    mov esi, ecx
    call ext4_free_blocks
    mov word [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 2], 0 ; eh_entries = 0
    jmp .wf_update_meta

.wf_no_extent:
    test ebx, ebx
    jz .wf_update_meta                      ; 0 bytes written to empty file

    mov edi, ebx
    call ext4_alloc_blocks
    cmp rax, -1
    je .wf_fail
    mov r8d, eax                            ; r8d = physical start block

    ; Setup extent root header in i_block
    mov word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 0], EH_MAGIC ; 0xF30A
    mov word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 2], 1        ; eh_entries = 1
    mov word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 4], 4        ; eh_max = 4
    mov word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 6], 0        ; eh_depth = 0
    mov dword [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 8], 0        ; eh_generation = 0

    ; Setup extent 0
    mov dword [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 12], 0       ; ee_block = 0
    mov word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 16], bx      ; ee_len = ebx
    mov word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 18], 0       ; ee_start_hi = 0
    mov dword [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 20], r8d     ; ee_start_lo = r8d

.wf_write_blocks:
    ; r8d = starting physical block
    ; ebx = total blocks to write
    ; r13 = user data pointer
    ; r14 = total byte count
    xor r9d, r9d                            ; r9d = current block index (0 .. ebx - 1)

.wf_blk_loop:
    cmp r9d, ebx
    jae .wf_update_meta

    ; Calculate remaining bytes: rem = r14 - r9 * 1024
    mov rax, r9
    shl rax, 10                             ; rax = r9 * 1024
    mov rcx, r14
    sub rcx, rax                            ; rcx = remaining bytes

    mov eax, r8d
    add eax, r9d                            ; eax = physical block to write

    cmp rcx, 1024
    jb .wf_write_tail

    ; Full 1024-byte block: write directly from user buffer
    mov rsi, r9
    shl rsi, 10
    add rsi, r13
    mov eax, r8d
    add eax, r9d
    call ext4_blk_write
    jc .wf_fail
    inc r9d
    jmp .wf_blk_loop

.wf_write_tail:
    ; Partial block (< 1024 bytes) in ext4_wbuf
    push rax
    lea rdi, [r15 + ext4_wbuf - kmain]
    mov ecx, 1024 / 8
    xor eax, eax
    cld
    rep stosq
    pop rax

    ; Copy remaining bytes into ext4_wbuf
    push rax
    mov rax, r9
    shl rax, 10
    lea rsi, [r13 + rax]
    mov rcx, r14
    sub rcx, rax
    lea rdi, [r15 + ext4_wbuf - kmain]
    cld
    rep movsb
    pop rax

    lea rsi, [r15 + ext4_wbuf - kmain]
    call ext4_blk_write
    jc .wf_fail
    inc r9d
    jmp .wf_blk_loop

.wf_update_meta:
    ; Update inode metadata
    mov qword [r15 + ext4_inode - kmain + IN_OFF_SIZE], r14
    mov eax, r14d
    add eax, 511
    shr eax, 9                              ; sectors = (size + 511) / 512
    mov dword [r15 + ext4_inode - kmain + IN_OFF_BLOCKS], eax

    ; Save updated inode
    mov eax, r12d
    call ext4_inode_save
    jc .wf_fail

    call disk_flush_cache

    mov rax, r14                            ; return bytes written
    clc
.wf_pop:
    pop r15
    pop r14
    pop r13
    pop r12
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

.wf_fail:
    mov rax, -1
    stc
    jmp .wf_pop

; ================= load inode =================
; RAX = inode number -> ext4_inode filled. CF=1 on error.
ext4_inode_load:
    push rcx
    push rdx
    push rsi
    push rdi
    cmp dword [r15 + ext4_ipg - kmain], 0
    je .err
    test eax, eax
    jle .err
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
    sub ecx, eax                            ; ecx = remaining contiguous blocks in extent
    add rax, rdx                            ; phys = start_lo + off
    jmp .done
.leaf_next:
    add rdi, 12
    dec r8d
    jnz .map_scan_leaf
.none:
    mov rax, -1
    xor ecx, ecx
.done:
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    add rsp, 8                              ; preserve rcx return value
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

    ; If bytes left < block size, read tail block
    cmp rbx, r14
    jb .rf_tail

    ; Bulk read contiguous blocks from extent
    test rcx, rcx
    jnz @f
    mov rcx, 1
@@:
    push rax                        ; save phys block
    mov rax, rbx
    xor edx, edx
    div r14                         ; rax = full blocks needed
    cmp rax, rcx
    jbe @f
    mov rax, rcx                    ; clamp to extent contiguous blocks
@@:
    mov r8, rax                     ; r8 = blocks to read (>= 1)
    pop rax                         ; restore phys block

    ; Direct bulk read into user buffer!
    lea rsi, [rdi + r13]
    mov rcx, r8
    call ext4_blks_read
    jc .rf_err_pop

    ; Advance bytes read: r13 += r8 * r14
    mov rax, r8
    mul r14
    add r13, rax

    ; Advance logical block: e4t_lb += r8
    add dword [r15 + e4t_lb - kmain], r8d
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
ext4_wbuf   rb 1024
e4t_tblk    dd 0
e4w_bmp_blk dd 0
ext4_ok     db 0
ext4_lbs    dd 0
ext4_bpg    dd 0
ext4_ipg    dd 0
ext4_isize  dd 0
ext4_fdb    dd 0
ext4_gdtb   dd 0
ext4_groups_count dd 1
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