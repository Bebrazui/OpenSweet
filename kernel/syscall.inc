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
SYS_READ_FILE      = 20
SYS_UNLINK         = 21
SYS_GET_TIME       = 22
SYS_SYSINFO        = 23
SYS_OPEN           = 24
SYS_CLOSE          = 25
SYS_LSEEK          = 26
SYS_PIPE           = 27
SYS_DUP2           = 28
SYS_WAITPID        = 29
SYS_GETCWD         = 30
SYS_CHDIR          = 31
SYS_SPAWN_STDIO    = 32
SYS_MKDIR          = 33
SYS_RMDIR          = 34

align 16
sys_user_rsp       dq 0
sys_kernel_stack   dq 0
sys_spawn_arg_buf  rb 128                ; Global buffer for launch argument/file path
sys_spawn_argv_ptr dq 0                  ; Global pointer to user argv array (or 0)
sys_spawn_abi    dq 0                    ; Pending TCB_ABI_TYPE for next finalize (0/1)
sys_spawn_exec_mode dq 0                 ; !=0: elf_load_exec targets the RUNNING slot (execve)
sys_spawn_exec_entry dq 0                ; (out) entry point of the image loaded in exec mode
exec_path_buf    rb 64                   ; execve: staged pathname (kernel memory)
exec_argv_stage  rb 16 * 128             ; execve: staged argv strings
exec_argv_ptrs   dq 16                   ; execve: staged argv pointer table
sys_spawn_in_fd    dd -1                 ; Redirected stdin fd for child (-1 = default console)
sys_spawn_out_fd   dd -1                 ; Redirected stdout fd for child (-1 = default console)
sys_spawn_err_fd   dd -1                 ; Redirected stderr fd for child (-1 = default console)

align 16
; Dedicated per-task kernel syscall stacks (8 tasks * 8192 bytes = 64KB)
sys_kstack_pool:   rb (8 * 8192)
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

    ; Set initial dedicated syscall kernel stack for Task 0
    lea rax, [sys_kstack_pool + 8192]
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

    ; Check process ABI type (Native vs Linux)
if defined CONFIG_LINUX_COMPAT
    push rax
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rax]
    cmp dword [rbx + TCB_ABI_TYPE], 1
    pop rax
    je .do_linux_abi
end if

    ; 5. Dispatch native syscall by number in RAX
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
    cmp eax, SYS_READ_FILE
    je .do_read_file
    cmp eax, SYS_UNLINK
    je .do_unlink
    cmp eax, SYS_GET_TIME
    je .do_get_time
    cmp eax, SYS_SYSINFO
    je .do_sysinfo
    cmp eax, SYS_OPEN
    je .do_open
    cmp eax, SYS_CLOSE
    je .do_close
    cmp eax, SYS_LSEEK
    je .do_lseek
    cmp eax, SYS_PIPE
    je .do_pipe
    cmp eax, SYS_DUP2
    je .do_dup2
    cmp eax, SYS_WAITPID
    je .do_waitpid
    cmp eax, SYS_GETCWD
    je .do_getcwd
    cmp eax, SYS_CHDIR
    je .do_chdir
    cmp eax, SYS_SPAWN_STDIO
    je .do_spawn_stdio
    cmp eax, SYS_MKDIR
    je .do_mkdir
    cmp eax, SYS_RMDIR
    je .do_rmdir

if defined CONFIG_LINUX_COMPAT
    ; Fallback to Linux ABI for undeclared syscall numbers (marked distinctly
    ; so traces show which calls bypassed the ABI check)
    push rax
    push rsi
    lea rsi, [r15 + str_l_fb_log - kmain]
    call puts
    mov rax, [rsp + 8]
    call putdec64
    mov al, 10
    call putc
    pop rsi
    pop rax
    call linux_syscall_dispatch
    jmp .syscall_ret

.do_linux_abi:
    call linux_syscall_dispatch
    jmp .syscall_ret
else
    ; Unimplemented syscall
    mov rax, -1
    jmp .syscall_ret
end if

.do_exit:
    ; RDI = exit code
    call task_exit
    ; task_exit will not return

.do_write:
    ; RDI = fd, RSI = buffer, RDX = count
    call vfs_write_fd
    jmp .syscall_ret

