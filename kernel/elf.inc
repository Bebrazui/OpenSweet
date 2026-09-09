; ==============================================================================
; Opensweet OS - Native 64-bit ELF (ELF64) Loader (kernel/elf.inc)
; Parses, validates, and maps static x86_64 ELF executables from ext4 / memory.
; Sets up AMD64 System V ABI user stack and spawns Ring 3 user processes.
; ==============================================================================

ELF_MAGIC             = 0x464C457F      ; 0x7F, 'E', 'L', 'F'
ELFCLASS64            = 2
ELFDATA2LSB           = 1
EM_X86_64             = 0x3E
ET_EXEC               = 2
ET_DYN                = 3
PT_LOAD               = 1

; Return / Error Codes
ELF_OK                = 0
ELF_ERR_NOT_FOUND     = -1
ELF_ERR_READ_FAIL     = -2
ELF_ERR_TOO_SMALL     = -3
ELF_ERR_BAD_MAGIC     = -4
ELF_ERR_NOT_64BIT     = -5
ELF_ERR_BAD_MACHINE   = -6
ELF_ERR_BAD_TYPE      = -7
ELF_ERR_NO_LOAD       = -8
ELF_ERR_OOM           = -9
ELF_ERR_SPAWN_FAIL    = -10

; Elf64_Ehdr field offsets (64 bytes)
EHDR_OFF_IDENT        = 0               ; 16 bytes
EHDR_OFF_TYPE         = 16              ; uint16
EHDR_OFF_MACHINE      = 18              ; uint16
EHDR_OFF_VERSION      = 20              ; uint32
EHDR_OFF_ENTRY        = 24              ; uint64
EHDR_OFF_PHOFF        = 32              ; uint64
EHDR_OFF_SHOFF        = 40              ; uint64
EHDR_OFF_FLAGS        = 48              ; uint32
EHDR_OFF_EHSIZE       = 52              ; uint16
EHDR_OFF_PHENTSIZE    = 54              ; uint16
EHDR_OFF_PHNUM        = 56              ; uint16
EHDR_OFF_SHENTSIZE    = 58              ; uint16
EHDR_OFF_SHNUM        = 60              ; uint16
EHDR_OFF_SHSTRNDX     = 62              ; uint16

; Elf64_Phdr field offsets (56 bytes)
PHDR_OFF_TYPE         = 0               ; uint32 (PT_LOAD=1)
PHDR_OFF_FLAGS        = 4               ; uint32 (PF_X=1, PF_W=2, PF_R=4)
PHDR_OFF_OFFSET       = 8               ; uint64
PHDR_OFF_VADDR        = 16              ; uint64
PHDR_OFF_PADDR        = 24              ; uint64
PHDR_OFF_FILESZ       = 32              ; uint64
PHDR_OFF_MEMSZ        = 40              ; uint64
PHDR_OFF_ALIGN        = 48              ; uint64

align 16
str_elf_load_start    db "[ELF] Loading executable: '", 0
str_elf_load_size     db "' (", 0
str_elf_bytes         db " bytes)...", 10, 0
str_elf_entry_log     db "[ELF] Entry point: 0x", 0
str_elf_segs_log      db ", loaded segments: ", 0
str_elf_spawned       db 10, "[ELF] Ring 3 process spawned! TID=", 0
str_elf_nl            db 10, 0

str_elf_err_fnf       db "[ELF] Error: File not found on ext4 filesystem.", 10, 0
str_elf_err_read      db "[ELF] Error: Failed to read file from ext4.", 10, 0
str_elf_err_small     db "[ELF] Error: File too small to be a valid ELF64 binary.", 10, 0
str_elf_err_magic     db "[ELF] Error: Invalid ELF magic signature (not \x7fELF).", 10, 0
str_elf_err_not64     db "[ELF] Error: Not a 64-bit ELF binary (ELFCLASS64 required).", 10, 0
str_elf_err_endian    db "[ELF] Error: Unsupported endianness (Little Endian required).", 10, 0
str_elf_err_mach      db "[ELF] Error: Unsupported machine architecture (x86_64 required).", 10, 0
str_elf_err_type      db "[ELF] Error: Unsupported ELF type (ET_EXEC or ET_DYN required).", 10, 0
str_elf_err_noload    db "[ELF] Error: No valid PT_LOAD segments found.", 10, 0
str_elf_err_oom       db "[ELF] Error: Out of kernel heap memory to buffer executable.", 10, 0
str_elf_err_spawn     db "[ELF] Error: Failed to spawn user task (no free TCB slots).", 10, 0

