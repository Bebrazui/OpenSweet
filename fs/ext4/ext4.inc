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
    cmp eax, 8192                       ; Maximum 8192 blocks per group supported by 1024-byte ext4_bbuf
    ja .fail
    mov dword [r15 + ext4_bpg - kmain], eax
    mov eax, dword [r15 + ext4_sb - kmain + SB_IPG]
    test eax, eax
    jz .fail
    cmp eax, 8192                   ; inode bitmap lives in 1KB ext4_bbuf (8192 bits)
    ja .fail
    mov dword [r15 + ext4_ipg - kmain], eax
    movzx eax, word [r15 + ext4_sb - kmain + SB_ISIZE]
    test eax, eax
    jz .fail
    cmp eax, 128                    ; ext4_inode buffer is exactly 128 bytes;
    jne .fail                       ; larger inode sizes would overflow .bss
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
    cmp eax, 32                         ; Maximum 32 groups fit in 1024-byte ext4_nbuf (32B per desc)
    ja .fail
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
    cmp eax, dword [r15 + ext4_groups_count - kmain]
    jae .eis_err                            ; group out of range: never index GDT
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

    ; Compute physical block number: RAX = r12d * ext4_bpg + r11d + s_first_data_block
    mov eax, r12d
    mul dword [r15 + ext4_bpg - kmain]
    add eax, r11d
    add eax, dword [r15 + ext4_sb - kmain + SB_FDB]
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
    sub eax, dword [r15 + ext4_sb - kmain + SB_FDB] ; make relative to first data block
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
    cmp r12d, dword [r15 + ext4_groups_count - kmain]
    jae .fi_err                             ; group out of range: never index GDT/bitmap

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

    ; Clear bit r13d in bitmap (r13d: 0..ipg-1)
    mov ecx, r13d
    shr ecx, 3                              ; ecx = byte index
    mov edx, r13d
    and edx, 7                              ; edx = bit index 0..7
    mov al, 1
    push rcx
    mov cl, dl
    shl al, cl
    pop rcx
    not al
    and [r15 + ext4_bbuf - kmain + rcx], al

    ; Write inode bitmap back to disk
    mov eax, dword [r15 + e4w_bmp_blk - kmain]
    lea rsi, [r15 + ext4_bbuf - kmain]
    call ext4_blk_write
    jc .fi_err

    ; 1. Zero out the inode table entry on disk first
    push r12
    push r13
    lea rdi, [r15 + ext4_inode - kmain]
    mov ecx, 128 / 8
    xor eax, eax
    rep stosq
    mov eax, [rsp + 8 + 8 + 16]             ; original 1-based inode from saved RDI
    call ext4_inode_save
    pop r13
    pop r12

    ; 2. Re-read GDT into ext4_nbuf since ext4_inode_save overwrote it with its own block
    mov eax, dword [r15 + ext4_gdtb - kmain]
    lea rsi, [r15 + ext4_nbuf - kmain]
    call ext4_blk_read
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

; ================= resolve parent directory inode and basename =================
; Inputs:
;   RSI = path string (ASCIIZ)
;   RDI = destination buffer for basename (256 bytes)
; Outputs:
;   EAX = parent directory inode number
;   RDI = filled with null-terminated basename
;   CF  = 0 on success, 1 on error
ext4_resolve_parent:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    sub rsp, 288                        ; [rsp]: temp path copy (256 bytes), [rsp+256]: last_slash_offset

    mov r12, rsi                        ; r12 = input path
    mov r13, rdi                        ; r13 = basename dest

    ; 1. Copy input path to local stack buffer and find last '/'
    lea rdi, [rsp]
    mov rsi, r12
    mov qword [rsp + 256], -1           ; default last_slash = -1
    xor ecx, ecx                        ; ecx = char index
.rp_copy:
    lodsb
    stosb
    cmp al, '/'
    jne @f
    mov [rsp + 256], rcx                ; record last slash offset
@@:
    test al, al
    jz .rp_copied
    inc ecx
    cmp ecx, 250
    jb .rp_copy
    mov byte [rdi], 0                   ; ensure null termination
.rp_copied:

    mov rax, [rsp + 256]                ; rax = last_slash offset
    cmp rax, -1
    jne .rp_have_slash

    ; Case A: No slash found (e.g. "myfile.txt")
    ; Parent is CWD
        ; Copy whole string to basename dest
    lea rsi, [rsp]
    mov rdi, r13
    call .rp_strcpy
    mov eax, [r15 + ext4_cwd_inode - kmain]
jmp .rp_success

.rp_have_slash:
    test rax, rax
    jnz .rp_subpath

    ; Case B: Last slash is at index 0 (e.g. "/myfile.txt")
    ; Parent is root directory (inode 2)
        ; Copy string starting after slash to basename dest
    lea rsi, [rsp + 1]
    mov rdi, r13
    call .rp_strcpy
    mov eax, 2
jmp .rp_success

.rp_subpath:
    ; Case C: Slash is at index > 0 (e.g. "a/b/file" or "/a/b/file")
    ; Split path at last slash: put null terminator at last slash
    mov byte [rsp + rax], 0
    ; Copy filename component (after last slash) to basename dest
    lea rsi, [rsp + rax + 1]
    mov rdi, r13
    call .rp_strcpy

    ; Lookup directory path [rsp] to find its inode
    lea rsi, [rsp]
    call ext4_lookup
    test eax, eax
    jz .rp_fail                         ; parent directory doesn't exist!

.rp_success:
    ; EAX = parent inode
    ; Verify basename is not empty
    cmp byte [r13], 0
    je .rp_fail
    add rsp, 288
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    clc
    ret

.rp_fail:
    add rsp, 288
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    mov rax, -1
    stc
    ret

.rp_strcpy:
    lodsb
    stosb
    test al, al
    jnz .rp_strcpy
    ret

; ================= add entry to directory =================
; Inputs:
;   EAX = parent directory inode
;   EDX = child inode number
;   RSI = child name string (ASCIIZ)
;   CL  = child file_type (1 = file, 2 = dir)
; Outputs:
;   RAX = 0 on success, -1 on error
;   CF  = 0 on success, 1 on error
ext4_add_dir_entry:
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
    push r15
    sub rsp, 32

    mov r12d, eax                       ; r12d = parent inode
    mov r13d, edx                       ; r13d = child inode
    mov r14, rsi                        ; r14 = name pointer
    mov [rsp + 24], cl                  ; [rsp+24] = file_type (preserve r15 = kmain!)


    ; Calculate name length
    mov rsi, r14
    xor ecx, ecx
.ade_nlen:
    cmp byte [rsi + rcx], 0
    je .ade_nlen_done
    inc ecx
    cmp ecx, 255
    jb .ade_nlen