.do_read:
    ; RDI = fd, RSI = buffer, RDX = count
    call vfs_read_fd
    jmp .syscall_ret

.do_open:
    ; RDI = path, RSI = flags, RDX = mode
    call vfs_open
    jmp .syscall_ret

.do_close:
    ; RDI = fd
    call vfs_close
    jmp .syscall_ret

.do_lseek:
    ; RDI = fd, RSI = offset, RDX = whence
    call vfs_lseek
    jmp .syscall_ret

.do_pipe:
    ; RDI = user pointer to int pipefd[2]
    call vfs_pipe
    jmp .syscall_ret

.do_dup2:
    ; RDI = oldfd, RSI = newfd
    call vfs_dup2
    jmp .syscall_ret

.do_waitpid:
    ; RDI = pid, RSI = status_ptr, RDX = options
    call sys_handler_waitpid
    jmp .syscall_ret

.do_getcwd:
    ; RDI = user buffer, RSI = buffer size
    call sys_handler_getcwd
    jmp .syscall_ret

.do_chdir:
    ; RDI = user path pointer
    call sys_handler_chdir
    jmp .syscall_ret

.do_spawn_stdio:
    ; RDI = path, RSI = argv, RDX = in_fd, R10 = out_fd, R8 = err_fd
    pushfq
    cli
    call sys_handler_spawn_stdio
    popfq
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
    pushfq
    cli
    call sys_handler_listdir
    popfq
    jmp .syscall_ret

.do_spawn:
    ; RDI = user path pointer
    pushfq
    cli
    call sys_handler_spawn
    popfq
    jmp .syscall_ret

.do_brk:
    ; RDI = new brk address (or 0 to query current brk)
    call sys_handler_brk
    jmp .syscall_ret

.do_write_file:
    ; RDI = path, RSI = buffer, RDX = count
    pushfq
    cli
    call sys_handler_write_file
    popfq
    jmp .syscall_ret

.do_read_file:
    ; RDI = path, RSI = buffer, RDX = max_count
    pushfq
    cli
    call sys_handler_read_file
    popfq
    jmp .syscall_ret

.do_unlink:
    ; RDI = path
    pushfq
    cli
    call sys_handler_unlink
    popfq
    jmp .syscall_ret

.do_mkdir:
    ; RDI = path
    pushfq
    cli
    call sys_handler_mkdir
    popfq
    jmp .syscall_ret

.do_rmdir:
    ; RDI = path
    pushfq
    cli
    call sys_handler_rmdir
    popfq
    jmp .syscall_ret

.do_get_time:
    ; Read CMOS RTC Real Time Clock with timeout guard
    mov ecx, 100000
.wait_rtc:
    mov al, 0x0A
    out 0x70, al
    in al, 0x71
    test al, 0x80
    jz .rtc_ready
    dec ecx
    jnz .wait_rtc

.rtc_ready:
    ; Read seconds (reg 0)
    xor al, al
    out 0x70, al
    in al, 0x71
    mov bl, al

    ; Read minutes (reg 2)
    mov al, 0x02
    out 0x70, al
    in al, 0x71
    mov bh, al

    ; Read hours (reg 4)
    mov al, 0x04
    out 0x70, al
    in al, 0x71
    mov cl, al

    ; Read status register B (reg 0x0B) to check BCD vs Binary
    mov al, 0x0B
    out 0x70, al
    in al, 0x71
    test al, 0x04               ; Bit 2: 1 = binary, 0 = BCD
    jnz .rtc_binary

    ; Convert BCD to binary
    movzx eax, bl
    mov edx, eax
    and edx, 0x0F
    shr eax, 4
    imul eax, 10
    add edx, eax
    mov bl, dl

    movzx eax, bh
    mov edx, eax
    and edx, 0x0F
    shr eax, 4
    imul eax, 10
    add edx, eax
    mov bh, dl

    movzx eax, cl
    mov edx, eax
    and edx, 0x0F
    shr eax, 4
    imul eax, 10
    add edx, eax
    mov cl, dl

