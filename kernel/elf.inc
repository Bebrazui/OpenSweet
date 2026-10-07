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
PT_DYNAMIC            = 2
PT_INTERP             = 3

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
str_elf_abi_log       db "[ELF] ABI detect -> ", 0
str_elf_interp_log    db "[ELF] PT_INTERP interpreter: '", 0
str_elf_quote_nl      db "'", 10, 0
str_elf_nl            db 10, 0
str_elf_arg_log       db "[ELF_ARG] ", 0

align 8
elf_has_interp        dq 0
elf_interp_base       dq 0x00600000     ; Base VA to load dynamic interpreter (in code slot)
elf_interp_entry      dq 0
elf_interp_path       rb 128
str_interp_fallback   db "/ld-linux-x86-64.so.2", 0
str_elf_interp_ok     db "[ELF] Dynamic linker loaded at 0x600000!", 10, 0
str_elf_err_interp    db "[ELF] Error: Failed to load dynamic interpreter.", 10, 0

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
    pushfq
    cli

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
    lea rsi, [r15 + .str_fnf_path - kmain]
    call puts
    mov rsi, r14
    call puts
    mov al, 10
    call putc
    jmp .elff_ret
.str_fnf_path: db "[ELF] Requested path: ", 0
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

    ; 5. Verify file size (i_size_lo is 32-bit dword at offset 4)
    mov eax, dword [r15 + ext4_inode - kmain + IN_OFF_SIZE]
    push rax
    lea rsi, [r15 + .str_dbg_sz - kmain]
    call puts
    pop rax
    push rax
    call puthex64
    mov al, 10
    call putc
    pop rax

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
.str_dbg_sz: db "[ELF_DBG] inode.i_size=", 0
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
    popfq
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
    mov qword [r15 + elf_has_interp - kmain], 0

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
    jc .no_phdrs
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
    ; execve mode: reuse the RUNNING task's slot — pid, fds and page tables
    ; survive, only the image/stack/TCB-exec-state are replaced.
    cmp qword [r15 + sys_spawn_exec_mode - kmain], 0
    jne .slot_current
    call task_alloc_user_slot
    cmp eax, -1
    jne .slot_ok

    add rsp, 24
    lea rsi, [r15 + str_elf_err_spawn - kmain]
    call puts
    mov eax, ELF_ERR_SPAWN_FAIL
    jmp .exec_fail

.slot_current:
    mov ebx, [r15 + sched_curr_task - kmain]
    mov eax, ebx
    dec eax                           ; slot -> 0-based
    imul eax, USER_MEM_STRIDE
    add eax, USER_PHYS_BASE           ; phys_code of the current slot
    mov r9, rax
    add rax, 0x00400000
    mov r10, rax                      ; phys_stack (phys_code + 4MB)
    imul eax, ebx, TCB_SIZE
    mov r8, [r15 + task_table - kmain + rax + TCB_CR3]

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
    mov rax, [r12 + EHDR_OFF_ENTRY]
    cmp word [r12 + EHDR_OFF_TYPE], ET_DYN
    jne @f
    cmp rax, 0x400000
    jae @f
    add rax, 0x400000                 ; Relocate PIE entry to 0x400000
@@:
    push rax                          ; [rsp + 0]  = saved entry point RIP

    ; Stack layout:
    ; [rsp + 0]  = entry RIP
    ; [rsp + 8]  = phys_stack
    ; [rsp + 16] = phys_code
    ; [rsp + 24] = slot ID
    ; [rsp + 32] = phnum
    ; [rsp + 40] = phentsize
    ; [rsp + 48] = phoff

    ; Zero out private physical code (4MB) and stack (2MB)
    ; execve: skip the wipe — a failed load must leave the RUNNING image
    ; intact, and stale bytes outside the new segments/BSS are unreachable.
    cmp qword [r15 + sys_spawn_exec_mode - kmain], 0
    jne .skip_zero
    mov rdi, [rsp + 16]               ; phys_code
    mov ecx, (4 * 1024 * 1024) / 8
    xor eax, eax
    cld
    rep stosq

    mov rdi, [rsp + 8]                ; phys_stack
    mov ecx, (2 * 1024 * 1024) / 8
    xor eax, eax
    rep stosq