.ade_nlen_done:
    test ecx, ecx
    jz .ade_err
    mov [rsp + 0], ecx                  ; [rsp+0] = name_len

    ; Calculate needed record length: (8 + name_len + 3) & ~3
    lea eax, [ecx + 8 + 3]
    and eax, not 3
    mov [rsp + 8], eax                  ; [rsp+8] = needed_rec_len

    ; Load parent directory inode
    mov eax, r12d
    call ext4_inode_load
    jc .ade_err

    ; Verify directory (i_mode & 0xF000 == 0x4000)
    movzx edx, word [r15 + ext4_inode - kmain + IN_OFF_MODE]
    and edx, 0xF000
    cmp edx, 0x4000
    jne .ade_err

    ; Map directory logical block 0
    xor eax, eax
    call ext4_map
    cmp rax, -1
    je .ade_err
    mov ebx, eax                        ; ebx = physical block of directory
    mov [rsp + 16], rbx                 ; save dir phys block

    ; Read directory block into ext4_wbuf
    mov eax, ebx
    lea rsi, [r15 + ext4_wbuf - kmain]
    call ext4_blk_read
    jc .ade_err

    ; Scan directory entries in ext4_wbuf
    xor r8d, r8d                        ; r8d = byte offset in block
    mov r10d, [rsp + 8]                 ; r10d = needed_rec_len

.ade_scan:
    cmp r8d, 1024
    jae .ade_err

    movzx eax, word [r15 + ext4_wbuf - kmain + r8 + 4] ; rec_len
    test ax, ax
    jz .ade_err

    ; Check if unused slot (inode == 0)
    cmp dword [r15 + ext4_wbuf - kmain + r8], 0
    jne .ade_chk_split
    cmp eax, r10d
    jae .ade_use_slot

.ade_chk_split:
    ; Minimal rec_len for current entry = (8 + entry_name_len + 3) & ~3
    movzx ecx, byte [r15 + ext4_wbuf - kmain + r8 + 6] ; entry name_len
    add ecx, 8 + 3
    and ecx, not 3                      ; min rec_len for current
    mov edx, eax
    sub edx, ecx                        ; spare space in current entry
    cmp edx, r10d
    jae .ade_split

    add r8d, eax
    jmp .ade_scan

.ade_split:
    ; Shrink current entry to ecx
    mov word [r15 + ext4_wbuf - kmain + r8 + 4], cx
    add r8d, ecx
    ; edx is available size for new entry
    jmp .ade_fill

.ade_use_slot:
    mov edx, eax

.ade_fill:
    ; Check within bounds
    mov ecx, [rsp + 0]                  ; name_len
    lea eax, [r8 + rcx + 8]
    cmp eax, 1024
    ja .ade_err

    lea rdi, [r15 + ext4_wbuf - kmain + r8]
    mov dword [rdi + 0], r13d           ; child inode
    mov word  [rdi + 4], dx             ; rec_len
    mov byte  [rdi + 6], cl             ; name_len
    mov al, [rsp + 24]
    mov byte  [rdi + 7], al             ; file_type

    ; Copy name characters
    lea rdi, [rdi + 8]
    mov rsi, r14
    rep movsb

    ; Write directory block back to disk
    mov rax, [rsp + 16]
    lea rsi, [r15 + ext4_wbuf - kmain]
    call ext4_blk_write
    jc .ade_err

    call disk_flush_cache

    xor eax, eax
    clc

.ade_pop:
    add rsp, 32
    pop r15
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


.ade_err:
    mov rax, -1
    stc
    jmp .ade_pop

; ================= remove entry from directory =================
; Inputs:
;   EAX = parent directory inode
;   EDX = child inode number to remove
; Outputs:
;   RAX = 0 on success, -1 on error
;   CF  = 0 on success, 1 on error
ext4_remove_dir_entry:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r12
    push r13

    mov r12d, eax                       ; r12d = parent inode
    mov r13d, edx                       ; r13d = target child inode

    ; Load parent directory inode
    mov eax, r12d
    call ext4_inode_load
    jc .rde_err

    xor eax, eax
    call ext4_map
    cmp rax, -1
    je .rde_err
    mov ebx, eax                        ; ebx = dir phys block

    ; Read directory block into ext4_wbuf
    mov eax, ebx
    lea rsi, [r15 + ext4_wbuf - kmain]
    call ext4_blk_read
    jc .rde_err

    ; Scan directory entries in ext4_wbuf
    xor r8d, r8d                        ; r8d = offset in block
.rde_scan:
    cmp r8d, 1024
    jae .rde_err

    mov eax, dword [r15 + ext4_wbuf - kmain + r8] ; entry inode
    cmp eax, r13d
    je .rde_found

    movzx eax, word [r15 + ext4_wbuf - kmain + r8 + 4] ; rec_len
    test ax, ax
    jz .rde_err
    add r8d, eax
    jmp .rde_scan

.rde_found:
    ; Mark slot as unused by clearing inode
    mov dword [r15 + ext4_wbuf - kmain + r8], 0

    ; Write block back to disk
    mov eax, ebx
    lea rsi, [r15 + ext4_wbuf - kmain]
    call ext4_blk_write
    jc .rde_err

    call disk_flush_cache
    xor eax, eax
    clc
.rde_pop:
    pop r13
    pop r12
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

.rde_err:
    mov rax, -1
    stc
    jmp .rde_pop

; ================= create new empty file =================
; RSI = filename or path (ASCIIZ). Out: RAX = inode or -1.
ext4_create_file:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r12
    push r13
    push r14
    push r15
    sub rsp, 288                        ; [rsp]: basename buffer

    mov r14, rsi                        ; r14 = input path

    ; 1. Check if file already exists
    mov rsi, r14
    call ext4_lookup
    test eax, eax
    jnz .cf_exists

    ; 2. Resolve parent directory inode and basename
    mov rsi, r14
    lea rdi, [rsp]
    call ext4_resolve_parent
    jc .cf_fail
    mov r12d, eax                       ; r12d = parent inode

    ; 3. Allocate free inode
    call ext4_alloc_inode
    cmp eax, -1
    je .cf_fail
    mov r13d, eax                       ; r13d = new inode

    ; 4. Initialize ext4_inode for new regular file
    lea rdi, [r15 + ext4_inode - kmain]
    mov ecx, 128 / 8
    xor eax, eax
    rep stosq

    mov word  [r15 + ext4_inode - kmain + IN_OFF_MODE], 0x81A4   ; -rw-r--r--
    mov dword [r15 + ext4_inode - kmain + IN_OFF_SIZE], 0
    mov word  [r15 + ext4_inode - kmain + 26], 1                 ; i_links_count = 1
    mov dword [r15 + ext4_inode - kmain + IN_OFF_BLOCKS], 0
    mov dword [r15 + ext4_inode - kmain + IN_OFF_FLAGS], EXT_FLAG ; 0x80000 (extents)

    ; Extents header
    mov word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 0], EH_MAGIC ; 0xF30A
    mov word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 2], 0        ; entries = 0
    mov word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 4], 4        ; max = 4
    mov word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 6], 0        ; depth = 0
    mov dword [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 8], 0        ; gen = 0

    ; Save new inode
    mov eax, r13d
    call ext4_inode_save
    jc .cf_fail_free_ino

    ; 5. Add entry to parent directory
    mov eax, r12d                       ; parent inode
    mov edx, r13d                       ; child inode
    lea rsi, [rsp]                      ; basename
    mov cl, 1                           ; EXT4_FT_REG_FILE (1)
    call ext4_add_dir_entry
    jc .cf_fail_free_ino

    call disk_flush_cache
    mov rax, r13                        ; return new inode in RAX
    clc
    jmp .cf_pop

.cf_exists:
    ; RAX already has existing inode
    clc
    jmp .cf_pop