; ==============================================================================
; elf_load_from_ext4: Read ELF binary from ext4 filesystem and execute in Ring 3
; RSI = file path (ASCIIZ)
; Returns: EAX = Task ID (or negative error code)
; ==============================================================================
elf_load_from_ext4:
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

    mov r14, rsi                      ; r14 = original path string

    ; 1. Check if filesystem is mounted
    cmp byte [r15 + ext4_ok - kmain], 0
    jne @f
    mov eax, ELF_ERR_READ_FAIL
    lea rsi, [r15 + str_elf_err_read - kmain]
    call puts
    jmp .elff_ret
@@:

    ; 2. Lookup file inode on ext4
    mov rsi, r14
    call ext4_lookup
    test eax, eax
    jnz @f
    mov eax, ELF_ERR_NOT_FOUND
    lea rsi, [r15 + str_elf_err_fnf - kmain]
    call puts
    jmp .elff_ret
@@:

    ; 3. Load inode details
    call ext4_inode_load
    jnc @f
    mov eax, ELF_ERR_READ_FAIL
    lea rsi, [r15 + str_elf_err_read - kmain]
    call puts
    jmp .elff_ret
@@:

    ; 4. Verify regular file (i_mode & 0xF000 == 0x8000)
    movzx edx, word [r15 + ext4_inode - kmain + IN_OFF_MODE]
    and edx, 0xF000
    cmp edx, 0x8000
    je @f
    mov eax, ELF_ERR_NOT_FOUND
    lea rsi, [r15 + str_elf_err_fnf - kmain]
    call puts
    jmp .elff_ret
@@:

    ; 5. Verify file size
    mov rax, qword [r15 + ext4_inode - kmain + IN_OFF_SIZE]
    cmp rax, 64                       ; sizeof(Elf64_Ehdr)
    jae @f
    mov eax, ELF_ERR_TOO_SMALL
    lea rsi, [r15 + str_elf_err_small - kmain]
    call puts
    jmp .elff_ret
@@:
    cmp rax, 16*1024*1024             ; max 16MB
    jbe @f
    mov eax, ELF_ERR_TOO_SMALL
    jmp .elff_ret
@@:
    mov r12, rax                      ; r12 = file size

    ; Print load banner
    mov eax, FB_CLR_HEADER
    call fb_console_set_color
    lea rsi, [r15 + str_elf_load_start - kmain]
    call puts
    mov eax, FB_CLR_FILE
    call fb_console_set_color
    mov rsi, r14
    call puts
    mov eax, FB_CLR_LABEL
    call fb_console_set_color
    lea rsi, [r15 + str_elf_load_size - kmain]
    call puts
    mov eax, FB_CLR_NUMBER
    call fb_console_set_color
    mov rax, r12
    call putdec64
    mov eax, FB_CLR_LABEL
    call fb_console_set_color
    lea rsi, [r15 + str_elf_bytes - kmain]
    call puts
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color

    ; 6. Allocate buffer from Kernel Heap
    mov rcx, r12
    call kmalloc
    test rax, rax
    jnz @f
    mov eax, ELF_ERR_OOM
    lea rsi, [r15 + str_elf_err_oom - kmain]
    call puts
    jmp .elff_ret
@@:
    mov r13, rax                      ; r13 = buffer pointer

    ; 7. Read file data from ext4 into allocated buffer
    mov rsi, r14                      ; file path
    mov rdi, r13                      ; destination buffer
    mov rcx, r12                      ; byte count
    call ext4_read_file_to_mem
    jnc @f
    ; Read failed
    mov rdi, r13
    call kfree
    mov eax, ELF_ERR_READ_FAIL
    lea rsi, [r15 + str_elf_err_read - kmain]
    call puts
    jmp .elff_ret