.skip_zero:

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

    ; Check for PT_INTERP (type == 3)
    cmp dword [rdx + PHDR_OFF_TYPE], PT_INTERP
    je .handle_interp

    ; Check if PT_LOAD (type == 1)
    cmp dword [rdx + PHDR_OFF_TYPE], PT_LOAD
    jne .next_phdr

    ; Extract segment parameters
    mov rsi, [rdx + PHDR_OFF_OFFSET]  ; rsi = file offset
    mov rdi, [rdx + PHDR_OFF_VADDR]   ; rdi = virtual memory address
    mov rax, [rdx + PHDR_OFF_FILESZ]  ; rax = file size
    mov r8,  [rdx + PHDR_OFF_MEMSZ]   ; r8  = memory size

    ; For PIE (ET_DYN), relocate segment virtual address by slot base (0x400000)
    cmp word [r12 + EHDR_OFF_TYPE], ET_DYN
    jne @f
    cmp rdi, 0x400000
    jae @f
    add rdi, 0x400000
@@:

    ; Validate file offset + filesz <= file size (with 64-bit integer overflow check)
    mov rbp, rsi
    add rbp, rax
    jc .next_phdr
    cmp rbp, r13
    ja .next_phdr

    ; Validate segment virtual address and memory bounds
    ; Segment must fall within the user code/data/bss slot [0x400000, 0x800000) (4MB)
    cmp rdi, 0x400000
    jb .next_phdr
    cmp rax, r8                       ; filesz <= memsz
    ja .next_phdr
    cmp r8, 0x400000                  ; memsz <= 4MB
    ja .next_phdr

    mov rbp, rdi
    sub rbp, 0x400000
    add rbp, r8                       ; (vaddr - 0x400000) + memsz
    cmp rbp, 0x400000
    ja .next_phdr

    ; Map virtual address (0x400000..) to slot's private physical memory
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

.handle_interp:
    mov qword [r15 + elf_has_interp - kmain], 1
    mov rsi, [rdx + PHDR_OFF_OFFSET]
    mov eax, dword [rdx + PHDR_OFF_FILESZ]
    cmp eax, 127
    jbe @f
    mov eax, 127
@@:
    push rcx
    push rdi
    push rsi
    mov ecx, eax
    lea rsi, [r12 + rsi]              ; source in elf buffer
    lea rdi, [r15 + elf_interp_path - kmain]
    rep movsb
    mov byte [rdi], 0                 ; null terminate
    ; Log interpreter path
    lea rsi, [r15 + str_elf_interp_log - kmain]
    call puts
    lea rsi, [r15 + elf_interp_path - kmain]
    call puts
    lea rsi, [r15 + str_elf_quote_nl - kmain]
    call puts
    pop rsi
    pop rdi
    pop rcx
    jmp .next_phdr

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

    ; 9. Setup AMD64 System V ABI User Stack
    ; Vector (argc/argv/envp/auxv) lives at phys_stack + 0x1FFE00 (virtual
    ; 0x1FFFFE00); 512 bytes are reserved so auxv can never run past the end
    ; of the stack slot (old base 0x1FFF00 overflowed by up to 0xA0 bytes).
    ; Physical address = phys_stack + 0x1FFE00
    mov r9, [r15 + sys_spawn_argv_ptr - kmain]
    test r9, r9
    jz .fallback_args

    ; Validate argv array pointer (up to 16 entries * 8 bytes = 128 bytes)
    ; execve: the table was staged in KERNEL memory before the old image was
    ; overwritten — skip the user-range check (CF must be defined for the jc).
    push rbx
    cmp qword [r15 + sys_spawn_exec_mode - kmain], 0
    jne @f
    mov rdi, r9
    mov rsi, 8
    call sys_validate_user_ptr
    jmp .argv_arr_checked
@@: clc
.argv_arr_checked:
    pop rbx
    ; Preserve r12 (ELF buffer base) across argv copying
    push r12

    ; Copy arguments into phys_stack + 0x1FF800 (virtual 0x1FFFF800)
    lea r12, [r10 + 0x1FF800]          ; physical destination
    mov r13, 0x1FFFF800                ; virtual destination
    xor ecx, ecx                       ; ecx = argc

.copy_argv_loop:
    cmp ecx, 16
    jae .argv_copied
    mov rsi, [r9 + rcx*8]              ; rsi = user string pointer
    test rsi, rsi
    jz .argv_copied

    ; Validate string pointer (up to 128 bytes string buffer)
    ; execve: staged kernel pointer — skip the user-range check.
    push rcx
    push r9
    push rbx
    push rsi                          ; save user string ptr: the validator is
    cmp qword [r15 + sys_spawn_exec_mode - kmain], 0
    je .str_validate
    clc
    jmp .str_checked