.rtc_binary:
    ; Pack into RAX: (hours << 16) | (minutes << 8) | seconds
    movzx eax, cl
    shl eax, 8
    movzx edx, bh
    or eax, edx
    shl eax, 8
    movzx edx, bl
    or eax, edx
    jmp .syscall_ret

.do_sysinfo:
    ; RDI = user buffer (os_sysinfo_t)
    mov rsi, 160
    call sys_validate_user_ptr
    jc .sysinfo_err
    jmp .sysinfo_valid

.sysinfo_err:
    mov rax, -1
    jmp .syscall_ret

.sysinfo_valid:
    push rdi
    call count_free
    pop rdi

    mov dword [rdi], 256
    mov eax, r8d
    shr eax, 8
    mov dword [rdi + 4], eax
    mov ecx, 256
    sub ecx, eax
    mov dword [rdi + 8], ecx
    mov dword [rdi + 12], 65536
    mov dword [rdi + 16], r8d
    mov dword [rdi + 20], 1920
    mov dword [rdi + 24], 1080
    mov dword [rdi + 28], 32
    mov eax, [r15 + sched_task_cnt - kmain]
    mov dword [rdi + 32], eax

    push rdi
    add rdi, 36
    mov ecx, (160 - 36) / 4
    xor eax, eax
    rep stosd
    pop rdi

    mov dword [rdi + 36], '0.0.'
    mov dword [rdi + 40], '4-sm'
    mov dword [rdi + 44], 'p x8'
    mov dword [rdi + 48], '6_64'

    mov dword [rdi + 68], 'Open'
    mov dword [rdi + 72], 'Swee'
    mov dword [rdi + 76], 't OS'

    mov dword [rdi + 100], 'Acry'
    mov dword [rdi + 104], 'lic '
    mov dword [rdi + 108], 'R2D'

    xor eax, eax
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

    ; Range A: slot code/data/heap [0x400000, 0x04000000)
    cmp rdi, 0x00400000
    jb .check_stack
    cmp rax, 0x04000000               ; must END <= 64MB
    jbe .valid

.check_stack:
    ; Range C: per-task stack slot [0x1FE00000, 0x20000000)
    ; (each task maps this to its own private stack page; the guard is the
    ; page table itself - nothing else lives at that VA)
    cmp rdi, 0x1FE00000
    jb .check_mmap
    cmp rax, 0x20000000               ; must END <= 512MB (mmap zone starts here)
    jbe .valid

.check_mmap:
    ; Range B: Linux mmap zone [0x20000000, 0x40000000)
    cmp rdi, 0x20000000
    jb .invalid
    cmp rax, 0x40000000               ; must END <= 1GB
    ja .invalid

.valid:
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
    test ah, ah
    jz .sr_done
    cmp ah, 4                 ; only return characters on KEY_DOWN
    jne .sr_loop              ; skip KEY_UP
    test al, al
    jz .sr_loop
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
; RSI = user argv pointer (or NULL)
; Returns: RAX = Task ID (or negative error code)
; ==============================================================================
sys_handler_spawn:
    mov dword [r15 + sys_spawn_in_fd - kmain], -1
    mov dword [r15 + sys_spawn_out_fd - kmain], -1
    mov dword [r15 + sys_spawn_err_fd - kmain], -1
    jmp sys_handler_spawn_core

; ==============================================================================
; sys_handler_spawn_stdio: Spawn ELF with argv and custom stdin/stdout/stderr
; RDI = path, RSI = argv, RDX = in_fd, R10 = out_fd, R8 = err_fd
; ==============================================================================
sys_handler_spawn_stdio:
    mov [r15 + sys_spawn_in_fd - kmain], edx
    mov [r15 + sys_spawn_out_fd - kmain], r10d
    mov [r15 + sys_spawn_err_fd - kmain], r8d

sys_handler_spawn_core:
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

    ; Validate path pointer
    push rsi
    mov rsi, 64
    call sys_validate_user_ptr
    pop rsi
    jc .sp_err

    ; Store user argv pointer globally for elf_load_exec
    mov [r15 + sys_spawn_argv_ptr - kmain], rsi

    ; Copy string into local kernel buffer on stack and global spawn arg
    mov rsi, rdi
    lea rdi, [rsp]
    lea rbx, [r15 + sys_spawn_arg_buf - kmain]
    mov ecx, 63
