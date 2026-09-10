; ==============================================================================
; Opensweet OS - Fast 64-bit System Call Subsystem (kernel/syscall.inc)
; Implements SYSCALL/SYSRET (IA32_STAR, IA32_LSTAR, IA32_FMASK, EFER.SCE).
; ==============================================================================

SYS_EXIT           = 0
SYS_WRITE          = 1
SYS_READ           = 2
SYS_SLEEP          = 3
SYS_YIELD          = 4
SYS_UPTIME         = 5
SYS_GUI_CREATE_WIN = 10
SYS_GUI_UPDATE_WIN = 11
SYS_GUI_POLL_EVENT = 12
SYS_GUI_CLOSE_WIN  = 13
SYS_APP_REGISTER   = 14
SYS_APP_INFO       = 15
SYS_LISTDIR        = 16
SYS_SPAWN          = 17
SYS_BRK            = 18
SYS_WRITE_FILE     = 19

align 16
sys_user_rsp       dq 0
sys_kernel_stack   dq 0

align 16
sys_kstack_pool:   rb 8192
sys_kstack_top:

str_sys_crash      db 10, "[Kernel] Ring 3 user task crashed. Terminating task.", 10, 0
str_sys_nosys      db "[Syscall] Unimplemented syscall: 0x", 0

; ==============================================================================
; syscall_init: Enable System Call Extensions and configure MSRs
; ==============================================================================
syscall_init:
    push rax
    push rcx
    push rdx

    ; Set initial dedicated syscall kernel stack
    lea rax, [sys_kstack_top]
    mov [sys_kernel_stack], rax

    ; 1. Enable SCE (System Call Enable, bit 0) in IA32_EFER (0xC0000080)
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1                         ; SCE = 1
    wrmsr

    ; 2. Configure IA32_STAR (0xC0000081):
    ; [47:32] = Kernel CS selector for SYSCALL (0x08). Kernel SS will be 0x08 + 8 = 0x10.
    ; [63:48] = User CS/SS base for SYSRET (0x1B).
    ; NOTE: AMD and Intel handle STAR[63:48] differently on SYSRET:
    ; Intel forces RPL=3 into CS and SS:
    ;   CS = (STAR[63:48] + 16) | 3
    ;   SS = (STAR[63:48] + 8)  | 3
    ; But AMD does NOT force RPL=3 into SS! It computes:
    ;   CS = (STAR[63:48] + 16) with RPL forced to 3
    ;   SS = STAR[63:48] + 8    (NO RPL force! Exact addition!)
    ; Therefore, STAR[63:48] MUST be set to 0x001B (0x18 | 3)!
    ; With 0x001B:
    ;   SS = 0x001B + 8  = 0x0023 (USER_DATA_SEL | 3) - valid RPL=3 on AMD and Intel!
    ;   CS = 0x001B + 16 = 0x002B (USER_CODE_SEL | 3) - valid RPL=3 on AMD and Intel!
    mov ecx, 0xC0000081
    xor eax, eax
    mov edx, (0x001B shl 16) or 0x0008
    wrmsr

    ; 3. Configure IA32_LSTAR (0xC0000082):
    ; Entry point virtual address for 64-bit SYSCALL
    mov ecx, 0xC0000082
    lea rax, [r15 + syscall_entry - kmain]
    mov rdx, rax
    shr rdx, 32
    wrmsr

    ; 4. Configure IA32_FMASK (0xC0000084):
    ; RFLAGS bits to clear when executing SYSCALL (mask out IF, TF, DF, AC)
    mov ecx, 0xC0000084
    mov eax, 0x257FD5                 ; Standard x86_64 RFLAGS mask (clears IF, TF, etc.)
    xor edx, edx
    wrmsr

    pop rdx
    pop rcx
    pop rax
    ret