.str_validate:
    mov rdi, rsi
    mov rsi, 1
    call sys_validate_user_ptr
.str_checked:
    pop rsi
    pop rbx
    pop r9
    pop rcx
    jc .argv_copied

    ; H-L7: keep argv strings below the vector at 0x1FFE00 (leave 256 bytes
    ; of slack for the current string); drop remaining args instead of
    ; overwriting the argc/argv/envp/auxv vector or the next task's memory.
    lea rax, [r10 + 0x1FFE00 - 0x100]
    cmp r12, rax
    jae .argv_copied

    ; Store virtual address in argv[rcx] table
    mov [r10 + 0x1FFE00 + 8 + rcx*8], r13

    push rsi
    push r12
    push r13
    push rcx
    lea rsi, [r15 + str_elf_arg_log - kmain]
    call puts
    pop rcx
    push rcx
    mov rax, rcx
    call putdec64
    mov al, '='
    call putc
    mov al, 39
    call putc
    mov rsi, [rsp + 24]                 ; original rsi
    call puts
    mov al, 39
    call putc
    mov al, 10
    call putc
    pop rcx
    pop r13
    pop r12
    pop rsi

    ; Copy string characters
    mov edx, 127                       ; max 127 bytes per arg
.cp_char:
    mov al, [rsi]
    mov [r12], al
    inc rsi
    inc r12
    inc r13
    test al, al
    jz .cp_char_done
    dec edx
    jnz .cp_char
    mov byte [r12 - 1], 0              ; force null terminator
.cp_char_done:
    ; Align r12 and r13 to 8 bytes
    add r12, 7
    and r12, -8
    add r13, 7
    and r13, -8
    inc ecx
    jmp .copy_argv_loop

.argv_copied:
    pop r12                            ; restore ELF buffer base
    test ecx, ecx
    jz .fallback_args

    ; Successfully copied arguments
    mov [r10 + 0x1FFE00], rcx                  ; [0x00] = argc
    mov qword [r10 + 0x1FFE00 + 8 + rcx*8], 0  ; argv[argc] = NULL

    ; Setup standard POSIX environment strings at phys_stack + 0x1FF500 (virtual 0x1FFFF500)
    lea rdi, [r10 + 0x1FF500]
    mov dword [rdi + 0], "PATH"
    mov dword [rdi + 4], "=/bi"
    mov dword [rdi + 8], "n:/"
    mov dword [rdi + 16], "HOME"
    mov word  [rdi + 20], "=/"
    mov byte  [rdi + 22], 0
    mov dword [rdi + 32], "USER"
    mov dword [rdi + 36], "=roo"
    mov word  [rdi + 40], "t"
    mov dword [rdi + 48], "TERM"
    mov dword [rdi + 52], "=xte"
    mov dword [rdi + 56], "rm-2"
    mov dword [rdi + 60], "56co"
    mov dword [rdi + 64], "lor"
    mov dword [rdi + 80], "SHEL"
    mov dword [rdi + 84], "L=/s"
    mov word  [rdi + 88], "h"

    ; Populate envp pointers
    lea rdi, [r10 + 0x1FFE00 + 8 + rcx*8 + 8]
    mov qword [rdi + 0*8], 0x1FFFF500
    mov qword [rdi + 1*8], 0x1FFFF500 + 16
    mov qword [rdi + 2*8], 0x1FFFF500 + 32
    mov qword [rdi + 3*8], 0x1FFFF500 + 48
    mov qword [rdi + 4*8], 0x1FFFF500 + 80
    mov qword [rdi + 5*8], 0
    lea rdi, [rdi + 6*8]
    jmp .args_done