.cf_fail_free_ino:
    mov edi, r13d
    call ext4_free_inode

.cf_fail:
    mov rax, -1
    stc

.cf_pop:
    add rsp, 288
    pop r15
    pop r14
    pop r13
    pop r12
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

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
    push r12
    push r13
    push r14
    push r15
    sub rsp, 288                        ; [rsp]: basename buffer

    mov r14, rsi

    ; 1. Resolve parent directory inode and basename
    mov rsi, r14
    lea rdi, [rsp]
    call ext4_resolve_parent
    jc .df_err
    mov r12d, eax                       ; r12d = parent inode

    ; 2. Lookup file to get its inode
    mov rsi, r14
    call ext4_lookup
    test eax, eax
    jz .df_err
    cmp eax, 11                         ; protect system inodes
    jb .df_err
    mov r13d, eax                       ; r13d = target inode to delete

    ; 3. Truncate file (frees all allocated data blocks)
    mov eax, r13d
    xor edx, edx
    call ext4_truncate_file
    jc .df_err

    ; 4. Free the inode
    mov edi, r13d
    call ext4_free_inode
    jc .df_err

    ; 5. Remove directory entry from parent directory
    mov eax, r12d                       ; parent inode
    mov edx, r13d                       ; target inode
    call ext4_remove_dir_entry
    jc .df_err

    call disk_flush_cache
    xor eax, eax
    clc

.df_pop:
    add rsp, 288
    pop r15
    pop r14
    pop r13
    pop r12
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

.df_err:
    mov rax, -1
    stc
    jmp .df_pop

; ================= create directory (mkdir) =================
; RSI = directory path (ASCIIZ)
; Out: RAX = 0 on success, -1 on error. CF=0 on success, 1 on error.
ext4_mkdir:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r12
    push r13
    push r14
    push r15
    sub rsp, 288                        ; [rsp]: basename buffer (256 bytes)

    mov r14, rsi                        ; r14 = full path

    ; 1. Check if target path already exists
    mov rsi, r14
    call ext4_lookup
    test eax, eax
    jnz .mkdir_err                      ; already exists!

    ; 2. Resolve parent directory inode and basename
    mov rsi, r14
    lea rdi, [rsp]
    call ext4_resolve_parent
    jc .mkdir_err
    mov r12d, eax                       ; r12d = parent inode

    ; 3. Allocate free inode for new directory
    call ext4_alloc_inode
    cmp eax, -1
    je .mkdir_err
    mov r13d, eax                       ; r13d = new dir inode

    ; 4. Allocate 1 physical block for directory entries ('.' and '..')
    call ext4_alloc_block
    cmp rax, -1
    je .mkdir_err_free_ino
    mov ebx, eax                        ; ebx = dir phys block

    ; 5. Format directory block in ext4_wbuf:
    ; Zero out entire 1024-byte buffer
    lea rdi, [r15 + ext4_wbuf - kmain]
    mov ecx, 1024 / 8
    xor eax, eax
    rep stosq

    ; Entry 1: "." (points to r13d)
    lea rdi, [r15 + ext4_wbuf - kmain]
    mov dword [rdi + 0], r13d           ; inode = new_inode
    mov word  [rdi + 4], 12             ; rec_len = 12
    mov byte  [rdi + 6], 1              ; name_len = 1
    mov byte  [rdi + 7], 2              ; file_type = EXT4_FT_DIR (2)
    mov byte  [rdi + 8], '.'

    ; Entry 2: ".." (points to r12d)
    lea rdi, [r15 + ext4_wbuf - kmain + 12]
    mov dword [rdi + 0], r12d           ; inode = parent_inode
    mov word  [rdi + 4], 1024 - 12      ; rec_len = 1012 (all remaining space)
    mov byte  [rdi + 6], 2              ; name_len = 2
    mov byte  [rdi + 7], 2              ; file_type = EXT4_FT_DIR (2)
    mov word  [rdi + 8], 0x2E2E         ; ".."

    ; Write directory block to disk
    mov eax, ebx
    lea rsi, [r15 + ext4_wbuf - kmain]
    call ext4_blk_write
    jc .mkdir_err_free_blk

    ; 6. Initialize inode for new directory
    lea rdi, [r15 + ext4_inode - kmain]
    mov ecx, 128 / 8
    xor eax, eax
    rep stosq

    mov word  [r15 + ext4_inode - kmain + IN_OFF_MODE], 0x41ED  ; drwxr-xr-x
    mov dword [r15 + ext4_inode - kmain + IN_OFF_SIZE], 1024    ; size = 1 block
    mov word  [r15 + ext4_inode - kmain + 26], 2                ; i_links_count = 2 (self + '.')
    mov dword [r15 + ext4_inode - kmain + IN_OFF_BLOCKS], 2     ; 2 sectors (1024 bytes)
    mov dword [r15 + ext4_inode - kmain + IN_OFF_FLAGS], EXT_FLAG ; extents

    ; Extents header in i_block
    mov word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 0], EH_MAGIC
    mov word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 2], 1 ; eh_entries = 1
    mov word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 4], 4 ; eh_max = 4
    mov word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 6], 0 ; eh_depth = 0
    mov dword [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 8], 0 ; eh_gen = 0
    mov dword [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 12], 0 ; ee_block = 0
    mov word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 16], 1 ; ee_len = 1
    mov word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 18], 0 ; ee_start_hi = 0
    mov dword [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 20], ebx ; ee_start_lo = dir_phys_block

    ; Save new directory inode
    mov eax, r13d
    call ext4_inode_save
    jc .mkdir_err_free_blk

    ; 7. Add entry to parent directory
    mov eax, r12d                       ; parent inode
    mov edx, r13d                       ; new dir inode
    lea rsi, [rsp]                      ; basename
    mov cl, 2                           ; EXT4_FT_DIR
    call ext4_add_dir_entry
    jc .mkdir_err_free_blk

    ; 8. Increment parent directory i_links_count (because child has '..')
    mov eax, r12d
    call ext4_inode_load
    jc .mkdir_done                      ; non-fatal if links count update fails
    inc word [r15 + ext4_inode - kmain + 26] ; i_links_count++
    mov eax, r12d
    call ext4_inode_save

.mkdir_done:
    call disk_flush_cache
    xor eax, eax
    clc

.mkdir_pop:
    add rsp, 288
    pop r15
    pop r14
    pop r13
    pop r12
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

.mkdir_err_free_blk:
    mov edi, ebx
    mov esi, 1
    call ext4_free_blocks

.mkdir_err_free_ino:
    mov edi, r13d
    call ext4_free_inode

.mkdir_err:
    mov rax, -1
    stc
    jmp .mkdir_pop

; ================= create fast symlink =================
; RSI = symlink path (ASCIIZ), RDX = target path string (ASCIIZ)
; Out: RAX = new inode or -1 on error. CF=0 on success, 1 on error.
ext4_symlink:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r12
    push r13
    push r14
    push r15
    sub rsp, 288                        ; [rsp]: basename buffer

    mov r14, rsi                        ; r14 = symlink path
    mov r12, rdx                        ; r12 = target string

    ; 1. Check if symlink path already exists
    mov rsi, r14
    call ext4_lookup
    test eax, eax
    jnz .sym_err                        ; already exists!

    ; 2. Calculate length of target string (must be <= 59 for fast symlink)
    mov rsi, r12
    xor ecx, ecx