.sp_cp:
    lodsb
    stosb
    mov [rbx], al
    inc rbx
    test al, al
    jz .sp_cp_done
    dec ecx
    jnz .sp_cp
    mov byte [rdi], 0
    mov byte [rbx], 0
.sp_cp_done:

    ; Launch ELF executable into Ring 3
    lea rsi, [rsp]
    call elf_load_from_ext4

    ; Reset redirection fds and argv pointer
    mov qword [r15 + sys_spawn_argv_ptr - kmain], 0
    mov dword [r15 + sys_spawn_in_fd - kmain], -1
    mov dword [r15 + sys_spawn_out_fd - kmain], -1
    mov dword [r15 + sys_spawn_err_fd - kmain], -1

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
    ; Reset redirection fds and argv pointer
    mov qword [r15 + sys_spawn_argv_ptr - kmain], 0
    mov dword [r15 + sys_spawn_in_fd - kmain], -1
    mov dword [r15 + sys_spawn_out_fd - kmain], -1
    mov dword [r15 + sys_spawn_err_fd - kmain], -1

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
; sys_handler_getcwd: Get current working directory path
; RDI = user buffer, RSI = buffer size
; Returns: RAX = user buffer pointer (or 0 on error)
; ==============================================================================
sys_handler_getcwd:
    push rbx
    push rcx
    push rsi
    push rdi

    test rdi, rdi
    jz .gc_err
    test rsi, rsi
    jz .gc_err

    ; Validate user buffer
    call sys_validate_user_ptr
    jc .gc_err

    ; Copy ext4_cwd_path into user buffer
    lea rsi, [r15 + ext4_cwd_path - kmain]
    mov rcx, [rsp + 8]                  ; original buffer size
    dec rcx                             ; keep space for null
.gc_copy:
    test rcx, rcx
    jz .gc_nul
    lodsb
    stosb
    test al, al
    jz .gc_done
    dec rcx
    jmp .gc_copy
.gc_nul:
    mov byte [rdi], 0
.gc_done:
    pop rdi
    pop rsi
    pop rcx
    pop rbx
    mov rax, rdi                        ; return user buffer pointer
    ret

.gc_err:
    pop rdi
    pop rsi
    pop rcx
    pop rbx
    xor eax, eax                        ; return NULL (0)
    ret

; ==============================================================================
; sys_handler_chdir: Change current working directory
; RDI = user path pointer
; Returns: RAX = 0 on success, -1 on error
; ==============================================================================
sys_handler_chdir:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    sub rsp, 64

    test rdi, rdi
    jz .cd_err

    ; Validate user pointer
    mov rsi, 64
    call sys_validate_user_ptr
    jc .cd_err

    ; Copy path into kernel buffer on stack
    mov rsi, rdi
    lea rdi, [rsp]
    mov ecx, 63
.cd_cp:
    lodsb
    stosb
    test al, al
    jz .cd_cp_done
    dec ecx
    jnz .cd_cp
    mov byte [rdi], 0
.cd_cp_done:

    lea rsi, [rsp]
    pushfq
    cli
    call ext4_cd
    ; popfq restores RFLAGS and destroys the CF set by ext4_cd, so capture
    ; the carry flag first: chdir always reported success before this fix.
    setc bl
    popfq
    test bl, bl
    jnz .cd_err

    xor eax, eax                        ; success = 0
    add rsp, 64
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

.cd_err:
    add rsp, 64
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    mov rax, -1
    ret

; ==============================================================================
; sys_handler_waitpid: Wait for child process state change (POSIX waitpid)
; RDI = target child PID (or -1 for any child)
; RSI = status pointer in user memory (or NULL)
; RDX = options (bit 0: WNOHANG = 1)
; Returns: RAX = reaped child PID, 0 if WNOHANG and child still alive, or -10 on error
; ==============================================================================
sys_handler_waitpid:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r12
    push r13
    push r14
    push r15

    mov r12, rdi                        ; r12 = target PID
    mov r13, rsi                        ; r13 = status pointer
    mov r14, rdx                        ; r14 = options