@@:

    ; 8. Extract basename from path for process name
    mov rsi, r14
    call elf_get_basename
    mov rdi, rax                      ; rdi = process name

    ; 9. Load and execute ELF from memory buffer
    mov rsi, r13                      ; rsi = buffer
    mov rdx, r12                      ; rdx = size
    call elf_load_exec
    mov ebx, eax                      ; ebx = TID or error code

    ; 10. Free memory buffer
    mov rdi, r13
    call kfree

    mov eax, ebx                      ; return TID or error

.elff_ret:
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

; ==============================================================================
; elf_load_exec: Parse ELF64 headers, map segments, and spawn Ring 3 task
; RSI = buffer with ELF file data
; RDX = file size in bytes
; RDI = process name string
; Returns: EAX = Task ID (or negative error code)
; ==============================================================================
elf_load_exec:
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

    mov r12, rsi                      ; r12 = buffer base
    mov r13, rdx                      ; r13 = file size
    mov r14, rdi                      ; r14 = process name

    ; 1. Validate ELF magic signature: \x7fELF
    cmp dword [r12 + EHDR_OFF_IDENT], ELF_MAGIC
    je @f
    mov eax, ELF_ERR_BAD_MAGIC
    lea rsi, [r15 + str_elf_err_magic - kmain]
    call puts
    jmp .exec_fail
@@:

    ; 2. Validate ELFCLASS64 (bit 4 == 2)
    cmp byte [r12 + EHDR_OFF_IDENT + 4], ELFCLASS64
    je @f
    mov eax, ELF_ERR_NOT_64BIT
    lea rsi, [r15 + str_elf_err_not64 - kmain]
    call puts
    jmp .exec_fail
@@:

    ; 3. Validate ELFDATA2LSB (bit 5 == 1)
    cmp byte [r12 + EHDR_OFF_IDENT + 5], ELFDATA2LSB
    je @f
    mov eax, ELF_ERR_NOT_64BIT
    lea rsi, [r15 + str_elf_err_endian - kmain]
    call puts
    jmp .exec_fail
@@:

    ; 4. Validate Machine Architecture: x86_64 (0x3E)
    cmp word [r12 + EHDR_OFF_MACHINE], EM_X86_64
    je @f
    mov eax, ELF_ERR_BAD_MACHINE
    lea rsi, [r15 + str_elf_err_mach - kmain]
    call puts
    jmp .exec_fail
@@:

    ; 5. Validate Executable Type: ET_EXEC (2) or ET_DYN (3)
    movzx eax, word [r12 + EHDR_OFF_TYPE]
    cmp eax, ET_EXEC
    je @f
    cmp eax, ET_DYN
    je @f
    mov eax, ELF_ERR_BAD_TYPE
    lea rsi, [r15 + str_elf_err_type - kmain]
    call puts
    jmp .exec_fail
@@:

    ; 6. Extract entry point and program header info
    mov r8,  [r12 + EHDR_OFF_ENTRY]    ; r8  = entry point virtual address
    mov r9,  [r12 + EHDR_OFF_PHOFF]    ; r9  = program header offset
    movzx r10d, word [r12 + EHDR_OFF_PHENTSIZE] ; r10d = ph entry size (typically 56)
    movzx r11d, word [r12 + EHDR_OFF_PHNUM]     ; r11d = ph count

    ; Verify program header table bounds
    test r11d, r11d
    jz .no_phdrs
    mov rax, r10
    imul rax, r11
    add rax, r9
    cmp rax, r13
    jbe .phdrs_bounds_ok
.no_phdrs:
    mov eax, ELF_ERR_NO_LOAD
    lea rsi, [r15 + str_elf_err_noload - kmain]
    call puts
    jmp .exec_fail

.phdrs_bounds_ok:
    ; Preserve phdr info on stack across task slot allocation
    push r9                           ; [rsp + 16] = phoff
    push r10                          ; [rsp + 8]  = phentsize
    push r11                          ; [rsp + 0]  = phnum

    ; 7. Allocate user task slot with private page tables and memory
    call task_alloc_user_slot
    cmp eax, -1
    jne .slot_ok

    add rsp, 24
    lea rsi, [r15 + str_elf_err_spawn - kmain]
    call puts
    mov eax, ELF_ERR_SPAWN_FAIL
    jmp .exec_fail