.sym_len_loop:
    cmp byte [rsi + rcx], 0
    je .sym_len_done
    inc ecx
    cmp ecx, 59
    ja .sym_err                         ; target too long for fast symlink (> 59 bytes)
    jmp .sym_len_loop
.sym_len_done:
    mov ebx, ecx                        ; ebx = target len

    ; 3. Resolve parent directory inode and basename
    mov rsi, r14
    lea rdi, [rsp]
    call ext4_resolve_parent
    jc .sym_err
    mov r8d, eax                        ; r8d = parent inode

    ; 4. Allocate free inode
    call ext4_alloc_inode
    cmp eax, -1
    je .sym_err
    mov r13d, eax                       ; r13d = new symlink inode

    ; 5. Initialize inode structure
    lea rdi, [r15 + ext4_inode - kmain]
    mov ecx, 128 / 8
    xor eax, eax
    rep stosq

    ; S_IFLNK (0xA000) | 0777 = 0xA1FF
    mov word  [r15 + ext4_inode - kmain + IN_OFF_MODE], 0xA1FF
    mov dword [r15 + ext4_inode - kmain + IN_OFF_SIZE], ebx
    mov word  [r15 + ext4_inode - kmain + 26], 1                  ; i_links_count = 1
    mov dword [r15 + ext4_inode - kmain + IN_OFF_BLOCKS], 0       ; 0 blocks used
    mov dword [r15 + ext4_inode - kmain + IN_OFF_FLAGS], 0        ; not extents!

    ; Copy target string directly into i_block (starts at offset 40)
    lea rdi, [r15 + ext4_inode - kmain + IN_OFF_BLOCK]
    mov rsi, r12
    mov ecx, ebx
    rep movsb
    mov byte [rdi], 0

    ; 6. Save inode to disk
    mov eax, r13d
    call ext4_inode_save
    jc .sym_err_free_ino

    ; 7. Add directory entry to parent directory (type 7 = DT_LNK)
    mov eax, r8d                        ; parent inode
    mov edx, r13d                       ; child inode
    lea rsi, [rsp]                      ; basename
    mov cl, 7                           ; type 7 = symlink
    call ext4_add_dir_entry
    jc .sym_err_free_ino

    call disk_flush_cache
    mov rax, r13                        ; return inode
    clc
.sym_pop:
    add rsp, 288
    pop r15
    pop r14
    pop r13
    pop r12
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

.sym_err_free_ino:
    mov edi, r13d
    call ext4_free_inode
.sym_err:
    mov rax, -1
    stc
    jmp .sym_pop

; ================= read symlink target from inode =================
; EAX = inode, RDI = user buffer, RSI = buffer capacity
; Out: RAX = bytes read or -1 on error.
ext4_readlink_inode:
    push rbx
    push rdi
    push rsi
    call ext4_inode_load
    jc .rli_err

    ; Check if symlink
    movzx edx, word [r15 + ext4_inode - kmain + IN_OFF_MODE]
    and edx, 0xF000
    cmp edx, 0xA000
    jne .rli_err

    mov eax, dword [r15 + ext4_inode - kmain + IN_OFF_SIZE]
    cmp rax, rsi                        ; capacity check
    jbe @f
    mov rax, rsi                        ; truncate to capacity
@@:
    mov ecx, eax                        ; count

    ; If i_size <= 59, read from inline i_block (fast symlink)
    cmp dword [r15 + ext4_inode - kmain + IN_OFF_SIZE], 59
    ja .rli_long

    lea rsi, [r15 + ext4_inode - kmain + IN_OFF_BLOCK]
    rep movsb
    jmp .rli_done

.rli_long:
    ; Long symlink: target path is stored in data block(s)
    push rax                            ; save return byte count
    push rcx                            ; save count
    push rdi                            ; save dest buf
    xor eax, eax                        ; logical block 0
    call ext4_map
    pop rdi
    pop rcx
    cmp rax, -1
    je .rli_long_fail
    ; Read block into ext4_wbuf
    lea rsi, [r15 + ext4_wbuf - kmain]
    call ext4_blk_read
    jc .rli_long_fail
    ; Copy ecx bytes from ext4_wbuf to dest rdi
    lea rsi, [r15 + ext4_wbuf - kmain]
    rep movsb
    pop rax                             ; restore return byte count
    jmp .rli_done

.rli_long_fail:
    pop rax
    jmp .rli_err

.rli_err:
    mov rax, -1
.rli_done:
    pop rsi
    pop rdi
    pop rbx
    ret

; ================= remove directory (rmdir) =================
; RSI = directory path (ASCIIZ)
; Out: RAX = 0 on success, -1 on error. CF=0 on success, 1 on error.
ext4_rmdir:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r12
    push r13
    push r14
    push r15
    sub rsp, 288                        ; [rsp]: basename buffer

    mov r14, rsi

    ; 1. Resolve parent directory inode and basename
    mov rsi, r14
    lea rdi, [rsp]
    call ext4_resolve_parent
    jc .rmdir_err
    mov r12d, eax                       ; r12d = parent inode

    ; 2. Lookup target directory to get its inode
    mov rsi, r14
    call ext4_lookup
    test eax, eax
    jz .rmdir_err
    cmp eax, 11
    jb .rmdir_err                       ; protect root and system inodes
    mov r13d, eax                       ; r13d = target dir inode

    ; 3. Load target directory inode
    mov eax, r13d
    call ext4_inode_load
    jc .rmdir_err

    ; Verify it is a directory
    movzx edx, word [r15 + ext4_inode - kmain + IN_OFF_MODE]
    and edx, 0xF000
    cmp edx, 0x4000
    jne .rmdir_err

    ; Map block 0 to verify directory is empty
    xor eax, eax
    call ext4_map
    cmp rax, -1
    je .rmdir_err
    mov ebx, eax                        ; ebx = dir phys block

    ; Read directory block into ext4_wbuf
    mov eax, ebx
    lea rsi, [r15 + ext4_wbuf - kmain]
    call ext4_blk_read
    jc .rmdir_err

    ; 4. Check if directory is empty (only '.' and '..' allowed)
    xor r8d, r8d
.rmdir_scan:
    cmp r8d, 1024
    jae .rmdir_is_empty

    mov eax, dword [r15 + ext4_wbuf - kmain + r8] ; entry inode
    test eax, eax
    jz .rmdir_next                      ; unused slot

    movzx ecx, byte [r15 + ext4_wbuf - kmain + r8 + 6] ; name_len
    cmp ecx, 1
    jne .rmdir_not_dot
    cmp byte [r15 + ext4_wbuf - kmain + r8 + 8], '.'
    je .rmdir_next
.rmdir_not_dot:
    cmp ecx, 2
    jne .rmdir_not_empty
    cmp word [r15 + ext4_wbuf - kmain + r8 + 8], 0x2E2E ; ".."
    je .rmdir_next

.rmdir_not_empty:
    ; Found entry other than '.' or '..' -> directory is NOT empty!
    jmp .rmdir_err

.rmdir_next:
    movzx eax, word [r15 + ext4_wbuf - kmain + r8 + 4] ; rec_len
    test ax, ax
    jz .rmdir_is_empty
    add r8d, eax
    jmp .rmdir_scan