.wait_loop:
    ; Get caller task ID
    mov ebp, [r15 + sched_curr_task - kmain]

    ; Case 1: Waiting for specific child (r12 > 0)
    cmp r12, 0
    jle .wait_any_child

    ; Validate r12 < MAX_TASKS
    cmp r12, MAX_TASKS
    jae .wait_echild

    imul eax, r12d, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rax]

    ; Check if task slot is active
    mov ecx, [rbx + TCB_STATE]
    cmp ecx, TASK_UNUSED
    je .wait_echild

    ; Check if caller is parent (or shell task 0)
    test ebp, ebp
    jz @f
    cmp [rbx + TCB_PARENT_ID], ebp
    jne .wait_echild
@@:
    ; Is child dead?
    cmp ecx, TASK_DEAD
    je .reap_specific_child

    ; Child is still running!
    test r14, 1                         ; WNOHANG?
    jnz .wait_nohang_zero

    ; Block caller and sleep for 10ms to yield CPU to child
    mov ecx, 10
    call task_sleep
    jmp .wait_loop

.reap_specific_child:
    mov ebx, r12d
    jmp .do_reap

.wait_any_child:
    ; Scan slots 1..MAX_TASKS-1 for caller's children
    xor ebx, ebx
    xor esi, esi                        ; count of active children found
    mov edx, 1                          ; slot index
.scan_children:
    cmp edx, MAX_TASKS
    jae .scan_done

    imul eax, edx, TCB_SIZE
    lea rdi, [r15 + task_table - kmain + rax]

    mov ecx, [rdi + TCB_STATE]
    cmp ecx, TASK_UNUSED
    je .next_scan

    ; Check if caller is parent (or task 0)
    test ebp, ebp
    jz .is_child
    cmp [rdi + TCB_PARENT_ID], ebp
    jne .next_scan

.is_child:
    inc esi                             ; found active child!
    cmp ecx, TASK_DEAD
    jne .next_scan

    ; Found dead child to reap!
    mov ebx, edx                        ; ebx = child slot to reap
    jmp .do_reap

.next_scan:
    inc edx
    jmp .scan_children

.scan_done:
    ; If no children exist at all -> ECHILD (-10)
    test esi, esi
    jz .wait_echild

    ; Children exist, but none have exited yet
    test r14, 1                         ; WNOHANG?
    jnz .wait_nohang_zero

    ; Blocking wait: sleep 10ms and retry
    mov ecx, 10
    call task_sleep
    jmp .wait_loop

.do_reap:
    ; Child PID in EBX
    imul eax, ebx, TCB_SIZE
    lea rdi, [r15 + task_table - kmain + rax]

    ; Read exit code
    mov edx, [rdi + TCB_EXIT_CODE]
    and edx, 0xFF
    shl edx, 8                          ; POSIX WEXITSTATUS formatting

    ; Store to user status pointer if provided
    test r13, r13
    jz .reap_finish

    push rdi
    push rsi
    mov rdi, r13
    mov rsi, 4
    call sys_validate_user_ptr
    pop rsi
    pop rdi
    jc .reap_finish                     ; ignore bad pointer

    mov dword [r13], edx

.reap_finish:
    ; Mark child slot as TASK_UNUSED so it can be reused
    mov dword [rdi + TCB_STATE], TASK_UNUSED

    mov eax, ebx                        ; Return reaped child PID
    jmp .wait_ret

.wait_nohang_zero:
    xor eax, eax                        ; Return 0 (no child exited yet)
    jmp .wait_ret

.wait_echild:
    mov rax, -10                        ; -10 (ECHILD)

.wait_ret:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
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

    ; Validate requested address: must be >= 0x01000000 and < 0x04000000 (heap range 16MB..64MB, 48MB)
    cmp rdi, 0x01000000
    jb .brk_fail
    cmp rdi, 0x04000000
    jae .brk_fail

    ; Update current brk
    mov [rbx + TCB_HEAP_BRK], edi
    mov eax, edi
    jmp .brk_done

.brk_query:
    mov eax, [rbx + TCB_HEAP_BRK]
    test eax, eax
    jnz .brk_done
    ; Default initial brk if 0 (starts at 16MB, away from code/stack/canvases)
    mov eax, 0x01000000
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
    ; EAX = target file inode number
    mov rsi, r12                      ; rsi = user data buffer
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