; ==============================================================================
; syscall_entry: Fast entry point for 64-bit SYSCALL
; Hardware sets:
;   RCX = user RIP
;   R11 = user RFLAGS
;   CS  = 0x08, SS = 0x10 (Ring 0)
;   IF  = 0 (Interrupts disabled)
; Arguments in: RAX (num), RDI (arg1), RSI (arg2), RDX (arg3), R10 (arg4), R8 (arg5), R9 (arg6)
; ==============================================================================
align 16
syscall_entry:
    ; 1. Save user RSP and switch to kernel syscall stack
    mov [sys_user_rsp], rsp
    mov rsp, [sys_kernel_stack]

    ; 2. Preserve user context on kernel stack
    push qword [sys_user_rsp]               ; User RSP
    push rcx                                ; User RIP
    push r11                                ; User RFLAGS
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10

    ; 3. Establish kernel image base in R15
    mov r15, 0xFFFF800000010000

    ; 4. Re-enable interrupts for kernel syscall execution
    sti

    ; 5. Dispatch syscall by number in RAX
    cmp eax, SYS_EXIT
    je .do_exit
    cmp eax, SYS_WRITE
    je .do_write
    cmp eax, SYS_READ
    je .do_read
    cmp eax, SYS_SLEEP
    je .do_sleep
    cmp eax, SYS_YIELD
    je .do_yield
    cmp eax, SYS_UPTIME
    je .do_uptime
    cmp eax, SYS_GUI_CREATE_WIN
    je .do_gui_create_win
    cmp eax, SYS_GUI_UPDATE_WIN
    je .do_gui_update_win
    cmp eax, SYS_GUI_POLL_EVENT
    je .do_gui_poll_event
    cmp eax, SYS_GUI_CLOSE_WIN
    je .do_gui_close_win
    cmp eax, SYS_APP_REGISTER
    je .do_app_register
    cmp eax, SYS_LISTDIR
    je .do_listdir
    cmp eax, SYS_SPAWN
    je .do_spawn
    cmp eax, SYS_BRK
    je .do_brk
    cmp eax, SYS_WRITE_FILE
    je .do_write_file

    ; Unimplemented syscall
    mov rax, -1
    jmp .syscall_ret

.do_exit:
    ; RDI = exit code
    call task_exit
    ; task_exit will not return

.do_write:
    ; RDI = fd (1=stdout, 2=stderr)
    ; RSI = buffer (user address)
    ; RDX = count
    call sys_handler_write
    jmp .syscall_ret

.do_read:
    ; RDI = fd (0=stdin)
    ; RSI = buffer
    ; RDX = count
    call sys_handler_read
    jmp .syscall_ret

.do_sleep:
    ; RDI = milliseconds
    mov ecx, edi
    call task_sleep
    xor eax, eax
    jmp .syscall_ret

.do_yield:
    call task_yield
    xor eax, eax
    jmp .syscall_ret

.do_uptime:
    mov eax, [r15 + timer_ticks - kmain]
    jmp .syscall_ret

.do_gui_create_win:
    ; RDI = title, RSI = x, RDX = y, R10 = w, R8 = h
    call wm_create_user_window
    jmp .syscall_ret

.do_gui_update_win:
    ; RDI = win_id
    call wm_update_user_window
    jmp .syscall_ret

.do_gui_poll_event:
    ; RDI = win_id, RSI = user_event_ptr
    call wm_poll_user_event
    jmp .syscall_ret

.do_gui_close_win:
    ; RDI = win_id
    call wm_close_user_window
    jmp .syscall_ret

.do_app_register:
    ; RDI = user pointer to os_app_info_t, RSI = size
    call app_reg_install_from_user
    jmp .syscall_ret

.do_listdir:
    ; RDI = user buffer (os_dirent_t*), RSI = max_entries
    call sys_handler_listdir
    jmp .syscall_ret

.do_spawn:
    ; RDI = user path pointer
    call sys_handler_spawn
    jmp .syscall_ret

.do_brk:
    ; RDI = new brk address (or 0 to query current brk)
    call sys_handler_brk
    jmp .syscall_ret