.fallback_args:
    ; Fallback: 1 argument (process name string at virtual 0x1FFFFF80)
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

    ; Stack layout at phys_stack + 0x1FFE00 (virtual 0x1FFFFE00):
    ; [0x00] = 1 (argc)
    ; [0x08] = 0x1FFFFF80 (argv[0])
    ; [0x10] = 0 (argv[1] = NULL)
    ; [0x18..] = envp
    lea rdi, [r10 + 0x1FFE00]
    mov qword [rdi + 0x00], 1
    mov qword [rdi + 0x08], 0x1FFFFF80
    mov qword [rdi + 0x10], 0

    ; Setup standard POSIX environment strings at phys_stack + 0x1FF500 (virtual 0x1FFFF500)
    lea rdi, [r10 + 0x1FF500]
    mov dword [rdi + 0], "PATH"
    mov dword [rdi + 4], "=/bi"
    mov dword [rdi + 8], "n:/"
    mov dword [rdi + 16], "HOME"
    mov word  [rdi + 20], "=/"
    mov byte  [rdi + 22], 0
    mov dword [rdi + 32], "USER"
    mov dword [rdi + 36], "=roo"
    mov word  [rdi + 40], "t"
    mov dword [rdi + 48], "TERM"
    mov dword [rdi + 52], "=xte"
    mov dword [rdi + 56], "rm-2"
    mov dword [rdi + 60], "56co"
    mov dword [rdi + 64], "lor"
    mov dword [rdi + 80], "SHEL"
    mov dword [rdi + 84], "L=/s"
    mov word  [rdi + 88], "h"

    lea rdi, [r10 + 0x1FFE00 + 0x18]
    mov qword [rdi + 0*8], 0x1FFFF500
    mov qword [rdi + 1*8], 0x1FFFF500 + 16
    mov qword [rdi + 2*8], 0x1FFFF500 + 32
    mov qword [rdi + 3*8], 0x1FFFF500 + 48
    mov qword [rdi + 4*8], 0x1FFFF500 + 80
    mov qword [rdi + 5*8], 0
    lea rdi, [rdi + 6*8]              ; RDI points after NULL envp

.args_done:
    ; Setup AMD64 System V / Linux Auxiliary Vector (auxv) at RDI:
    ; [0] AT_PHDR (3) = 0x400000 + phoff
    mov qword [rdi + 0*8], 3
    mov rax, [r12 + EHDR_OFF_PHOFF]
    add rax, 0x400000
    mov [rdi + 1*8], rax
    ; [1] AT_PHENT (4) = 56
    mov qword [rdi + 2*8], 4
    mov qword [rdi + 3*8], 56
    ; [2] AT_PHNUM (5) = phnum from ELF header
    mov qword [rdi + 4*8], 5
    movzx rax, word [r12 + EHDR_OFF_PHNUM]
    mov [rdi + 5*8], rax
    ; [3] AT_PAGESZ (6) = 4096
    mov qword [rdi + 6*8], 6
    mov qword [rdi + 7*8], 4096
    ; [4] AT_BASE (7) = interp_base (or 0 if static)
    mov qword [rdi + 8*8], 7
    xor eax, eax
    cmp qword [r15 + elf_has_interp - kmain], 0
    je @f
    mov rax, [r15 + elf_interp_base - kmain]
@@:
    mov [rdi + 9*8], rax
    ; [5] AT_FLAGS (8) = 0
    mov qword [rdi + 10*8], 8
    mov qword [rdi + 11*8], 0
    ; [6] AT_ENTRY (9) = entry RIP of executable
    mov qword [rdi + 12*8], 9
    mov [rdi + 13*8], r8
    ; [7] AT_UID (11) = 0
    mov qword [rdi + 14*8], 11
    mov qword [rdi + 15*8], 0
    ; [8] AT_EUID (12) = 0
    mov qword [rdi + 16*8], 12
    mov qword [rdi + 17*8], 0
    ; [9] AT_GID (13) = 0
    mov qword [rdi + 18*8], 13
    mov qword [rdi + 19*8], 0
    ; [10] AT_EGID (14) = 0
    mov qword [rdi + 20*8], 14
    mov qword [rdi + 21*8], 0
    ; [11] AT_CLKTCK (17) = 100
    mov qword [rdi + 22*8], 17
    mov qword [rdi + 23*8], 100
    ; [12] AT_SECURE (23) = 0
    mov qword [rdi + 24*8], 23
    mov qword [rdi + 25*8], 0
    ; [13] AT_RANDOM (25) = pointer to 16 random bytes (0x1FFFF500)
    mov qword [rdi + 26*8], 25
    mov qword [rdi + 27*8], 0x1FFFF500
    ; [14] AT_NULL (0) = 0
    mov qword [rdi + 28*8], 0
    mov qword [rdi + 29*8], 0

    mov qword [r15 + sys_spawn_argv_ptr - kmain], 0

    ; 10. Detect process ABI BEFORE finalize: task_finalize_user marks the
    ; task READY (and does vfs fd I/O) before returning, so on SMP the task
    ; may already run its first syscalls by the time we return here. The
    ; result is staged in sys_spawn_abi and picked up by task_finalize_user
    ; while it fills the TCB (setting TCB_ABI_TYPE afterwards raced: the
    ; first linux syscalls fell into the native dispatcher).