.rmdir_is_empty:
    ; 5. Free directory physical block
    mov edi, ebx
    mov esi, 1
    call ext4_free_blocks

    ; 6. Free directory inode
    mov edi, r13d
    call ext4_free_inode

    ; 7. Remove entry from parent directory
    mov eax, r12d                       ; parent inode
    mov edx, r13d                       ; target inode
    call ext4_remove_dir_entry

    ; 8. Decrement parent directory link count
    mov eax, r12d
    call ext4_inode_load
    jc .rmdir_done
    dec word [r15 + ext4_inode - kmain + 26] ; i_links_count--
    mov eax, r12d
    call ext4_inode_save

.rmdir_done:
    call disk_flush_cache
    xor eax, eax
    clc

.rmdir_pop:
    add rsp, 288
    pop r15
    pop r14
    pop r13
    pop r12
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

.rmdir_err:
    mov rax, -1
    stc
    jmp .rmdir_pop

; ================= truncate file to 0 size =================
; EAX = inode number (1-based), RDX = new size (0 supported)
; Out: RAX = 0 on success, -1 on error. CF=0 on success, 1 on error.
ext4_truncate_file:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r12
    push r13

    mov r12d, eax                       ; inode number

    ; Load target inode
    call ext4_inode_load
    jc .trunc_err

    ; Check if extents are enabled
    cmp word [r15 + ext4_inode - kmain + IN_OFF_BLOCK], EH_MAGIC
    jne .trunc_no_ext

    movzx ecx, word [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 2] ; eh_entries
    test ecx, ecx
    jz .trunc_no_ext

    xor ebx, ebx                        ; extent index 0 .. ecx - 1
.trunc_free_loop:
    cmp ebx, ecx
    jae .trunc_ext_freed
    mov r8d, ebx
    imul r8d, 12
    add r8d, IN_OFF_BLOCK + 12          ; r8d = offset of extent entry
    movzx esi, word  [r15 + ext4_inode - kmain + r8 + 4] ; ee_len
    mov   edi, dword [r15 + ext4_inode - kmain + r8 + 8] ; ee_start_lo
    push rcx
    push rbx
    call ext4_free_blocks
    pop rbx
    pop rcx
    inc ebx
    jmp .trunc_free_loop

.trunc_ext_freed:
    ; Reset extent header
    mov word [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 2], 0 ; eh_entries = 0

.trunc_no_ext:
    ; Reset file size and blocks count
    mov qword [r15 + ext4_inode - kmain + IN_OFF_SIZE], 0
    mov dword [r15 + ext4_inode - kmain + IN_OFF_BLOCKS], 0

    ; Save updated inode
    mov eax, r12d
    call ext4_inode_save
    jc .trunc_err

    call disk_flush_cache
    xor eax, eax
    clc
.trunc_pop:
    pop r13
    pop r12
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

.trunc_err:
    mov rax, -1
    stc
    jmp .trunc_pop

; ================= write data chunk to file by inode =================
; Inputs:
;   EAX = inode number
;   RDX = file offset (byte offset)
;   RSI = source data buffer
;   RCX = count of bytes to write
; Outputs:
;   RAX = bytes written (-1 on error)
;   CF  = 0 on success, 1 on error
; =====================================================================
ext4_write_file_chunk:
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
    push r15
    sub rsp, 16                         ; [rsp+0]: bytes_written, [rsp+8]: curr_off

    test rcx, rcx
    jz .wfc_zero                        ; 0 bytes: success

    mov qword [rsp + 0], 0              ; bytes_written = 0
    mov [rsp + 8], rdx                  ; curr_off = starting offset

    mov r12d, eax                       ; r12d = inode
    mov r13, rsi                        ; r13 = source data pointer
    mov r14, rcx                        ; r14 = bytes remaining

    ; Load inode
    mov eax, r12d
    call ext4_inode_load
    jc .wfc_err

    ; Verify regular file (i_mode & 0xF000 == 0x8000)
    movzx edx, word [r15 + ext4_inode - kmain + IN_OFF_MODE]
    and edx, 0xF000
    cmp edx, 0x8000
    jne .wfc_err

.wfc_loop:
    test r14, r14
    jz .wfc_finish

    ; Current file offset
    mov rax, [rsp + 8]                  ; rax = curr_off
    xor edx, edx
    mov ecx, 1024
    div ecx                             ; rax = L, edx = O
    mov r8d, eax                        ; r8d = logical block L
    mov r9d, edx                        ; r9d = offset in block O

    ; Space left in this 1024-byte block
    mov ecx, 1024
    sub ecx, r9d                        ; ecx = 1024 - O
    cmp r14, rcx
    jbe @f
    mov r10d, ecx                       ; clamp to space in block
    jmp .wfc_sz_ready
@@:
    mov r10d, r14d                      ; remaining fits in block
.wfc_sz_ready:

    ; Map logical block r8d
    mov eax, r8d
    call ext4_map
    cmp rax, -1
    jne .wfc_got_phys                   ; already mapped!

    ; Not mapped: allocate physical block
    call ext4_alloc_block
    cmp rax, -1
    je .wfc_err
    mov r11d, eax                       ; r11d = new physical block P

    ; Attach P (logical block L = r8d) to ext4_inode
    cmp word [r15 + ext4_inode - kmain + IN_OFF_BLOCK], EH_MAGIC
    jne .wfc_init_ext
    movzx ecx, word [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 2] ; eh_entries
    test ecx, ecx
    jz .wfc_init_ext

    ; Check if contiguous with last extent (index ecx - 1)
    dec ecx
    imul ecx, 12
    add ecx, IN_OFF_BLOCK + 12          ; ecx = offset of last extent
    mov edx, dword [r15 + ext4_inode - kmain + rcx + 0] ; last ee_block
    movzx esi, word [r15 + ext4_inode - kmain + rcx + 4] ; last ee_len
    mov edi, dword [r15 + ext4_inode - kmain + rcx + 8] ; last ee_start_lo

    lea eax, [edx + esi]
    cmp r8d, eax
    jne .wfc_new_ext
    lea eax, [edi + esi]
    cmp r11d, eax
    jne .wfc_new_ext
    cmp esi, 32767
    jae .wfc_new_ext

    ; Contiguous: increment ee_len
    inc word [r15 + ext4_inode - kmain + rcx + 4]
    jmp .wfc_ext_done

.wfc_new_ext:
    movzx ecx, word [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 2] ; eh_entries
    cmp ecx, 4
    jae .wfc_free_and_fail              ; 4 extents limit reached

    imul ecx, 12
    add ecx, IN_OFF_BLOCK + 12
    mov dword [r15 + ext4_inode - kmain + rcx + 0], r8d  ; ee_block = L
    mov word  [r15 + ext4_inode - kmain + rcx + 4], 1    ; ee_len = 1
    mov word  [r15 + ext4_inode - kmain + rcx + 6], 0    ; ee_start_hi = 0
    mov dword [r15 + ext4_inode - kmain + rcx + 8], r11d ; ee_start_lo = P
    inc word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 2]
    jmp .wfc_ext_done