.do_write_file:
    ; RDI = path, RSI = buffer, RDX = count
    call sys_handler_write_file
    jmp .syscall_ret

.syscall_ret:
    ; RAX = return code
    ; 6. Disable interrupts before restoring user stack and registers
    cli
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    pop r11                           ; User RFLAGS
    pop rcx                           ; User RIP
    pop rsp                           ; User RSP restored!

    ; 7. Return to Ring 3 (64-bit user mode)
    sysretq

; ==============================================================================
; sys_validate_user_ptr: Validate user pointer and range
; RDI = user buffer address
; RSI = buffer size in bytes
; Returns: CF = 0 if valid user space (< 0x0000800000000000), CF = 1 if invalid
; Preserves all registers except RAX
; ==============================================================================
sys_validate_user_ptr:
    test rdi, rdi
    jz .invalid
    test rsi, rsi
    jz .invalid
    mov rax, rdi
    add rax, rsi
    jc .invalid                       ; 64-bit integer overflow
    shr rax, 47
    jnz .invalid                      ; address >= 0x0000800000000000
    clc
    ret
.invalid:
    stc
    ret

; ==============================================================================
; sys_handler_write: Write bytes from user buffer to console & terminal
; RSI = buffer, RDX = count
; Returns: RAX = count written (or -1 on invalid pointer)
; ==============================================================================
sys_handler_write:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    test rdx, rdx
    jz .sw_done
    cmp rdx, 65536
    jbe @f
    mov rdx, 65536
@@:
    ; Validate user pointer
    mov rdi, rsi
    mov rsi, rdx
    call sys_validate_user_ptr
    jc .sw_err

    mov rsi, rdi                      ; restore rsi = user buffer
    mov rbx, [rsp + 16]               ; restore rbx = original count from saved rdx on stack
    cmp rbx, 65536
    jbe .sw_loop
    mov rbx, 65536
.sw_loop:
    test rbx, rbx
    jz .sw_done
    lodsb
    call putc                         ; Outputs to desktop terminal & serial!
    dec rbx
    jmp .sw_loop

.sw_done:
    pop rdi
    pop rsi
    pop rdx
    mov rax, rdx
    pop rcx
    pop rbx
    ret

.sw_err:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    mov rax, -1
    ret

; ==============================================================================
; sys_handler_read: Read available keyboard characters into user buffer
; RSI = buffer, RDX = max count
; Returns: RAX = count read (or -1 on invalid pointer)
; ==============================================================================
sys_handler_read:
    push rbx
    push rcx
    push rdx
    push rdi

    test rdx, rdx
    jz .sr_done
    cmp rdx, 65536
    jbe @f
    mov rdx, 65536
@@:
    ; Validate user pointer
    mov rdi, rsi
    mov rsi, rdx
    call sys_validate_user_ptr
    jc .sr_err

    mov rbx, [rsp + 8]                ; restore rbx = max count from saved rdx on stack
    cmp rbx, 65536
    jbe @f
    mov rbx, 65536
@@:
    xor ecx, ecx
.sr_loop:
    cmp rcx, rbx
    jae .sr_done
    call getc
    test al, al
    jz .sr_done
    stosb
    inc ecx
    jmp .sr_loop

.sr_done:
    mov rax, rcx
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    ret

.sr_err:
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    mov rax, -1
    ret

; ==============================================================================
; sys_handler_listdir: Read ext4 root directory entries into user buffer
; RDI = user buffer (os_dirent_t *), RSI = max_entries
; Returns: RAX = number of entries read, or -1 on error
; os_dirent_t layout (48 bytes):
;   +0:  inode (uint32_t)
;   +4:  type  (uint32_t) (1=file, 2=dir)
;   +8:  size  (uint64_t)
;   +16: name  (char[32])
; ==============================================================================
sys_handler_listdir:
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
    push rbp
    sub rsp, 32

    mov r14, rdi                      ; r14 = user buffer
    mov r13d, esi                     ; r13d = max_entries

    test r14, r14
    jz .ld_err
    test r13d, r13d
    jle .ld_err
    cmp r13d, 64
    jbe @f
    mov r13d, 64