; ==============================================================================
; sys_handler_read_file: Read file content directly from ext4 into user buffer
; RDI = user path pointer (e.g. "/hello.txt")
; RSI = user destination buffer
; RDX = max bytes to read
; Returns: RAX = bytes read, or -1 on error
; ==============================================================================
sys_handler_read_file:
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
    jz .rf_err
    test rsi, rsi
    jz .rf_err
    test rdx, rdx
    jz .rf_err

    mov r12, rsi                      ; user destination buffer
    mov r13, rdx                      ; max bytes

    ; Validate path pointer
    mov rsi, 64
    call sys_validate_user_ptr
    jc .rf_err

    ; Validate destination buffer
    mov rdi, r12
    mov rsi, r13
    call sys_validate_user_ptr
    jc .rf_err

    ; Copy path string to local stack buffer
    mov rsi, [rsp + 64 + 32]          ; original saved rdi
    lea rdi, [rsp]
    mov ecx, 63
.rf_cp_path:
    lodsb
    stosb
    test al, al
    jz .rf_cp_done
    dec ecx
    jnz .rf_cp_path
    mov byte [rdi], 0
.rf_cp_done:

    ; Skip leading slash '/' if present for ext4_lookup
    lea rsi, [rsp]
    cmp byte [rsi], '/'
    jne @f
    inc rsi
@@:
    mov rdi, r12                      ; user destination buffer
    mov rcx, r13                      ; max bytes
    call ext4_read_file_to_mem
    jc .rf_err

    ; RAX contains bytes read
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

.rf_err:
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
; sys_handler_unlink: Delete file from ext4 filesystem
; RDI = user path pointer (e.g. "test.txt" or "/test.txt")
; Returns: RAX = 0 on success, or -1 on error
; ==============================================================================
sys_handler_unlink:
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
    jz .ul_err

    ; Validate path pointer
    mov rsi, 64
    call sys_validate_user_ptr
    jc .ul_err

    ; Copy path string to stack buffer
    mov rsi, [rsp + 64 + 32]          ; saved rdi (original user path)
    lea rdi, [rsp]
    mov ecx, 63
.ul_cp_path:
    lodsb
    stosb
    test al, al
    jz .ul_cp_done
    dec ecx
    jnz .ul_cp_path
    mov byte [rdi], 0
.ul_cp_done:

    ; Skip leading slash '/' if present
    lea rsi, [rsp]
    cmp byte [rsi], '/'
    jne @f
    inc rsi
@@:
    call ext4_delete_file
    cmp rax, -1
    je .ul_err

    xor eax, eax                      ; return 0 on success
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

.ul_err:
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
; sys_handler_mkdir: Create directory in ext4 filesystem
; RDI = user path pointer
; Returns: RAX = 0 on success, or -1 on error
; ==============================================================================
sys_handler_mkdir:
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
    jz .md_err

    mov rsi, 64
    call sys_validate_user_ptr
    jc .md_err

    mov rsi, [rsp + 64 + 32]
    lea rdi, [rsp]
    mov ecx, 63
.md_cp_path:
    lodsb
    stosb
    test al, al
    jz .md_cp_done
    dec ecx
    jnz .md_cp_path
    mov byte [rdi], 0
.md_cp_done:

    lea rsi, [rsp]
    call ext4_mkdir
    cmp rax, -1
    je .md_err

    xor eax, eax
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

.md_err:
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
; sys_handler_rmdir: Remove empty directory from ext4 filesystem
; RDI = user path pointer
; Returns: RAX = 0 on success, or -1 on error
; ==============================================================================
sys_handler_rmdir:
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
    jz .rd_err

    mov rsi, 64
    call sys_validate_user_ptr
    jc .rd_err

    mov rsi, [rsp + 64 + 32]
    lea rdi, [rsp]
    mov ecx, 63
.rd_cp_path:
    lodsb
    stosb
    test al, al
    jz .rd_cp_done
    dec ecx
    jnz .rd_cp_path
    mov byte [rdi], 0
.rd_cp_done:

    lea rsi, [rsp]
    call ext4_rmdir
    cmp rax, -1
    je .rd_err

    xor eax, eax
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

.rd_err:
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