.slot_ok:
    ; EBX = slot ID (1..MAX_TASKS-1)
    ; R8  = pml4_phys
    ; R9  = phys_code
    ; R10 = phys_stack
    ; Current stack has: [rsp+0]=phnum, [rsp+8]=phentsize, [rsp+16]=phoff
    ; Push task slot info:
    push rbx                          ; [rsp + 24] = slot ID
    push r9                           ; [rsp + 16] = phys_code
    push r10                          ; [rsp + 8]  = phys_stack
    push qword [r12 + EHDR_OFF_ENTRY] ; [rsp + 0]  = saved entry point RIP

    ; Stack layout:
    ; [rsp + 0]  = entry RIP
    ; [rsp + 8]  = phys_stack
    ; [rsp + 16] = phys_code
    ; [rsp + 24] = slot ID
    ; [rsp + 32] = phnum
    ; [rsp + 40] = phentsize
    ; [rsp + 48] = phoff

    ; Zero out private physical code (2MB) and stack (2MB)
    mov rdi, [rsp + 16]               ; phys_code
    mov ecx, (2 * 1024 * 1024) / 8
    xor eax, eax
    cld
    rep stosq

    mov rdi, [rsp + 8]                ; phys_stack
    mov ecx, (2 * 1024 * 1024) / 8
    xor eax, eax
    rep stosq

    ; 8. Iterate program headers and load PT_LOAD segments
    xor ebx, ebx                      ; ebx = phdr index (0..phnum-1)
    xor ecx, ecx                      ; ecx = loaded segments counter

.phdr_loop:
    cmp ebx, [rsp + 32]               ; compare with phnum
    jae .phdrs_done

    ; Calculate pointer to current phdr: r12 + phoff + ebx * phentsize
    mov rax, rbx
    imul rax, [rsp + 40]              ; phentsize
    add rax, [rsp + 48]               ; phoff
    lea rdx, [r12 + rax]              ; rdx = phdr ptr

    ; Check if PT_LOAD (type == 1)
    cmp dword [rdx + PHDR_OFF_TYPE], PT_LOAD
    jne .next_phdr

    ; Extract segment parameters
    mov rsi, [rdx + PHDR_OFF_OFFSET]  ; rsi = file offset
    mov rdi, [rdx + PHDR_OFF_VADDR]   ; rdi = virtual memory address
    mov rax, [rdx + PHDR_OFF_FILESZ]  ; rax = file size
    mov r8,  [rdx + PHDR_OFF_MEMSZ]   ; r8  = memory size

    ; Validate file offset + filesz <= file size
    lea rbp, [rsi + rax]
    cmp rbp, r13
    ja .next_phdr

    ; Map virtual address (0x400000..) to slot's private physical code memory
    sub rdi, 0x400000
    add rdi, [rsp + 16]               ; [rsp + 16] = phys_code

    ; Copy segment data from file buffer to destination physical address
    test rax, rax
    jz .check_bss

    push rcx
    push rsi
    push rdi
    lea rsi, [r12 + rsi]              ; source buffer
    mov rcx, rax                      ; bytes count
    rep movsb
    pop rdi
    pop rsi
    pop rcx

.check_bss:
    ; Clear BSS if memsz > filesz
    cmp r8, rax
    jbe .seg_loaded

    push rcx
    push rdi
    lea rdi, [rdi + rax]              ; start of BSS in destination
    mov rcx, r8
    sub rcx, rax                      ; BSS size
    xor eax, eax
    rep stosb
    pop rdi
    pop rcx

.seg_loaded:
    inc ecx                           ; loaded_count++

.next_phdr:
    inc ebx
    jmp .phdr_loop

.phdrs_done:
    pop r8                            ; r8  = saved entry point RIP
    pop r10                           ; r10 = phys_stack
    pop r9                            ; r9  = phys_code
    pop rbx                           ; rbx = slot ID
    add rsp, 24                       ; discard phnum, phentsize, phoff

    test ecx, ecx
    jnz @f
    mov eax, ELF_ERR_NO_LOAD
    lea rsi, [r15 + str_elf_err_noload - kmain]
    call puts
    jmp .exec_fail