@@:

    ; Validate user buffer
    mov rdi, r14
    imul rsi, r13, 48
    call sys_validate_user_ptr
    jc .ld_err

    ; Check if ext4 is mounted
    cmp byte [r15 + ext4_ok - kmain], 1
    jne .ld_err

    ; Load root directory inode (2)
    mov eax, 2
    call ext4_inode_load
    jc .ld_err

    mov r12, qword [r15 + ext4_inode - kmain + IN_OFF_SIZE] ; dir size
    xor ebp, ebp                      ; ebp = entries count returned
    mov dword [r15 + e4t_lb - kmain], 0

.ld_next_block:
    mov eax, dword [r15 + e4t_lb - kmain]
    mov ecx, dword [r15 + ext4_lbs - kmain]
    add ecx, 10
    shl rax, cl                       ; byte offset
    cmp rax, r12
    jae .ld_done

    mov eax, dword [r15 + e4t_lb - kmain]
    call ext4_map
    cmp rax, -1
    je .ld_done

    ; Read directory block into ext4_bbuf
    lea rsi, [r15 + ext4_bbuf - kmain]
    call ext4_blk_read
    jc .ld_done

    lea r8, [r15 + ext4_bbuf - kmain]
    mov ecx, dword [r15 + ext4_lbs - kmain]
    mov edx, 1024
    shl edx, cl
    mov r9, r8
    add r9, rdx                       ; block end

.ld_parse_ent:
    cmp r8, r9
    jae .ld_block_done

    mov eax, [r8]                     ; entry inode
    test eax, eax
    jz .ld_skip

    movzx ecx, byte [r8 + 6]          ; name length
    test ecx, ecx
    jz .ld_skip

    movzx ebx, byte [r8 + 7]          ; type: 1=file, 2=dir

    ; Skip "."
    cmp ecx, 1
    jne @f
    cmp byte [r8 + 8], '.'
    je .ld_skip
@@:
    ; Skip ".."
    cmp ecx, 2
    jne @f
    cmp word [r8 + 8], '..'
    je .ld_skip
@@:

    ; Store entry into user buffer at r14
    mov [r14 + 0], eax                ; inode
    mov [r14 + 4], ebx                ; type

    ; Copy name (up to 31 chars)
    lea rsi, [r8 + 8]
    lea rdi, [r14 + 16]
    cmp ecx, 31
    jbe @f
    mov ecx, 31
@@:
    push rcx
    rep movsb
    mov byte [rdi], 0
    pop rcx

    ; Load file size for entry
    mov eax, [r14 + 0]                ; file inode
    push r8
    push r9
    call ext4_inode_load
    jc .ld_sz_zero
    mov rax, qword [r15 + ext4_inode - kmain + IN_OFF_SIZE]
    mov [r14 + 8], rax
    jmp .ld_sz_ok
.ld_sz_zero:
    mov qword [r14 + 8], 0
.ld_sz_ok:
    pop r9
    pop r8

    add r14, 48                       ; next dirent slot
    inc ebp
    cmp ebp, r13d
    jae .ld_done

.ld_skip:
    movzx eax, word [r8 + 4]          ; rec_len
    test eax, eax
    jz .ld_block_done
    add r8, rax
    jmp .ld_parse_ent

.ld_block_done:
    inc dword [r15 + e4t_lb - kmain]
    jmp .ld_next_block

.ld_done:
    mov eax, ebp
    add rsp, 32
    pop rbp
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

.ld_err:
    add rsp, 32
    pop rbp
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
    mov rax, -1
    ret

