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