if defined CONFIG_LINUX_COMPAT
    push rdi
    push rsi
    push rax
    mov qword [r15 + sys_spawn_abi - kmain], 0
    cmp qword [r15 + elf_has_interp - kmain], 0
    jne .abi_is_linux

    ; Content scan first: GNU ld (Linux toolchains) always emits PT_GNU_STACK,
    ; PE-derived ELFs (our native apps / mingw-built test bins) never do — so
    ; arbitrary Debian binaries classify correctly regardless of file name.
    mov rsi, [r12 + EHDR_OFF_PHOFF]
    movzx ecx, word [r12 + EHDR_OFF_PHNUM]
    movzx eax, word [r12 + EHDR_OFF_PHENTSIZE]
.abi_ph_loop:
    test ecx, ecx
    jz .abi_name_check
    cmp dword [r12 + rsi + PHDR_OFF_TYPE], 0x6474E551  ; PT_GNU_STACK
    je .abi_is_linux
    add rsi, rax
    dec ecx
    jmp .abi_ph_loop
.abi_name_check:
    mov rsi, r14
    mov rdi, r14                     ; candidate basename start
.abi_scan:
    mov al, [rsi]
    test al, al
    jz .abi_scan_done
    inc rsi
    cmp al, '/'
    jne .abi_scan
    mov rdi, rsi                     ; restart basename after the last slash
    jmp .abi_scan
.abi_scan_done:
    cmp dword [rdi], 'linu'          ; basename starts with "linu"?
    je .abi_is_linux
    cmp dword [rdi], 'busy'          ; basename starts with "busy"?
    jne .abi_not_linux
.abi_is_linux:
    mov qword [r15 + sys_spawn_abi - kmain], 1
.abi_not_linux:
    push rsi
    lea rsi, [r15 + str_elf_abi_log - kmain]
    call puts
    mov rax, [r15 + sys_spawn_abi - kmain]
    call putdec64
    mov al, 10
    call putc
    pop rsi
    pop rax
    pop rsi
    pop rdi
end if

    ; If PT_INTERP requested, load dynamic linker and switch entry RIP
    call elf_load_interp              ; EBX = slot ID, R8 = entry RIP -> RAX = effective entry RIP
    mov r8, rax

    ; 11. Finalize Ring 3 User Task!
    ; EBX = slot ID
    ; RSI = process name (r14)
    ; RDX = entry point (r8)
    ; R8  = user stack top (0x1FFFFE00 = start of the argv/envp/auxv vector)
    mov rsi, r14
    mov rdx, r8                       ; entry RIP
    mov qword [r15 + sys_spawn_exec_entry - kmain], r8 ; for execve frame patch
    mov r8,  0x1FFFFE00               ; user RSP
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
; NOTE: the candidate pointer is tracked in RCX, not RAX: lodsb overwrites
; AL on every character, which used to leave the returned pointer with a
; zeroed low byte (address with bits 0..7 = 0) instead of the real basename
; address — process names and the linux ABI detection read garbage there.
; ==============================================================================
elf_get_basename:
    push rsi
    push rcx
    mov rcx, rsi                      ; candidate: start of path
.gb_loop:
    lodsb
    test al, al
    jz .gb_done
    cmp al, '/'
    jne .gb_loop
    mov rcx, rsi                      ; candidate: character after '/'
    jmp .gb_loop
.gb_done:
    mov rax, rcx
    pop rcx
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

; ==============================================================================
; elf_load_interp: Load dynamic linker (ld-linux.so) if PT_INTERP was requested
; EBX = task slot ID
; R8  = main binary entry RIP
; Returns: RAX = entry point to execute (interp entry if dynamic, or original R8)
; ==============================================================================
elf_load_interp:
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

    cmp qword [r15 + elf_has_interp - kmain], 0
    jne @f
    mov rax, r8                       ; Static binary: keep original entry RIP
    jmp .li_ret