.wfc_init_ext:
    mov word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 0], EH_MAGIC
    mov word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 2], 1
    mov word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 4], 4
    mov word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 6], 0
    mov dword [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 8], 0
    mov dword [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 12], r8d
    mov word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 16], 1
    mov word  [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 18], 0
    mov dword [r15 + ext4_inode - kmain + IN_OFF_BLOCK + 20], r11d

.wfc_ext_done:
    test r9d, r9d
    jnz .wfc_new_partial
    cmp r10d, 1024
    je .wfc_write_direct

.wfc_new_partial:
    lea rdi, [r15 + ext4_wbuf - kmain]
    mov ecx, 1024 / 8
    xor eax, eax
    rep stosq

    lea rdi, [r15 + ext4_wbuf - kmain + r9]
    mov rsi, r13
    mov ecx, r10d
    rep movsb

    mov eax, r11d
    lea rsi, [r15 + ext4_wbuf - kmain]
    call ext4_blk_write
    jc .wfc_err
    jmp .wfc_step_done

.wfc_got_phys:
    mov r11d, eax                       ; r11d = existing physical block P
    test r9d, r9d
    jnz .wfc_read_modify
    cmp r10d, 1024
    je .wfc_write_direct

.wfc_read_modify:
    mov eax, r11d
    lea rsi, [r15 + ext4_wbuf - kmain]
    call ext4_blk_read
    jc .wfc_err

    lea rdi, [r15 + ext4_wbuf - kmain + r9]
    mov rsi, r13
    mov ecx, r10d
    rep movsb

    mov eax, r11d
    lea rsi, [r15 + ext4_wbuf - kmain]
    call ext4_blk_write
    jc .wfc_err
    jmp .wfc_step_done

.wfc_write_direct:
    mov eax, r11d
    mov rsi, r13
    call ext4_blk_write
    jc .wfc_err

.wfc_step_done:
    add [rsp + 0], r10                  ; bytes_written += chunk
    add [rsp + 8], r10                  ; curr_off += chunk
    add r13, r10                        ; buffer += chunk
    sub r14, r10                        ; remaining -= chunk
    jmp .wfc_loop

.wfc_finish:
    ; Update file size if grew
    mov rax, [rsp + 8]                  ; final offset
    cmp rax, qword [r15 + ext4_inode - kmain + IN_OFF_SIZE]
    jbe @f
    mov qword [r15 + ext4_inode - kmain + IN_OFF_SIZE], rax
    add rax, 511
    shr rax, 9
    mov dword [r15 + ext4_inode - kmain + IN_OFF_BLOCKS], eax
@@:
    mov eax, r12d
    call ext4_inode_save
    jc .wfc_err

    call disk_flush_cache

    mov rax, [rsp + 0]                  ; return bytes_written
    clc
    jmp .wfc_pop

.wfc_free_and_fail:
    mov edi, r11d
    mov esi, 1
    call ext4_free_blocks
.wfc_err:
    mov rax, -1
    stc
    jmp .wfc_pop

.wfc_zero:
    xor eax, eax
    clc

.wfc_pop:
    add rsp, 16
    pop r15
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

; ================= write data to file (truncate + write from 0) =================
; RAX = inode number, RSI = data_buffer, RDX = count.
; Out: RAX = bytes written (or -1 on error). CF=0 on success, 1 on error.
ext4_write_file:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    mov ebx, eax                        ; ebx = inode
    mov rdi, rsi                        ; rdi = data_buffer
    mov rcx, rdx                        ; rcx = count

    ; 1. Truncate file to 0
    mov eax, ebx
    xor edx, edx
    call ext4_truncate_file
    jc .wf_err

    ; 2. Write chunk from offset 0
    mov eax, ebx                        ; inode
    xor edx, edx                        ; offset = 0
    mov rsi, rdi                        ; data_buffer
    ; rcx = count
    call ext4_write_file_chunk
    jc .wf_err

    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    clc
    ret

.wf_err:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    mov rax, -1
    stc
    ret

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
    cmp eax, dword [r15 + ext4_groups_count - kmain]
    jae .err                                ; group out of range: never index GDT
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
    mov eax, edx                            ; temp: end = block+len
    add eax, ecx
    cmp r9d, eax                            ; L >= end ?
    jae .leaf_next
    mov eax, r9d
    sub eax, edx                            ; off = L - ee_block
    mov edx, [rdi + 8]                      ; ee_start_lo (32-bit)
    sub ecx, eax                            ; ecx = remaining contiguous blocks in extent
    add rax, rdx                            ; phys = start_lo + off
    cmp eax, dword [r15 + ext4_sb - kmain + 0x04] ; bounds check against s_blocks_count_lo
    jae .none
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
    mov r10d, dword [r15 + ext4_inode - kmain + IN_OFF_SIZE]
    cmp rax, r10
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

; ================= read directory entries =================
; EAX = directory inode, RDI = output buffer, RSI = buffer size,
; RDX = starting byte offset. Fills struct linux_dirent64 records.
; Returns: RAX = bytes written (-1 on error), RDX = final directory offset.
ext4_read_dir:
    push rbx
    push r12
    push r13
    push r14
    push rbp
    sub rsp, 32                 ; [rsp+0]=cur_off, [rsp+8]=blk_idx, [rsp+16]=off_in_blk, [rsp+24]=blk_size

    mov [rsp + 0], rdx          ; cur_off = starting byte offset
    mov r12, rdi                ; r12 = user output buffer
    mov r13, rsi                ; r13 = buffer capacity
    xor r14, r14                ; r14 = bytes written

    call ext4_inode_load
    jc .rd_err

    ; Calculate block size = 1024 << ext4_lbs
    mov ecx, [r15 + ext4_lbs - kmain]
    mov r8d, 1024
    shl r8d, cl
    mov [rsp + 24], r8          ; save blk_size

    ; Starting block index and offset in block from starting byte offset
    mov rax, [rsp + 0]          ; cur_off
    xor rdx, rdx
    div r8                      ; rax = blk_idx, rdx = off_in_blk
    mov [rsp + 8], rax          ; blk_idx
    mov [rsp + 16], rdx         ; off_in_blk

.rd_block_loop:
    mov rax, [rsp + 8]          ; blk_idx
    mov ecx, [r15 + ext4_lbs - kmain]
    add ecx, 10
    shl rax, cl                 ; logical byte offset of this block
    mov r10d, dword [r15 + ext4_inode - kmain + IN_OFF_SIZE]
    cmp rax, r10
    jae .rd_done

    mov eax, dword [rsp + 8]    ; blk_idx
    call ext4_map
    cmp rax, -1
    je .rd_err

    lea rsi, [r15 + ext4_bbuf - kmain]
    call ext4_blk_read
    jc .rd_err

    ; r11 = current entry pointer in buffer
    lea r11, [r15 + ext4_bbuf - kmain]
    add r11, [rsp + 16]         ; + off_in_blk

    ; r10 = end of block buffer
    lea r10, [r15 + ext4_bbuf - kmain]
    add r10, [rsp + 24]         ; + blk_size