@@:
    mov edx, ecx                      ; edx = loaded count

    ; Print diagnostic info
    push rbx
    push r8
    push r10
    lea rsi, [r15 + str_elf_entry_log - kmain]
    call puts
    mov rax, [rsp + 8]                ; r8 = entry point
    call puthex64
    lea rsi, [r15 + str_elf_segs_log - kmain]
    call puts
    mov eax, edx
    call putdec64
    lea rsi, [r15 + str_elf_nl - kmain]
    call puts
    pop r10
    pop r8
    pop rbx

    ; 9. Setup AMD64 System V ABI User Stack at virtual 0x7FFF00
    ; Physical address = phys_stack + 0x1FFF00
    ; Store process name string at physical phys_stack + 0x1FFF80 (virtual 0x7FFF80)
    push rdi
    push rsi
    mov rsi, r14                      ; process name
    lea rdi, [r10 + 0x1FFF80]
    xor ecx, ecx
.copy_arg_name:
    lodsb
    stosb
    test al, al
    jz .arg_name_done
    inc ecx
    cmp ecx, 63
    jb .copy_arg_name
    mov byte [rdi], 0
.arg_name_done:
    pop rsi
    pop rdi

    ; Stack layout at phys_stack + 0x1FFF00 (virtual 0x7FFF00):
    ; [0x00] = 1 (argc)
    ; [0x08] = 0x7FFF80 (argv[0] - user virtual address)
    ; [0x10] = 0 (argv[1] = NULL)
    ; [0x18] = 0 (envp[0] = NULL)
    lea rdi, [r10 + 0x1FFF00]
    mov qword [rdi + 0x00], 1
    mov qword [rdi + 0x08], 0x7FFF80
    mov qword [rdi + 0x10], 0
    mov qword [rdi + 0x18], 0

    ; 10. Finalize Ring 3 User Task!
    ; EBX = slot ID
    ; RSI = process name (r14)
    ; RDX = entry point (r8)
    ; R8  = user stack top (0x7FFF00)
    mov rsi, r14
    mov rdx, r8                       ; entry RIP
    mov r8,  0x7FFF00                 ; user RSP
    call task_finalize_user
    cmp eax, -1
    jne .spawn_ok

    lea rsi, [r15 + str_elf_err_spawn - kmain]
    call puts
    mov eax, ELF_ERR_SPAWN_FAIL
    jmp .exec_fail


.spawn_ok:
    ; Print success
    push rax
    mov eax, FB_CLR_SUCCESS
    call fb_console_set_color
    lea rsi, [r15 + str_elf_spawned - kmain]
    call puts
    mov eax, [rsp]
    call putdec64
    lea rsi, [r15 + str_elf_nl - kmain]
    call puts
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    pop rax
    jmp .exec_ret

.exec_fail:
    ; RAX contains error code

.exec_ret:
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

; ==============================================================================
; elf_get_basename: Extract filename component from path
; RSI = path string (ASCIIZ)
; Returns: RAX = pointer to start of basename within or after slashes
; ==============================================================================
elf_get_basename:
    push rsi
    mov rax, rsi
.gb_loop:
    lodsb
    test al, al
    jz .gb_done
    cmp al, '/'
    jne .gb_loop
    mov rax, rsi                      ; update to character after '/'
    jmp .gb_loop
.gb_done:
    pop rsi
    ret

; ==============================================================================
; elf_check_is_elf_command: Check if input command string ends with ".elf"
; RSI = command string (ASCIIZ)
; Returns: AL = 1 if ends with ".elf", 0 otherwise
; ==============================================================================
elf_check_is_elf_command:
    push rsi
    push rcx
    xor ecx, ecx
.chk_len:
    cmp byte [rsi + rcx], 0
    je .chk_got_len
    inc ecx
    jmp .chk_len
.chk_got_len:
    cmp ecx, 4
    jb .chk_not_elf
    cmp dword [rsi + rcx - 4], 0x666C652E ; '.elf'
    jne .chk_not_elf
    mov al, 1
    jmp .chk_ret
.chk_not_elf:
    xor al, al
.chk_ret:
    pop rcx
    pop rsi
    ret