@@:
    ; Force Linux ABI for dynamic binaries
    mov qword [r15 + sys_spawn_abi - kmain], 1

    ; 1. Try finding interpreter by requested path
    lea rsi, [r15 + elf_interp_path - kmain]
    call ext4_lookup
    test eax, eax
    jnz .li_have_inode

    ; Fallback: look in root directory (/ld-linux-x86-64.so.2)
    lea rsi, [r15 + str_interp_fallback - kmain]
    call ext4_lookup
    test eax, eax
    jnz .li_have_inode

    ; Interpreter not found!
    lea rsi, [r15 + str_elf_err_interp - kmain]
    call puts
    mov rax, r8                       ; Fallback to main binary
    jmp .li_ret

.li_have_inode:
    call ext4_inode_load
    jc .li_fail

    mov r12d, dword [r15 + ext4_inode - kmain + IN_OFF_SIZE]
    cmp r12, 64
    jb .li_fail

    ; 2. Allocate buffer from heap to read ld-linux
    mov rcx, r12
    call kmalloc
    test rax, rax
    jz .li_fail
    mov r13, rax                      ; r13 = interp buffer

    ; 3. Read file into buffer
    lea rsi, [r15 + elf_interp_path - kmain]
    push rax
    call ext4_lookup
    test eax, eax
    jnz @f
    lea rsi, [r15 + str_interp_fallback - kmain]
@@:
    pop rax
    mov rdi, r13
    mov rcx, r12
    call ext4_read_file_to_mem
    jc .li_free_fail

    ; 4. Validate ELF header of interpreter
    cmp dword [r13 + EHDR_OFF_IDENT], ELF_MAGIC
    jne .li_free_fail
    cmp byte [r13 + EHDR_OFF_IDENT + 4], ELFCLASS64
    jne .li_free_fail

    ; Compute interpreter entry RIP: interp_base + e_entry
    mov rax, [r13 + EHDR_OFF_ENTRY]
    add rax, [r15 + elf_interp_base - kmain]
    mov [r15 + elf_interp_entry - kmain], rax

    ; 5. Calculate physical destination base for interpreter
    ; Slot code memory is 4MB at phys_code.
    ; Interp base is 0x600000 -> phys_dest = phys_code + 0x200000
    lea eax, [ebx - 1]
    imul eax, USER_MEM_STRIDE
    add eax, USER_PHYS_BASE + 0x00200000
    mov r14d, eax                     ; r14 = physical base for interpreter

    ; 6. Iterate and load PT_LOAD segments of the interpreter
    movzx r10d, word [r13 + EHDR_OFF_PHNUM]     ; phnum
    movzx ebp, word [r13 + EHDR_OFF_PHENTSIZE]  ; phentsize
    mov r9, [r13 + EHDR_OFF_PHOFF]              ; phoff
    xor ecx, ecx                                ; ph index

.li_phdr_loop:
    cmp ecx, r10d
    jae .li_load_done

    mov rax, rcx
    imul rax, rbp
    add rax, r9
    lea rdx, [r13 + rax]              ; rdx = phdr ptr

    cmp dword [rdx + PHDR_OFF_TYPE], PT_LOAD
    jne .li_next_phdr

    ; Segment details:
    mov rsi, [rdx + PHDR_OFF_OFFSET]  ; file offset
    mov rdi, [rdx + PHDR_OFF_VADDR]   ; virtual address offset (from 0)
    mov r8,  [rdx + PHDR_OFF_FILESZ]  ; filesz
    mov r11, [rdx + PHDR_OFF_MEMSZ]   ; memsz

    ; Copy segment data into task's physical memory
    push rcx
    push rdi
    lea rdi, [r14 + rdi]              ; target physical address
    lea rsi, [r13 + rsi]              ; source buffer
    mov rcx, r8                       ; filesz
    rep movsb

    ; Clear BSS if memsz > filesz
    cmp r11, r8
    jbe @f
    mov rcx, r11
    sub rcx, r8                       ; BSS size
    xor eax, eax
    rep stosb
@@:
    pop rdi
    pop rcx

.li_next_phdr:
    inc ecx
    jmp .li_phdr_loop


.li_load_done:
    ; Advance linux_mmap_next to 0x20100000 (beyond interpreter)
    mov qword [r15 + linux_mmap_next - kmain], 0x20100000

    lea rsi, [r15 + str_elf_interp_ok - kmain]
    call puts

    mov rdi, r13
    call kfree

    mov rax, [r15 + elf_interp_entry - kmain]
    jmp .li_ret

.li_free_fail:
    mov rdi, r13
    call kfree
.li_fail:
    lea rsi, [r15 + str_elf_err_interp - kmain]
    call puts
    mov rax, [rsp + 56]               ; original r8 (main binary entry)

.li_ret:
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