.rd_entry_loop:
    cmp r11, r10
    jae .rd_next_block

    ; Ensure we can read the fixed ext4_dir_entry header (8 bytes)
    lea rax, [r11 + 8]
    cmp rax, r10
    ja .rd_next_block

    movzx ebx, word [r11 + 4]   ; rec_len (bytes to advance in ext4 block)
    test ebx, ebx
    jz .rd_next_block

    lea rax, [r11 + rbx]
    cmp rax, r10
    ja .rd_next_block

    mov eax, [r11 + 0]          ; entry inode
    test eax, eax
    jz .rd_skip                 ; deleted / unused directory entry

    ; Valid entry: extract name_len and file_type
    movzx ecx, byte [r11 + 6]   ; name_len
    movzx ebp, byte [r11 + 7]   ; file_type (d_type)

    ; Calculate linux_dirent64 reclen:
    ; header = 19 bytes (d_ino 8, d_off 8, d_reclen 2, d_type 1)
    ; total = (19 + name_len + 1 null byte + 7) & ~7
    lea edx, [ecx + 20 + 7]
    and edx, -8                 ; edx = dirent64 reclen

    ; Check if entry fits into user buffer
    mov rax, r14
    add rax, rdx
    cmp rax, r13
    jbe @f
    ; Buffer cannot fit this entry: if nothing written yet, return EINVAL (-2)
    test r14, r14
    jz .rd_einval
    jmp .rd_done                 ; buffer full, stop and return what we have
@@:
    ; Fill struct linux_dirent64 at [r12 + r14]:
    mov eax, [r11 + 0]          ; inode
    mov [r12 + r14 + 0], rax    ; d_ino (64-bit zero extended)

    mov rax, [rsp + 0]          ; cur_off
    add rax, rbx                ; d_off = offset after this entry
    mov [r12 + r14 + 8], rax    ; d_off

    mov [r12 + r14 + 16], dx    ; d_reclen (16-bit)

    ; Translate ext4 file_type to Linux dirent64 d_type:
    cmp ebp, 7
    jbe .rd_type_ok
    xor ebp, ebp
.rd_type_ok:
    movzx eax, byte [r15 + ext4_dtype_to_dirent - kmain + rbp]
    mov [r12 + r14 + 18], al    ; d_type (8-bit)

    ; Copy name into d_name (starts at offset 19)
    push rcx
    push rdi
    push rsi
    lea rsi, [r11 + 8]          ; ext4 entry name
    lea rdi, [r12 + r14 + 19]   ; d_name in dirent64
    rep movsb
    mov byte [rdi], 0           ; null-terminate
    pop rsi
    pop rdi
    pop rcx

    add r14, rdx                ; total bytes written += dirent64 reclen

.rd_skip:
    add [rsp + 0], rbx          ; cur_off += rec_len
    add r11, rbx                ; entry ptr += rec_len
    jmp .rd_entry_loop

.rd_next_block:
    mov qword [rsp + 16], 0     ; off_in_blk = 0 for subsequent blocks
    inc qword [rsp + 8]         ; blk_idx++
    jmp .rd_block_loop

.rd_done:
    mov rax, r14                ; bytes written
    mov rdx, [rsp + 0]          ; final offset
    add rsp, 32
    pop rbp
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

.rd_einval:
    mov rax, -2                 ; -EINVAL
    add rsp, 32
    pop rbp
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

.rd_err:
    mov rax, -1
    add rsp, 32
    pop rbp
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

; ================= resolve path without following final symlink =================
; RSI = ASCIIZ path -> RAX = inode or 0.
; ================= resolve path at base directory inode =================
; EAX = base dir inode (for relative paths), RSI = ASCIIZ path -> RAX = inode or 0.
ext4_lookup_at_nofollow:
    mov edx, 1                          ; nofollow = 1
    jmp ext4_lookup_at_common

ext4_lookup_at:
    xor edx, edx                        ; nofollow = 0
    jmp ext4_lookup_at_common

; ================= resolve path without following final symlink =================
; RSI = ASCIIZ path -> RAX = inode or 0.
ext4_lookup_nofollow:
    mov eax, [r15 + ext4_cwd_inode - kmain]
    mov edx, 1                          ; nofollow = 1
    jmp ext4_lookup_at_common

; ================= resolve path "/a/b/c" following symlinks =================
; RSI = ASCIIZ path -> RAX = inode or 0. Path buffer is not modified.
ext4_lookup:
    mov eax, [r15 + ext4_cwd_inode - kmain]
    xor edx, edx                        ; nofollow = 0
    jmp ext4_lookup_at_common

ext4_lookup_at_common:
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push rax                            ; [rsp + 8] = base inode
    push rdx                            ; [rsp + 0] = nofollow flag
    mov r12, rdx                        ; r12 = nofollow flag
    mov ebx, eax                        ; ebx = base inode default
    mov r13d, eax                       ; r13d = parent directory inode (init to base)
    cmp byte [rsi], '/'
    jne .have_comp
    mov ebx, 2                          ; absolute -> root inode
    mov r13d, 2
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
    mov byte [rdi], 0                       ; null-terminate e4t_name
    push rsi                                ; preserve path pointer!
    push rcx                                ; preserve extracted name length!
    mov eax, ebx
    call ext4_inode_load                    ; load current dir
    pop rcx                                 ; restore extracted name length!
    jc .fail_pop
    lea rsi, [r15 + e4t_name - kmain]
    call ext4_dir_find

    test eax, eax
    jz .fail_pop
    mov r13d, ebx                           ; r13d = parent directory inode!
    mov ebx, eax                            ; new cur
    call ext4_inode_load
    jc .fail_pop

    ; Check if this inode is a symlink (S_IFLNK = 0xA000)
    movzx edx, word [r15 + ext4_inode - kmain + IN_OFF_MODE]
    and edx, 0xF000
    cmp edx, 0xA000
    jne .not_symlink

    ; Check if we should follow this symlink:
    ; Always follow if more components follow in path (byte [rsi] != 0).
    ; If this is the LAST component, follow only if r12 == 0 (follow mode).
    mov rdx, [rsp]                           ; saved path pointer
    cmp byte [rdx], 0
    jne .do_follow
    test r12, r12
    jnz .not_symlink                         ; do not follow last component!

.do_follow:
    ; Symlink loop guard: max 40 symlink hops
    cmp dword [r15 + ext4_lookup_depth - kmain], 40
    jae .fail_pop                           ; too many symlink levels (ELOOP) -> fail lookup

    inc dword [r15 + ext4_lookup_depth - kmain]

    ; Check if target is fast symlink (i_size <= 59)
    mov edx, dword [r15 + ext4_inode - kmain + IN_OFF_SIZE]
    cmp edx, 59
    ja .do_follow_long

    ; Fast symlink: target is in i_block (starts at offset 40)
    ; Allocate 64 bytes on stack to hold target path string
    sub rsp, 64
    push rdi
    push rsi
    push rcx
    lea rsi, [r15 + ext4_inode - kmain + IN_OFF_BLOCK]
    lea rdi, [rsp + 24]
    mov ecx, edx
    cmp ecx, 63
    jbe @f
    mov ecx, 63
@@:
    rep movsb
    mov byte [rdi], 0                       ; null-terminate
    pop rcx
    pop rsi
    pop rdi

    ; Recursively resolve fast symlink target relative to parent directory (r13d)
    lea rsi, [rsp]
    mov eax, r13d
    call ext4_lookup_at
    add rsp, 64
    dec dword [r15 + ext4_lookup_depth - kmain]
    test eax, eax
    jz .fail_pop
    mov ebx, eax                            ; update current inode to resolved target
    call ext4_inode_load                    ; load target inode
    jc .fail_pop
    jmp .not_symlink