; ==============================================================================
; sys_handler_spawn: Spawn an ELF executable from ext4 storage in Ring 3
; RDI = user path pointer (ASCIIZ, e.g. "/notepad.elf")
; Returns: RAX = Task ID (or negative error code)
; ==============================================================================
sys_handler_spawn:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 64

    test rdi, rdi
    jz .sp_err

    ; Validate user pointer
    mov rsi, 64
    call sys_validate_user_ptr
    jc .sp_err

    ; Copy string into local kernel buffer on stack
    mov rsi, rdi
    lea rdi, [rsp]
    mov ecx, 63
.sp_cp:
    lodsb
    stosb
    test al, al
    jz .sp_cp_done
    dec ecx
    jnz .sp_cp
    mov byte [rdi], 0
.sp_cp_done:

    ; Launch ELF executable into Ring 3
    lea rsi, [rsp]
    call elf_load_from_ext4

    add rsp, 64
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

.sp_err:
    add rsp, 64
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    mov rax, -1
    ret

; ==============================================================================
; sys_handler_brk: Change data segment size (heap brk) for current Ring 3 task
; RDI = requested new brk address (or 0 to query current brk)
; Returns: RAX = current/new brk address, or -1 on failure
; ==============================================================================
sys_handler_brk:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    ; Get current task TCB
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rax]

    ; Check if query (RDI == 0)
    test rdi, rdi
    jz .brk_query

    ; Validate requested address: must be >= 0x00500000 and < 0x00600000 (heap range)
    cmp rdi, 0x00500000
    jb .brk_fail
    cmp rdi, 0x00600000
    jae .brk_fail

    ; Update current brk
    mov [rbx + TCB_HEAP_BRK], edi
    mov eax, edi
    jmp .brk_done

.brk_query:
    mov eax, [rbx + TCB_HEAP_BRK]
    test eax, eax
    jnz .brk_done
    ; Default initial brk if 0
    mov eax, 0x00500000
    mov [rbx + TCB_HEAP_BRK], eax

.brk_done:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

.brk_fail:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    mov rax, -1
    ret

; ==============================================================================
; sys_handler_write_file: Create or write data to an ext4 file
; RDI = user path pointer (ASCIIZ, e.g. "/notes.txt")
; RSI = user buffer with file content
; RDX = byte count (up to 1024 for ext4 write block)
; Returns: RAX = bytes written, or -1 on error
; ==============================================================================
sys_handler_write_file:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 64

    test rdi, rdi
    jz .wf_err
    test rsi, rsi
    jz .wf_err

    mov r12, rsi                      ; r12 = user data buffer
    mov r13, rdx                      ; r13 = count

    ; Validate path pointer
    mov rsi, 64
    call sys_validate_user_ptr
    jc .wf_err

    ; Validate data buffer pointer
    mov rdi, r12
    mov rsi, r13
    call sys_validate_user_ptr
    jc .wf_err

    ; Copy path string to stack buffer
    mov rsi, [rsp + 64 + 32]          ; saved rdi (original user path)
    lea rdi, [rsp]
    mov ecx, 63
.wf_cp_path:
    lodsb
    stosb
    test al, al
    jz .wf_cp_done
    dec ecx
    jnz .wf_cp_path
    mov byte [rdi], 0
.wf_cp_done:

    ; Skip leading slash '/' if present
    lea rsi, [rsp]
    cmp byte [rsi], '/'
    jne @f
    inc rsi
@@:
    ; Check if file already exists in ext4
    call ext4_lookup
    test eax, eax
    jnz .wf_have_inode

    ; File does not exist, create it!
    call ext4_create_file
    cmp eax, -1
    je .wf_err

.wf_have_inode:
    ; EAX = inode number
    mov rdi, rax                      ; rdi = inode
    mov rax, rdi                      ; rax = inode
    mov rsi, r12                      ; rsi = user buffer
    mov rdx, r13                      ; rdx = count
    call ext4_write_file
    cmp rax, -1
    je .wf_err

    ; RAX contains bytes written
    add rsp, 64
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

.wf_err:
    add rsp, 64
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    mov rax, -1
    ret