.do_follow_long:
    ; Long symlink: target path is stored in data block
    ; Allocate 256 bytes on stack to hold target path string
    sub rsp, 256
    push rsi                                ; save outer path pointer
    push rdi
    push rcx
    xor eax, eax                            ; logical block 0
    call ext4_map
    cmp rax, -1
    je .follow_long_fail
    ; Read block into ext4_wbuf
    lea rsi, [r15 + ext4_wbuf - kmain]
    call ext4_blk_read
    jc .follow_long_fail

    ; Copy up to 255 bytes from ext4_wbuf to stack buffer [rsp + 24]
    mov eax, dword [r15 + ext4_inode - kmain + IN_OFF_SIZE]
    cmp eax, 254
    jbe @f
    mov eax, 254
@@:
    mov ecx, eax
    lea rsi, [r15 + ext4_wbuf - kmain]
    lea rdi, [rsp + 24]
    rep movsb
    mov byte [rdi], 0                       ; null-terminate

    ; Recursively resolve long symlink target!
    lea rsi, [rsp + 24]
    mov eax, r13d                           ; parent directory inode
    call ext4_lookup_at
    pop rcx
    pop rdi
    pop rsi                                 ; restore outer path pointer
    add rsp, 256
    dec dword [r15 + ext4_lookup_depth - kmain]
    test eax, eax
    jz .fail_pop
    mov ebx, eax                            ; update current inode to resolved target
    call ext4_inode_load                    ; load target inode
    jc .fail_pop
    jmp .not_symlink

.follow_long_fail:
    pop rcx
    pop rdi
    pop rsi
    add rsp, 256
    dec dword [r15 + ext4_lookup_depth - kmain]
    jmp .fail_pop

.not_symlink:
    pop rsi                                 ; restore path pointer!
    jmp .skip_slash                         ; handles '/' and end-of-path
.done:
    mov eax, ebx
.pop3:
    add rsp, 16                             ; discard rax (base inode) and rdx (nofollow flag)
    pop r13
    pop r12
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
    mov r10d, dword [r15 + ext4_inode - kmain + IN_OFF_SIZE]
    cmp rax, r10
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

; ; ================= read file chunk with offset =================
; Inputs:
;   RSI = ASCIIZ file path
;   RDI = destination buffer
;   RCX = max bytes to read
;   RDX = file offset
; Outputs:
;   RAX = bytes read (0 on EOF/error)
;   CF  = 0 on success, 1 on error
ext4_read_file_chunk:
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15

    mov r12, rcx                    ; r12 = max bytes
    mov r14, rdx                    ; r14 = file offset

    ; Step 1: lookup file path
    call ext4_lookup
    test eax, eax
    jz ext4_read_file_chunk_inode.rfc_err

    ; Step 2: load inode
    call ext4_inode_load
    jc ext4_read_file_chunk_inode.rfc_err
    jmp ext4_read_file_chunk_inode.rfc_have_inode

; ================= read file chunk directly by inode =================
; Inputs:
;   EAX = inode number
;   RDI = destination buffer
;   RCX = max bytes to read
;   RDX = file offset
; Outputs:
;   RAX = bytes read (0 on EOF/error)
;   CF  = 0 on success, 1 on error
ext4_read_file_chunk_inode:
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15

    mov r12, rcx                    ; r12 = max bytes
    mov r14, rdx                    ; r14 = file offset

    ; Load inode directly
    call ext4_inode_load
    jc .rfc_err

.rfc_have_inode:
    ; Step 3: verify it is a regular file (i_mode & 0xF000 == 0x8000)
    movzx edx, word [r15 + ext4_inode - kmain + IN_OFF_MODE]
    and edx, 0xF000
    cmp edx, 0x8000
    jne .rfc_err

    ; Step 4: get file size
    mov eax, dword [r15 + ext4_inode - kmain + IN_OFF_SIZE]
    mov dword [r15 + ext4_inode - kmain + IN_OFF_SIZE + 4], 0

    ; Check if offset >= file size (EOF)
    cmp r14, rax
    jae .rfc_zero

    ; Available bytes in file = file_size - offset
    sub rax, r14
    cmp r12, rax
    jbe @f
    mov r12, rax                    ; clamp to available bytes
@@:
    test r12, r12
    jz .rfc_zero

    xor r13d, r13d                  ; r13 = bytes read so far

.rfc_blk_loop:
    cmp r13, r12
    jae .rfc_success

    ; Block size = 1024
    ; Logical block = r14 / 1024
    ; Block offset  = r14 % 1024
    mov rax, r14
    xor edx, edx
    mov ebx, 1024
    div ebx                         ; eax = logical block, edx = block offset

    push rdx                        ; save block offset

    ; Map logical block to physical block
    call ext4_map
    pop rdx                         ; restore block offset
    ; RAX = phys block, EDX = block offset

    ; Remaining bytes in this 1024-byte block
    mov ecx, 1024
    sub ecx, edx

    ; Remaining bytes to read overall
    mov rbx, r12
    sub rbx, r13
    cmp rbx, rcx
    jbe @f
    mov rbx, rcx                    ; rbx = chunk size in this block
@@:

    cmp rax, -1
    je .rfc_sparse

    ; Read physical block into ext4_bbuf
    push rbx
    push rdx
    push rdi
    lea rsi, [r15 + ext4_bbuf - kmain]
    call ext4_blk_read
    pop rdi
    pop rdx
    pop rbx
    jc .rfc_err

    ; Copy rbx bytes from ext4_bbuf + edx to [rdi + r13]
    push rsi
    push rdi
    push rcx
    lea rsi, [r15 + ext4_bbuf - kmain + rdx]
    lea rdi, [rdi + r13]
    mov rcx, rbx
    rep movsb
    pop rcx
    pop rdi
    pop rsi
    jmp .rfc_adv

.rfc_sparse:
    ; Zero-fill sparse block chunk
    push rdi
    push rcx
    lea rdi, [rdi + r13]
    xor eax, eax
    mov rcx, rbx
    rep stosb
    pop rcx
    pop rdi

.rfc_adv:
    add r13, rbx
    add r14, rbx
    jmp .rfc_blk_loop

.rfc_success:
    mov rax, r12
    clc
    jmp .rfc_done

.rfc_zero:
    xor eax, eax
    clc
    jmp .rfc_done

.rfc_err:
    xor eax, eax
    stc

.rfc_done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
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
    xor edx, edx                    ; offset = 0
    jmp ext4_read_file_chunk

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
    mov eax, dword [r15 + ext4_inode - kmain + IN_OFF_SIZE]
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
ext4_lookup_depth dd 0
ext4_cwd_path  db "/", 0
               rb 62

; ext4 file_type -> linux dirent64 d_type mapping:
; ext4: 0=unknown, 1=REG, 2=DIR, 3=CHR, 4=BLK, 5=FIFO, 6=SOCK, 7=LNK
; dirent64: 0=DT_UNKNOWN, 8=DT_REG, 4=DT_DIR, 2=DT_CHR, 6=DT_BLK, 1=DT_FIFO, 12=DT_SOCK, 10=DT_LNK
ext4_dtype_to_dirent db 0, 8, 4, 2, 6, 1, 12, 10

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