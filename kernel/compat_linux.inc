; ==============================================================================
; Opensweet OS - Linux ABI Compatibility Layer (kernel/compat_linux.inc)
; Provides drop-in modular binary compatibility for Linux x86_64 executables.
; Can be fully toggled via CONFIG_LINUX_COMPAT.
; ==============================================================================

CONFIG_LINUX_COMPAT = 1

; Linux x86_64 Syscall Numbers
LINUX_SYS_READ        = 0
LINUX_SYS_WRITE       = 1
LINUX_SYS_PREAD64     = 17
LINUX_SYS_PWRITE64    = 18
LINUX_SYS_OPEN        = 2
LINUX_SYS_CLOSE       = 3
LINUX_SYS_STAT        = 4
LINUX_SYS_FSTAT       = 5
LINUX_SYS_LSTAT       = 6
LINUX_SYS_POLL        = 7
LINUX_SYS_LSEEK       = 8
LINUX_SYS_MMAP        = 9
LINUX_SYS_MPROTECT    = 10
LINUX_SYS_MUNMAP      = 11
LINUX_SYS_BRK         = 12
LINUX_SYS_IOCTL       = 16
LINUX_SYS_WRITEV      = 20
LINUX_SYS_ACCESS      = 21
LINUX_SYS_PIPE        = 22
LINUX_SYS_DUP         = 32
LINUX_SYS_DUP2        = 33
LINUX_SYS_NANOSLEEP   = 35
LINUX_SYS_GETPID      = 39
LINUX_SYS_CLONE       = 56
LINUX_SYS_FORK        = 57
LINUX_SYS_EXECVE      = 59
LINUX_SYS_EXIT        = 60
LINUX_SYS_WAIT4       = 61
LINUX_SYS_UNAME       = 63
LINUX_SYS_FCNTL       = 72
LINUX_SYS_GETCWD      = 79
LINUX_SYS_CHDIR       = 80
LINUX_SYS_MKDIR       = 83
LINUX_SYS_RMDIR       = 84
LINUX_SYS_UNLINK      = 87
LINUX_SYS_GETUID      = 102
LINUX_SYS_GETGID      = 104
LINUX_SYS_GETEUID     = 107
LINUX_SYS_GETEGID     = 108
LINUX_SYS_ARCH_PRCTL  = 158
LINUX_SYS_GETTID      = 186
LINUX_SYS_FUTEX       = 202
LINUX_SYS_SET_TID_ADDR= 218
LINUX_SYS_EXIT_GROUP  = 231
LINUX_SYS_OPENAT      = 257
LINUX_SYS_MKDIRAT     = 258
LINUX_SYS_UNLINKAT    = 263
LINUX_SYS_NEWFSTATAT   = 262
LINUX_SYS_GETDENTS64   = 217
LINUX_SYS_VFORK       = 58
LINUX_SYS_GETPPID     = 110
LINUX_SYS_RT_SIGACTION = 13
LINUX_SYS_RT_SIGPROCMASK = 14
LINUX_SYS_RT_SIGRETURN = 15
LINUX_SYS_PAUSE       = 34
LINUX_SYS_ALARM       = 37
LINUX_SYS_KILL        = 62
LINUX_SYS_SETPGID     = 109
LINUX_SYS_GETPGRP     = 111
LINUX_SYS_SETSID      = 112
LINUX_SYS_GETPGID     = 121
LINUX_SYS_GETSID      = 124
LINUX_SYS_TGKILL      = 234
LINUX_SYS_READV       = 19
LINUX_SYS_MADVISE     = 28
LINUX_SYS_SYSINFO     = 99
LINUX_SYS_FACCESSAT   = 269
LINUX_SYS_GETRUSAGE   = 98
LINUX_SYS_TIMES       = 100
LINUX_SYS_READLINK    = 89
LINUX_SYS_READLINKAT  = 267
LINUX_SYS_FLOCK       = 73
LINUX_SYS_FSYNC       = 74
LINUX_SYS_FDATASYNC   = 75
LINUX_SYS_TRUNCATE    = 76
LINUX_SYS_FTRUNCATE   = 77
LINUX_SYS_CHMOD       = 90
LINUX_SYS_FCHMOD      = 91
LINUX_SYS_CHOWN       = 92
LINUX_SYS_FCHOWN      = 93
LINUX_SYS_UMASK       = 95
LINUX_SYS_SETUID      = 105
LINUX_SYS_SETGID      = 106
LINUX_SYS_SETREUID    = 113
LINUX_SYS_SETREGID    = 114
LINUX_SYS_GETGROUPS   = 115
LINUX_SYS_SETGROUPS   = 116
LINUX_SYS_SETRESUID   = 117
LINUX_SYS_GETRESUID   = 118
LINUX_SYS_SETRESGID   = 119
LINUX_SYS_GETRESGID   = 120
LINUX_SYS_CHROOT      = 161
LINUX_SYS_FCHMODAT    = 268
LINUX_SYS_FCHOWNAT    = 260
LINUX_SYS_UTIMENSAT   = 280
LINUX_SYS_STATX       = 332

; Signal numbers (Linux x86_64 numbering)
LINUX_SIGINT   = 2
LINUX_SIGILL   = 4
LINUX_SIGTRAP  = 5
LINUX_SIGBUS   = 7
LINUX_SIGFPE   = 8
LINUX_SIGKILL  = 9
LINUX_SIGUSR1  = 10
LINUX_SIGSEGV  = 11
LINUX_SIGPIPE  = 13
LINUX_SIGALRM  = 14
LINUX_SIGTERM  = 15
LINUX_SIGCHLD  = 17
LINUX_SIGCONT  = 18
LINUX_SIGSTOP  = 19
LINUX_SIGTSTP  = 20
LINUX_SIGTTIN  = 21
LINUX_SIGTTOU  = 22
LINUX_SIGURG   = 23
LINUX_SIGWINCH = 28

; rt_sigaction sa_flags bits (the ones we understand)
SA_RESTORER    = 0x04000000           ; sa_restorer holds the sigreturn trampoline
SA_NODEFER     = 0x40000000           ; do not block the signal during its handler

LINUX_SYS_RENAME      = 82
LINUX_SYS_LINK        = 86
LINUX_SYS_SYMLINK     = 88
LINUX_SYS_LINKAT      = 265
LINUX_SYS_SYMLINKAT   = 266
LINUX_SYS_RENAMEAT    = 264
LINUX_SYS_RENAMEAT2   = 316

; Linux Error Codes (negative return values in RAX)
LINUX_EPERM   = -1
LINUX_ENOENT  = -2
LINUX_ESRCH   = -3
LINUX_EINTR   = -4
LINUX_EIO     = -5
LINUX_ENOEXEC = -8
LINUX_EBADF   = -9
LINUX_EAGAIN  = -11
LINUX_ENOMEM  = -12
LINUX_EACCES  = -13
LINUX_EFAULT  = -14
LINUX_EEXIST  = -17
LINUX_EXDEV   = -18
LINUX_ENODEV  = -19
LINUX_ENOTDIR = -20
LINUX_EISDIR  = -21
LINUX_EINVAL  = -22
LINUX_ENFILE  = -23
LINUX_EMFILE  = -24
LINUX_ENOTTY = -25
LINUX_EROFS   = -30
LINUX_EMLINK  = -31
LINUX_EPIPE   = -32
LINUX_ENOLCK  = -37
LINUX_ENOSYS  = -38
LINUX_ENOTEMPTY = -39
LINUX_ELOOP   = -40

; ==============================================================================
; Signal machinery (Linux ABI): queueing, delivery at syscall return, probing.
;
; Conventions:
;   * pending/blocked are 64-bit bitmaps, bit = sig - 1;
;   * action table entry (sig-1)*32: +0 handler (0=DFL, 1=IGN, else addr),
;     +8 restorer, +16 sa_flags, +24 sa_mask;
;   * delivery happens on the way back to ring 3 (sig_on_syscall_ret), where
;     the syscall frame F is: [0]=r10 .. [96]=r11(flags), [104]=RIP,
;     [112]=user RSP; RAX (live) = syscall return value;
;   * inside the dispatcher the frame sits behind the `call` return address,
;     so handlers see RIP at +112 and user RSP at +120 (one slot higher);
;   * death by signal stores 0x80000000 | sig into TCB_EXIT_CODE, which
;     sys_handler_waitpid turns into the raw POSIX WIFSIGNALED status.
; ==============================================================================

; ------------------------------------------------------------------------------
; sig_next_actionable: first pending, unblocked, actionable signal.
; In:  RBX = TCB pointer
; Out: EAX = signal (1..64) or 0; EDX = 0 (default terminate) or handler addr.
;      The found signal stays pending (the caller consumes it); dead signals
;      (SIG_IGN, default-ignore) are consumed here — they die either way.
; Clobbers: RAX, RCX, RDX, RSI
; ------------------------------------------------------------------------------
sig_next_actionable:
.sna_loop:
    mov rax, [rbx + TCB_SIG_PENDING]
    mov rcx, [rbx + TCB_SIG_BLOCKED]
    not rcx
    and rax, rcx                      ; rax = pending & ~blocked
    jz .sna_none
    bsf rcx, rax                      ; rcx = bit = sig - 1
    ; Tier A default-ignore set (stop signals are treated as no-ops until
    ; real job control arrives)
    lea esi, [rcx + 1]
    cmp esi, LINUX_SIGCHLD
    je .sna_drop
    cmp esi, LINUX_SIGCONT
    je .sna_drop
    cmp esi, LINUX_SIGWINCH
    je .sna_drop
    cmp esi, LINUX_SIGURG
    je .sna_drop
    cmp esi, LINUX_SIGSTOP
    je .sna_drop
    cmp esi, LINUX_SIGTSTP
    je .sna_drop
    cmp esi, LINUX_SIGTTIN
    je .sna_drop
    cmp esi, LINUX_SIGTTOU
    je .sna_drop
    ; handler lookup: 0 = DFL, 1 = IGN, else address
    mov rdx, rcx
    shl rdx, 5                       ; (sig-1) * 32
    mov rdx, [rbx + TCB_SIG_ACT + rdx]
    cmp rdx, 1
    je .sna_drop                      ; explicitly ignored
    mov eax, esi                      ; EAX = sig, EDX = DFL(0)/handler
    ret
.sna_drop:
    btr [rbx + TCB_SIG_PENDING], rcx
    jmp .sna_loop
.sna_none:
    xor eax, eax
    xor edx, edx
    ret

; ------------------------------------------------------------------------------
; sig_probe_actionable: EINTR check for blocking loops.
; Out: EAX = signal (0 = keep waiting). Preserves every other register.
;      Also consumes dead signals on the way (same semantics as delivery).
; ------------------------------------------------------------------------------
sig_probe_actionable:
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
    mov ebx, [r15 + sched_curr_task - kmain]
    imul ebx, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rbx]
    cmp dword [rbx + TCB_ABI_TYPE], 1
    jne .spa_none
    call sig_next_actionable
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
.spa_none:
    xor eax, eax
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

; ------------------------------------------------------------------------------
; sig_send_to_task: queue a signal to a task (Linux ABI targets only).
; In:  EDI = target slot, ESI = signal (1..64), EDX = sender pid (0 = kernel)
; Out: EAX = 0 | -ESRCH | -EINVAL. All other registers preserved.
;      A sleeping target is woken so its blocking loop re-checks EINTR.
; ------------------------------------------------------------------------------
sig_send_to_task:
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
    test esi, esi
    jle .sst_einval
    cmp esi, 64
    ja .sst_einval
    cmp edi, MAX_TASKS
    jae .sst_esrch
    imul ebx, edi, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rbx]
    cmp dword [rbx + TCB_STATE], TASK_UNUSED
    je .sst_esrch
    cmp dword [rbx + TCB_ABI_TYPE], 1
    jne .sst_esrch                    ; signals exist only for Linux ABI tasks
    mov ecx, esi
    dec ecx
    mov eax, 1
    shl rax, cl
    or [rbx + TCB_SIG_PENDING], rax
    mov dword [rbx + TCB_SIG_SENDER], edx
    cmp dword [rbx + TCB_STATE], TASK_SLEEPING
    jne @f
    mov dword [rbx + TCB_STATE], TASK_READY
    mov qword [rbx + TCB_FUTEX_ADDR], 0  ; a signal interrupts futex wait too
@@:
    xor eax, eax
    jmp .sst_done
.sst_einval:
    mov eax, LINUX_EINVAL
    jmp .sst_done
.sst_esrch:
    mov eax, LINUX_ESRCH
.sst_done:
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

; ------------------------------------------------------------------------------
; sig_kill_current: default signal action — log "[Linux ABI] Terminated by
; signal N" and terminate the current task with the WIFSIGNALED marker
; (0x80000000 | sig in TCB_EXIT_CODE -> waitpid raw status).
; In: ESI = signal number. Never returns.
; ------------------------------------------------------------------------------
sig_kill_current:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi                          ; sig lives at [rsp + 24]
    push rdi
    push r8
    push r9
    lea rsi, [r15 + str_l_sigkill - kmain]
    call puts
    mov rax, [rsp + 24]               ; sig (puts may clobber caller-saves)
    call putdec64
    mov al, 10
    call putc
    pop r9
    pop r8
    pop rdi
    pop rsi                           ; sig back in ESI
    pop rdx
    pop rcx
    pop rbx
    pop rax
    mov edi, esi
    or edi, 0x80000000                ; signal-death marker for waitpid
    call task_exit                    ; never returns

; ------------------------------------------------------------------------------
; sig_on_syscall_ret: deliver pending signals just before returning to ring 3.
; Called from .syscall_ret with RSP = frame base F and RAX = syscall result,
; interrupts off. May patch the frame (enter a signal handler) or never return
; (default action = terminate the task).
; ------------------------------------------------------------------------------
sig_on_syscall_ret:
    lea rbp, [rsp + 8]                ; F (this call pushed a return address)
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
    mov r12, rax                      ; syscall return value — probes clobber
                                      ; rax, so the result lives on in r12
    mov ebx, [r15 + sched_curr_task - kmain]
    imul ebx, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rbx]
    cmp dword [rbx + TCB_ABI_TYPE], 1
    jne .sor_ret
    movsxd r12, r12d                  ; Linux errno convention: 32-bit -errno
                                      ; returns must reach user space as
                                      ; sign-extended 64-bit values
.sor_loop:
    call sig_next_actionable          ; EAX = sig, EDX = DFL/handler
    test eax, eax
    jz .sor_ret
    mov r13d, eax                     ; r13 = sig
    mov ecx, r13d
    dec ecx
    btr [rbx + TCB_SIG_PENDING], rcx   ; consume this instance
    test rdx, rdx
    jz .sor_die                       ; default action -> die with the signal
    ; ---------------- handler delivery: build rt_sigframe ----------------
    mov r8, rdx                       ; r8 = handler address
    mov r14, [rbp + 112]              ; interrupted user RSP (F-relative)
    sub r14, 960                      ; (frame 952B + 8 for the ABI rounding)
    and r14, -16
    add r14, 8                        ; FR ≡ 8 mod 16: handlers enter like a
                                      ; CALL (pretcode = return address), while
                                      ; the FXSAVE area lands on 16 (FR+440)
    cmp r14, 0x1FE00000               ; must stay inside the task stack slot
    jb .sor_die
    cmp r14, 0x1FFFFE00
    jae .sor_die
    ; zero: siginfo (128B @FR+8), uc prefix+mcontext+sigmask (304B @FR+136)
    lea rdi, [r14 + 8]
    xor eax, eax
    mov ecx, 128 / 8
    rep stosq
    lea rdi, [r14 + 136]
    mov ecx, 304 / 8
    rep stosq
    ; fill mcontext from the frame (Linux x86_64 sigcontext layout)
    lea rdi, [r14 + 176]
    mov rax, [rbp + 16]
    mov [rdi + 0], rax                ; r8
    mov rax, [rbp + 8]
    mov [rdi + 8], rax                ; r9
    mov rax, [rbp + 0]
    mov [rdi + 16], rax               ; r10
    mov rax, [rbp + 96]
    mov [rdi + 24], rax               ; r11 (= user RFLAGS, as in Linux pt_regs)
    mov rax, [rbp + 72]
    mov [rdi + 32], rax               ; r12
    mov rax, [rbp + 64]
    mov [rdi + 40], rax               ; r13
    mov rax, [rbp + 56]
    mov [rdi + 48], rax               ; r14
    mov rax, [rbp + 48]
    mov [rdi + 56], rax               ; r15
    mov rax, [rbp + 24]
    mov [rdi + 64], rax               ; rdi
    mov rax, [rbp + 32]
    mov [rdi + 72], rax               ; rsi
    mov rax, [rbp + 88]
    mov [rdi + 80], rax               ; rbp
    mov rax, [rbp + 80]
    mov [rdi + 88], rax               ; rbx
    mov rax, [rbp + 40]
    mov [rdi + 96], rax               ; rdx
    mov rax, r12                      ; syscall return value (kept in r12)
    mov [rdi + 104], rax              ; rax
    mov rax, [rbp + 104]
    mov [rdi + 112], rax              ; rcx (= interrupted RIP)
    mov rax, [rbp + 112]
    mov [rdi + 120], rax              ; rsp
    mov rax, [rbp + 104]
    mov [rdi + 128], rax              ; rip
    mov rax, [rbp + 96]
    mov [rdi + 136], rax              ; eflags
    lea rax, [r14 + 440]
    mov [rdi + 184], rax              ; fpstate -> FXSAVE area inside the frame
    fxsave [r14 + 440]
    ; siginfo: si_signo + si_pid (si_code stays SI_USER = 0)
    mov dword [r14 + 8], r13d
    mov eax, [rbx + TCB_SIG_SENDER]
    mov [r14 + 8 + 16], eax           ; si_pid
    ; pretcode: the handler's `ret` jumps here -> restorer -> rt_sigreturn
    mov ecx, r13d
    dec ecx
    shl rcx, 5
    mov rax, [rbx + TCB_SIG_ACT + rcx + 8]
    mov [r14 + 0], rax                ; (0 if the app registered no restorer)
    ; uc_sigmask = mask from BEFORE the handler (rt_sigreturn restores it)
    mov rax, [rbx + TCB_SIG_BLOCKED]
    mov [r14 + 432], rax
    ; blocked |= sa_mask [| sig unless SA_NODEFER], KILL/STOP stay unblockable
    mov ecx, r13d
    dec ecx                           ; bit = sig - 1
    mov rdx, rcx
    shl rdx, 5                        ; action table offset
    test qword [rbx + TCB_SIG_ACT + rdx + 16], SA_NODEFER
    jnz @f
    mov rax, 1
    shl rax, cl
    or [rbx + TCB_SIG_BLOCKED], rax
@@:
    mov rax, [rbx + TCB_SIG_ACT + rdx + 24]
    or [rbx + TCB_SIG_BLOCKED], rax
    mov rax, [rbx + TCB_SIG_BLOCKED]
    and rax, not ((1 shl 8) or (1 shl 18))
    mov [rbx + TCB_SIG_BLOCKED], rax
    ; patch the syscall frame: RIP = handler, RSP = FR, RDI/RSI/RDX = args
    mov [rbp + 104], r8               ; RIP
    mov [rbp + 112], r14              ; user RSP
    mov [rbp + 24], r13               ; RDI = sig
    lea rax, [r14 + 8]
    mov [rbp + 32], rax               ; RSI = &siginfo
    lea rax, [r14 + 136]
    mov [rbp + 40], rax               ; RDX = &ucontext
    mov rax, r12                      ; RAX at handler entry = syscall result
    jmp .sor_loop                     ; queue may hold another signal
    ; ---------------- default action: terminate ----------------
.sor_die:
    mov esi, r13d                     ; sig
    call sig_kill_current             ; log + task_exit (never returns)
    jmp .sor_ret
.sor_ret:
    mov rax, r12                      ; restore the syscall return value
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

; ------------------------------------------------------------------------------
; sig_on_exception: turn a ring3 CPU exception into a POSIX signal.
; In:  RDI = common_ex frame base (15 pushed GPRs, vector at +120, then the
;      CPU frame), ESI = exception vector.
; Native-ABI tasks keep the legacy behaviour (log + task_exit). Linux ABI
; tasks: default / ignored / blocked fault -> terminate via sig_kill_current
; (waitpid reports WIFSIGNALED); installed handler -> build an rt_sigframe
; from the faulting context, patch the interrupted iretq frame in place
; (RIP = handler, RSP = sigframe, RDI/RSI/RDX = handler args) and return —
; common_ex then pops the GPRs and iretqs into the handler. Never returns
; unless a handler must run. Clobbers all registers.
; ------------------------------------------------------------------------------
sig_on_exception:
    push rbp
    mov rbp, rdi                      ; rbp = frame base (stable: above rsp)
    push rbx
    push r12
    push r13
    push r14
    mov r12d, esi                     ; r12 = vector
    ; the faulting ring3 context belongs to the running task
    mov ebx, [r15 + sched_curr_task - kmain]
    imul ebx, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rbx]
    cmp dword [rbx + TCB_ABI_TYPE], 1
    je .sx_sig
    ; native ABI: legacy terminate (exit code = the task's own RDI)
    lea rsi, [r15 + str_sys_crash - kmain]
    call puts
    mov edi, [rbp + 72]
    call task_exit                    ; never returns
.sx_sig:
    ; vector -> signal number (SIGSEGV is the fallback)
    mov esi, LINUX_SIGSEGV
    cmp r12d, 0
    je .sx_fpe
    cmp r12d, 2
    je .sx_fpe
    cmp r12d, 16
    je .sx_fpe
    cmp r12d, 1
    je .sx_trap
    cmp r12d, 3
    je .sx_ill
    cmp r12d, 4
    je .sx_ill
    cmp r12d, 5
    je .sx_ill
    cmp r12d, 6
    je .sx_ill
    cmp r12d, 17
    je .sx_bus
    jmp .sx_ptrs
.sx_fpe:
    mov esi, LINUX_SIGFPE
    jmp .sx_ptrs
.sx_trap:
    mov esi, LINUX_SIGTRAP
    jmp .sx_ptrs
.sx_ill:
    mov esi, LINUX_SIGILL
    jmp .sx_ptrs
.sx_bus:
    mov esi, LINUX_SIGBUS
.sx_ptrs:
    ; CPU frame pointers: error-code vectors shift RIP/RSP by one slot
    lea rdx, [rbp + 16*8]             ; &RIP
    lea rcx, [rbp + 19*8]             ; &user RSP
    cmp r12d, 14
    je .sx_hp
    cmp r12d, 13
    je .sx_hp
    cmp r12d, 12
    je .sx_hp
    cmp r12d, 11
    je .sx_hp
    cmp r12d, 10
    je .sx_hp
    cmp r12d, 8
    je .sx_hp
    cmp r12d, 17
    je .sx_hp
    jmp .sx_act
.sx_hp:
    lea rdx, [rbp + 17*8]             ; &RIP (after the CPU error code)
    lea rcx, [rbp + 20*8]             ; &user RSP
.sx_act:
    mov r13, rdx                      ; r13 = &rip slot
    mov r14, rcx                      ; r14 = &rsp slot
    ; action lookup: 0 = SIG_DFL, 1 = SIG_IGN, else handler address
    mov ecx, esi                      ; ESI = signal (r12 holds the vector!)
    dec ecx                           ; bit = sig - 1
    mov rax, rcx
    shl rax, 5
    mov rdx, [rbx + TCB_SIG_ACT + rax]
    mov r8, 1
    shl r8, cl
    test r8, [rbx + TCB_SIG_BLOCKED]
    jnz .sx_die                       ; blocking a fault is undefined -> kill
    test rdx, rdx
    jz .sx_die                        ; SIG_DFL -> terminate with the signal
    cmp rdx, 1
    je .sx_die                        ; SIG_IGN cannot swallow a fault
    ; --- handler delivery: rt_sigframe off the faulting user stack ---
    mov r8, rdx                       ; r8 = handler address
    mov rax, [r14]                    ; faulting user RSP
    sub rax, 960
    and rax, -16
    add rax, 8                        ; FR ≡ 8 mod 16: handlers enter like a CALL
    cmp rax, 0x1FE00000               ; must stay inside the task stack slot
    jb .sx_die
    cmp rax, 0x1FFFFE00
    jae .sx_die
    mov r9, rax                       ; r9 = FR
    ; zero: siginfo (128B @FR+8), uc prefix+mcontext+sigmask (304B @FR+136)
    lea rdi, [r9 + 8]
    xor eax, eax
    mov ecx, 128 / 8
    rep stosq
    lea rdi, [r9 + 136]
    mov ecx, 304 / 8
    rep stosq
    ; fill mcontext from the exception frame (Linux x86_64 sigcontext)
    lea rdi, [r9 + 176]
    mov rax, [rbp + 56]
    mov [rdi + 0], rax                ; r8
    mov rax, [rbp + 48]
    mov [rdi + 8], rax                ; r9
    mov rax, [rbp + 40]
    mov [rdi + 16], rax               ; r10
    mov rax, [rbp + 32]
    mov [rdi + 24], rax               ; r11
    mov rax, [rbp + 24]
    mov [rdi + 32], rax               ; r12
    mov rax, [rbp + 16]
    mov [rdi + 40], rax               ; r13
    mov rax, [rbp + 8]
    mov [rdi + 48], rax               ; r14
    mov rax, [rbp + 0]
    mov [rdi + 56], rax               ; r15
    mov rax, [rbp + 72]
    mov [rdi + 64], rax               ; rdi
    mov rax, [rbp + 80]
    mov [rdi + 72], rax               ; rsi
    mov rax, [rbp + 64]
    mov [rdi + 80], rax               ; rbp
    mov rax, [rbp + 104]
    mov [rdi + 88], rax               ; rbx
    mov rax, [rbp + 88]
    mov [rdi + 96], rax               ; rdx
    mov rax, [rbp + 112]
    mov [rdi + 104], rax              ; rax (faulting value)
    mov rax, [rbp + 96]
    mov [rdi + 112], rax              ; rcx (real rcx — exceptions keep it)
    mov rax, [r14]
    mov [rdi + 120], rax              ; rsp
    mov rax, [r13]
    mov [rdi + 128], rax              ; rip
    mov rax, [r13 + 16]
    mov [rdi + 136], rax              ; eflags
    mov rax, [r13 + 8]
    mov [rdi + 144], ax               ; cs
    movzx eax, r12b
    mov [rdi + 160], rax              ; trapno = vector
    mov rax, cr2
    mov [rdi + 176], rax              ; cr2 (fault address for #PF)
    lea rax, [r9 + 440]
    mov [rdi + 184], rax              ; fpstate -> FXSAVE area inside the frame
    fxsave [r9 + 440]
    ; siginfo: si_signo / si_code / si_addr
    mov dword [r9 + 8], esi           ; si_signo
    cmp esi, LINUX_SIGSEGV
    je .sx_si_segv
    cmp esi, LINUX_SIGILL
    je .sx_si_rip
    cmp esi, LINUX_SIGTRAP
    je .sx_si_rip
    cmp esi, LINUX_SIGFPE
    je .sx_si_rip
    mov dword [r9 + 16], 1            ; SIGBUS & co: generic trap cause
    jmp .sx_si_done
.sx_si_segv:
    cmp r12b, 14                      ; #PF -> SEGV_MAPERR + si_addr = CR2
    jne .sx_si_kernel
    mov dword [r9 + 16], 1
    mov rax, cr2
    mov [r9 + 24], rax
    jmp .sx_si_done
.sx_si_kernel:
    mov dword [r9 + 16], 0x80         ; SI_KERNEL (#GP and friends)
    jmp .sx_si_done
.sx_si_rip:
    mov dword [r9 + 16], 1            ; ILL_ILLOPC / TRAP_BRKPT / FPE_INTDIV
    mov rax, [r13]
    mov [r9 + 24], rax                ; si_addr = faulting RIP
.sx_si_done:
    ; pretcode: the handler's `ret` jumps here -> restorer -> rt_sigreturn
    mov ecx, esi
    dec ecx                           ; bit = sig - 1 (rep stosq ate rcx)
    mov rdx, rcx
    shl rdx, 5                        ; action table offset
    mov rax, [rbx + TCB_SIG_ACT + rdx + 8]
    mov [r9], rax                     ; (0 if the app registered no restorer)
    ; uc_sigmask = mask from BEFORE the handler (rt_sigreturn restores it)
    mov rax, [rbx + TCB_SIG_BLOCKED]
    mov [r9 + 432], rax
    ; blocked |= sa_mask [| sig unless SA_NODEFER]; KILL/STOP stay unblockable
    test qword [rbx + TCB_SIG_ACT + rdx + 16], SA_NODEFER
    jnz @f
    mov rax, 1
    shl rax, cl
    or [rbx + TCB_SIG_BLOCKED], rax
@@:
    mov rax, [rbx + TCB_SIG_ACT + rdx + 24]
    or [rbx + TCB_SIG_BLOCKED], rax
    mov rax, [rbx + TCB_SIG_BLOCKED]
    and rax, not ((1 shl 8) or (1 shl 18))
    mov [rbx + TCB_SIG_BLOCKED], rax
    ; consume any queued instance of this signal
    btr [rbx + TCB_SIG_PENDING], rcx
    ; patch the interrupted frame: RIP = handler, RSP = FR, args in rdi/rsi/rdx
    mov [r13], r8                     ; RIP
    mov [r14], r9                     ; user RSP
    mov [rbp + 72], rsi               ; RDI = sig (zero-extended)
    lea rax, [r9 + 8]
    mov [rbp + 80], rax               ; RSI = &siginfo
    lea rax, [r9 + 136]
    mov [rbp + 88], rax               ; RDX = &ucontext
    jmp .sx_ret
    ; ---------------- default action: terminate ----------------
.sx_die:
    call sig_kill_current             ; ESI = sig; never returns
.sx_ret:
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
; ==============================================================================
; vfs_release_tgid_locks: Release all advisory inode locks held by a TGID
; In: EDI = tgid
; ==============================================================================
vfs_release_tgid_locks:
    push rbx
    push rcx
    push rdx
    push rsi
    lea rsi, [r15 + inode_lock_table - kmain]
    xor ecx, ecx
.vrtl_loop:
    cmp ecx, MAX_INODE_LOCKS
    jae .vrtl_done
    mov edx, ecx
    shl edx, 4                       ; edx = ecx * 16
    cmp dword [rsi + rdx + 0], 0
    jz .vrtl_next
    cmp dword [rsi + rdx + 4], edi
    jne .vrtl_next
    mov dword [rsi + rdx + 0], 0
    mov dword [rsi + rdx + 4], 0
    mov dword [rsi + rdx + 8], 0
.vrtl_next:
    inc ecx
    jmp .vrtl_loop
.vrtl_done:
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

; ==============================================================================
; linux_syscall_dispatch: Dispatch Linux x86_64 syscall
; In: RAX = syscall number
;     RDI = arg1, RSI = arg2, RDX = arg3, R10 = arg4, R8 = arg5, R9 = arg6
; Out: RAX = result code (or negative error code)
; ==============================================================================
align 16
linux_syscall_dispatch:
    push rax
    push rsi
    lea rsi, [r15 + str_l_call_log - kmain]
    call puts
    mov rax, [rsp + 8]
    call putdec64
    mov al, 10
    call putc
    pop rsi
    pop rax
    cmp eax, LINUX_SYS_READ
    je .l_read
    cmp eax, LINUX_SYS_POLL
    je .l_poll
    cmp eax, LINUX_SYS_WRITE
    je .l_write
    cmp eax, LINUX_SYS_WRITEV
    je .l_writev
    cmp eax, LINUX_SYS_IOCTL
    je .l_ioctl
    cmp eax, LINUX_SYS_OPEN
    je .l_open
    cmp eax, LINUX_SYS_CLOSE
    je .l_close
    cmp eax, LINUX_SYS_LSEEK
    je .l_lseek
    cmp eax, LINUX_SYS_MMAP
    je .l_mmap
    cmp eax, LINUX_SYS_MUNMAP
    je .l_munmap
    cmp eax, LINUX_SYS_MPROTECT
    je .l_mprotect
    cmp eax, LINUX_SYS_BRK
    je .l_brk
    cmp eax, LINUX_SYS_DUP2
    je .l_dup2
    cmp eax, LINUX_SYS_GETPID
    je .l_getpid
    cmp eax, LINUX_SYS_EXIT
    je .l_exit
    cmp eax, LINUX_SYS_EXIT_GROUP
    je .l_exit_group
    cmp eax, LINUX_SYS_UNAME
    je .l_uname
    cmp eax, LINUX_SYS_GETCWD
    je .l_getcwd
    cmp eax, LINUX_SYS_CHDIR
    je .l_chdir
    cmp eax, LINUX_SYS_MKDIR
    je .l_mkdir
    cmp eax, LINUX_SYS_RMDIR
    je .l_rmdir
    cmp eax, LINUX_SYS_UNLINK
    je .l_unlink
    cmp eax, LINUX_SYS_ARCH_PRCTL
    je .l_arch_prctl
    cmp eax, LINUX_SYS_SET_TID_ADDR
    je .l_set_tid_address
    cmp eax, LINUX_SYS_GETTID
    je .l_gettid
    cmp eax, LINUX_SYS_GETUID
    je .l_getuid
    cmp eax, LINUX_SYS_GETGID
    je .l_getgid
    cmp eax, LINUX_SYS_GETEUID
    je .l_geteuid
    cmp eax, LINUX_SYS_GETEGID
    je .l_getegid
    cmp eax, LINUX_SYS_SETUID
    je .l_setuid
    cmp eax, LINUX_SYS_SETGID
    je .l_setgid
    cmp eax, LINUX_SYS_SETREUID
    je .l_setreuid
    cmp eax, LINUX_SYS_SETREGID
    je .l_setregid
    cmp eax, LINUX_SYS_SETRESUID
    je .l_setresuid
    cmp eax, LINUX_SYS_GETRESUID
    je .l_getresuid
    cmp eax, LINUX_SYS_SETRESGID
    je .l_setresGID
    cmp eax, LINUX_SYS_GETRESGID
    je .l_getresGID
    cmp eax, LINUX_SYS_GETGROUPS
    je .l_getgroups
    cmp eax, LINUX_SYS_SETGROUPS
    je .l_setgroups
    cmp eax, LINUX_SYS_UMASK
    je .l_umask
    cmp eax, LINUX_SYS_CHROOT
    je .l_chroot
    cmp eax, LINUX_SYS_FORK
    je .l_fork
    cmp eax, LINUX_SYS_VFORK
    je .l_fork
    cmp eax, LINUX_SYS_CLONE
    je .l_clone
    cmp eax, LINUX_SYS_WAIT4
    je .l_wait4
    cmp eax, LINUX_SYS_GETPPID
    je .l_getppid
    cmp eax, LINUX_SYS_EXECVE
    je .l_execve
    cmp eax, LINUX_SYS_RT_SIGACTION
    je .l_rt_sigaction
    cmp eax, LINUX_SYS_RT_SIGPROCMASK
    je .l_rt_sigprocmask
    cmp eax, LINUX_SYS_RT_SIGRETURN
    je .l_rt_sigreturn
    cmp eax, LINUX_SYS_PAUSE
    je .l_pause
    cmp eax, LINUX_SYS_ALARM
    je .l_alarm
    cmp eax, LINUX_SYS_FUTEX
    je .l_futex
    cmp eax, LINUX_SYS_PREAD64
    je .l_pread64
    cmp eax, LINUX_SYS_PWRITE64
    je .l_pwrite64
    cmp eax, LINUX_SYS_STAT
    je .l_stat
    cmp eax, LINUX_SYS_FSTAT
    je .l_fstat
    cmp eax, LINUX_SYS_LSTAT
    je .l_lstat
    cmp eax, LINUX_SYS_PIPE
    je .l_pipe
    cmp eax, LINUX_SYS_DUP
    je .l_dup
    cmp eax, LINUX_SYS_FCNTL
    je .l_fcntl
    cmp eax, LINUX_SYS_FLOCK
    je .l_flock
    cmp eax, LINUX_SYS_NEWFSTATAT
    je .l_newfstatat
    cmp eax, LINUX_SYS_OPENAT
    je .l_openat
    cmp eax, LINUX_SYS_GETDENTS64
    je .l_getdents64
    cmp eax, LINUX_SYS_KILL
    je .l_kill
    cmp eax, LINUX_SYS_TGKILL
    je .l_tgkill
    cmp eax, LINUX_SYS_SETPGID
    je .l_setpgid
    cmp eax, LINUX_SYS_GETPGRP
    je .l_getpgrp
    cmp eax, LINUX_SYS_SETSID
    je .l_setsid
    cmp eax, LINUX_SYS_GETPGID
    je .l_getpgid
    cmp eax, LINUX_SYS_GETSID
    je .l_getsid
    cmp eax, LINUX_SYS_ACCESS
    je .l_access
    cmp eax, LINUX_SYS_FACCESSAT
    je .l_faccessat
    cmp eax, LINUX_SYS_READV
    je .l_readv
    cmp eax, LINUX_SYS_MADVISE
    je .l_madvise
    cmp eax, LINUX_SYS_SYSINFO
    je .l_sysinfo
    cmp eax, LINUX_SYS_GETRUSAGE
    je .l_getrusage
    cmp eax, LINUX_SYS_TIMES
    je .l_times
    cmp eax, LINUX_SYS_READLINK
    je .l_readlink
    cmp eax, LINUX_SYS_READLINKAT
    je .l_readlinkat
    cmp eax, LINUX_SYS_FSYNC
    je .l_fsync
    cmp eax, LINUX_SYS_FDATASYNC
    je .l_fsync
    cmp eax, LINUX_SYS_TRUNCATE
    je .l_truncate
    cmp eax, LINUX_SYS_FTRUNCATE
    je .l_ftruncate
    cmp eax, LINUX_SYS_CHMOD
    je .l_chmod
    cmp eax, LINUX_SYS_FCHMOD
    je .l_fchmod
    cmp eax, LINUX_SYS_FCHMODAT
    je .l_fchmodat
    cmp eax, LINUX_SYS_CHOWN
    je .l_chown
    cmp eax, LINUX_SYS_FCHOWN
    je .l_fchown
    cmp eax, LINUX_SYS_FCHOWNAT
    je .l_fchownat
    cmp eax, LINUX_SYS_UTIMENSAT
    je .l_utimensat
    cmp eax, LINUX_SYS_STATX
    je .l_statx
    cmp eax, LINUX_SYS_RENAME
    je .l_rename
    cmp eax, LINUX_SYS_RENAMEAT
    je .l_renameat
    cmp eax, LINUX_SYS_RENAMEAT2
    je .l_renameat2
    cmp eax, LINUX_SYS_LINK
    je .l_link
    cmp eax, LINUX_SYS_LINKAT
    je .l_linkat
    cmp eax, LINUX_SYS_SYMLINK
    je .l_symlink
    cmp eax, LINUX_SYS_SYMLINKAT
    je .l_symlinkat
    cmp eax, LINUX_SYS_MKDIRAT
    je .l_mkdirat
    cmp eax, LINUX_SYS_UNLINKAT
    je .l_unlinkat

    ; Unhandled Linux syscall: log to serial and return -ENOSYS
    push rax
    push rsi
    lea rsi, [r15 + str_l_unimpl - kmain]
    call puts
    mov rax, [rsp + 8]
    call putdec64
    mov al, 10
    call putc
    pop rsi
    pop rax
    mov rax, LINUX_ENOSYS
    ret

.l_read:
    ; RDI = fd, RSI = buf, RDX = count
    call vfs_read_fd
    cmp rax, -1
    jne .l_read_ret
    mov eax, [r15 + vfs_last_err - kmain]   ; positive errno from vfs
    neg rax                                 ; Linux ABI wants -errno
.l_read_ret:
    ret

.l_write:
    ; RDI = fd, RSI = buf, RDX = count
    push rdi
    push rsi
    push rdx
    call vfs_write_fd
    pop rdx
    pop rsi
    pop rdi
    push rax
    push rsi
    push rdi
    push rdx
    lea rsi, [r15 + str_l_write_dbg - kmain]
    call puts
    mov rax, [rsp + 8]                  ; saved rdi (fd)
    call putdec64
    mov al, ','
    call putc
    mov al, ' '
    call putc
    mov rax, [rsp + 0]                  ; saved rdx (count)
    call putdec64
    lea rsi, [r15 + str_l_write_dbg2 - kmain]
    call puts
    mov rax, [rsp + 24]                 ; saved rax (result)
    call putdec64
    mov al, ' '
    call putc
    mov al, '['
    call putc
    push rsi
    push rcx
    mov rcx, [rsp + 16]                 ; count (saved rdx)
    cmp rcx, 40
    jbe @f
    mov rcx, 40
@@:
    mov rsi, [rsp + 32]                 ; buf (saved rsi)
.dbg_str_loop:
    test rcx, rcx
    jz .dbg_str_done
    mov al, ' '
    call putc
    movzx eax, byte [rsi]
    push rax
    shr al, 4
    and al, 0x0F
    add al, '0'
    cmp al, '9'
    jbe @f
    add al, 'A' - '0' - 10
@@:
    call putc
    pop rax
    and al, 0x0F
    add al, '0'
    cmp al, '9'
    jbe @f
    add al, 'A' - '0' - 10
@@:
    call putc
    inc rsi
    dec rcx
    jmp .dbg_str_loop
.dbg_str_done:
    pop rcx
    pop rsi
    mov al, ']'
    call putc
    mov al, 10
    call putc
    pop rdx
    pop rdi
    pop rsi
    pop rax
    cmp rax, -1
    jne .l_write_ret
    mov eax, [r15 + vfs_last_err - kmain]   ; positive errno from vfs
    neg rax                                 ; Linux ABI wants -errno
.l_write_ret:
    ret

.l_writev:
    ; RDI = fd, RSI = const struct iovec *iov, RDX = int iovcnt
    push rbx
    push r12
    push r13
    push r14
    push rbp
    mov r12, rdi                ; r12 = fd
    mov r13, rsi                ; r13 = iov
    mov r14, rdx                ; r14 = iovcnt
    xor ebp, ebp                ; ebp = total written
    ; H-L6: zero count = zero bytes, clamp iovcnt and validate the iovec array
    test r14, r14
    jz .l_wv_done
    cmp r14, 1024               ; IOV_MAX clamp (no r14*16 overflow)
    ja .l_wv_einval
    mov rdi, r13
    mov rsi, r14
    shl rsi, 4                  ; iovcnt * sizeof(struct iovec)
    call sys_validate_user_ptr
    jc .l_wv_efault
.l_wv_loop:
    test r14, r14
    jz .l_wv_done
    mov rdi, r12                ; fd
    mov rsi, [r13 + 0]          ; iov_base
    mov rdx, [r13 + 8]          ; iov_len
    test rdx, rdx
    jz .l_wv_next
    push rsi
    push rdx
    call sys_handler_write
    pop rdx
    pop rsi
    js .l_wv_err
    add rbp, rax
.l_wv_next:
    add r13, 16                 ; sizeof(struct iovec) = 16
    dec r14
    jmp .l_wv_loop
.l_wv_done:
    mov rax, rbp
    pop rbp
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
.l_wv_err:
    test rbp, rbp
    jnz .l_wv_done
    mov eax, [r15 + vfs_last_err - kmain]   ; positive errno from vfs
    neg rax                                 ; Linux ABI wants -errno
    pop rbp
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
.l_wv_einval:
    mov rax, LINUX_EINVAL
    jmp .l_wv_pop
.l_wv_efault:
    mov rax, LINUX_EFAULT
.l_wv_pop:
    pop rbp
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

.l_readv:
    ; RDI = fd, RSI = const struct iovec *iov, RDX = int iovcnt
    push rbx
    push r12
    push r13
    push r14
    push rbp
    mov r12, rdi                ; r12 = fd
    mov r13, rsi                ; r13 = iov
    mov r14, rdx                ; r14 = iovcnt
    xor ebp, ebp                ; ebp = total read
    test r14, r14
    jz .l_rv_done
    cmp r14, 1024
    ja .l_rv_einval
    mov rdi, r13
    mov rsi, r14
    shl rsi, 4                  ; iovcnt * 16
    call sys_validate_user_ptr
    jc .l_rv_efault
.l_rv_loop:
    test r14, r14
    jz .l_rv_done
    mov rdi, r12                ; fd
    mov rsi, [r13 + 0]          ; iov_base
    mov rdx, [r13 + 8]          ; iov_len
    test rdx, rdx
    jz .l_rv_next
    call vfs_read_fd
    cmp rax, -1
    je .l_rv_err
    test rax, rax
    jz .l_rv_done               ; EOF
    add rbp, rax
.l_rv_next:
    add r13, 16
    dec r14
    jmp .l_rv_loop
.l_rv_done:
    mov rax, rbp
    pop rbp
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
.l_rv_err:
    test rbp, rbp
    jnz .l_rv_done
    mov eax, [r15 + vfs_last_err - kmain]
    neg rax
    pop rbp
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
.l_rv_einval:
    mov rax, LINUX_EINVAL
    pop rbp
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
.l_rv_efault:
    mov rax, LINUX_EFAULT
    pop rbp
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

.l_ioctl:
    ; RDI = fd, RSI = request, RDX = arg
    ; Check if fd is a console fd (0, 1, 2)
    cmp rdi, 3
    jae .l_ioctl_notty
    ; TIOCGWINSZ = 0x5413: struct winsize { unsigned short ws_row, ws_col, ws_xpixel, ws_ypixel; }
    cmp rsi, 0x5413
    je .l_ioctl_winsz
    ; TCGETS = 0x5401: struct termios (60 bytes)
    cmp rsi, 0x5401
    je .l_ioctl_tcgets
    ; TCSETS/TCSETSW/TCSETSF (0x5402..0x5404): accept as no-op
    cmp rsi, 0x5402
    je .l_ioctl_ok
    cmp rsi, 0x5403
    je .l_ioctl_ok
    cmp rsi, 0x5404
    je .l_ioctl_ok
.l_ioctl_notty:
    mov rax, -25                ; -ENOTTY
    ret
.l_ioctl_ok:
    xor eax, eax
    ret
.l_ioctl_winsz:
    test rdx, rdx
    jz .l_ioctl_efault
    push rdi
    push rsi
    mov rdi, rdx
    mov rsi, 8
    call sys_validate_user_ptr
    pop rsi
    pop rdi
    jc .l_ioctl_efault
    mov word [rdx + 0], 25      ; ws_row = 25
    mov word [rdx + 2], 80      ; ws_col = 80
    mov word [rdx + 4], 0       ; ws_xpixel = 0
    mov word [rdx + 6], 0       ; ws_ypixel = 0
    xor eax, eax
    ret
.l_ioctl_tcgets:
    test rdx, rdx
    jz .l_ioctl_efault
    push rdi
    push rsi
    mov rdi, rdx
    mov rsi, 60
    call sys_validate_user_ptr
    pop rsi
    pop rdi
    jc .l_ioctl_efault
    push rdi
    push rcx
    mov rdi, rdx
    mov ecx, 60 / 4
    xor eax, eax
    rep stosd
    pop rcx
    pop rdi
    ; c_iflag = ICRNL (0x0100), c_oflag = OPOST|ONLCR (0x0005), c_cflag = CS8|CREAD (0x00bf), c_lflag = ISIG|ICANON|ECHO (0x000b)
    mov dword [rdx + 0],  0x00000100   ; c_iflag
    mov dword [rdx + 4],  0x00000005   ; c_oflag
    mov dword [rdx + 8],  0x000000bf   ; c_cflag
    mov dword [rdx + 12], 0x0000000b   ; c_lflag
    xor eax, eax
    ret
.l_ioctl_efault:
    mov rax, LINUX_EFAULT
    ret

.l_access:
    ; RDI = pathname, RSI = mode
    test rdi, rdi
    jz .l_acc_efault
    push rsi
    mov rsi, 64
    call sys_validate_user_ptr
    pop rsi
    jc .l_acc_efault
    push rsi
    mov rsi, rdi
    call vfs_lookup_with_fallback
    pop rsi
    test eax, eax
    jz .l_acc_enoent
    xor eax, eax
    ret
.l_acc_enoent:
    mov rax, LINUX_ENOENT
    ret
.l_acc_efault:
    mov rax, LINUX_EFAULT
    ret

.l_faccessat:
    ; RDI = dirfd, RSI = pathname, RDX = mode, R10 = flags
    mov rdi, rsi                ; pathname
    mov rsi, rdx                ; mode
    jmp .l_access

.l_madvise:
    ; RDI = addr, RSI = length, RDX = advice
    xor eax, eax                ; Successful stub
    ret

.l_sysinfo:
    ; RDI = struct sysinfo pointer (112 bytes)
    test rdi, rdi
    jz .l_sysinfo_efault
    push rdi
    push rsi
    mov rsi, 112
    call sys_validate_user_ptr
    pop rsi
    pop rdi
    jc .l_sysinfo_efault
    ; Zero 112 bytes
    push rdi
    push rcx
    push rax
    mov rcx, 112 / 8
    xor eax, eax
    rep stosq
    pop rax
    pop rcx
    pop rdi
    ; sysinfo.uptime = timer_ticks / 1000
    mov eax, [r15 + timer_ticks - kmain]
    xor edx, edx
    mov ecx, 1000
    div ecx
    mov [rdi + 0], rax          ; uptime (seconds)
    ; totalram = 256MB, freeram = ~128MB
    mov qword [rdi + 32], 256 * 1024 * 1024  ; totalram
    mov qword [rdi + 40], 128 * 1024 * 1024  ; freeram
    mov word  [rdi + 80], 4096               ; mem_unit = 4KB
    mov word  [rdi + 78], 1                  ; procs = 1
    xor eax, eax
    ret
.l_sysinfo_efault:
    mov rax, LINUX_EFAULT
    ret

.l_getrusage:
    ; RDI = int who (RUSAGE_SELF = 0, RUSAGE_CHILDREN = -1, RUSAGE_THREAD = 1)
    ; RSI = struct rusage pointer (144 bytes: 18 * uint64_t)
    test rsi, rsi
    jz .l_rusage_efault
    push rdi
    push rsi
    mov rdi, rsi
    mov rsi, 144
    call sys_validate_user_ptr
    pop rsi
    pop rdi
    jc .l_rusage_efault
    ; Zero out struct rusage (144 bytes)
    push rdi
    push rcx
    push rax
    mov rdi, rsi
    mov ecx, 144 / 8
    xor eax, eax
    rep stosq
    pop rax
    pop rcx
    pop rdi
    xor eax, eax
    ret
.l_rusage_efault:
    mov rax, LINUX_EFAULT
    ret

.l_times:
    ; RDI = struct tms pointer (32 bytes: 4 * clock_t = 4 * uint64_t)
    test rdi, rdi
    jz .l_times_skip_buf
    push rdi
    push rsi
    mov rsi, 32
    call sys_validate_user_ptr
    pop rsi
    pop rdi
    jc .l_times_efault
    ; Zero 32 bytes
    mov qword [rdi + 0],  0             ; tms_utime
    mov qword [rdi + 8],  0             ; tms_stime
    mov qword [rdi + 16], 0             ; tms_cutime
    mov qword [rdi + 24], 0             ; tms_cstime
.l_times_skip_buf:
    ; Return clock ticks since boot (timer_ticks)
    mov eax, [r15 + timer_ticks - kmain]
    ret
.l_times_efault:
    mov rax, LINUX_EFAULT
    ret

.l_readlinkat:
    ; RDI = dirfd, RSI = pathname, RDX = buf, R10 = bufsiz
    mov rdi, rsi                ; pathname
    mov rsi, rdx                ; buf
    mov rdx, r10                ; bufsiz
    ; Fallthrough to .l_readlink

.l_readlink:
    ; RDI = pathname, RSI = buf, RDX = bufsiz
    test rdi, rdi
    jz .l_rl_fault
    test rsi, rsi
    jz .l_rl_fault
    test rdx, rdx
    jz .l_rl_inval
    push rdi
    push rsi
    push rdx
    ; Validate pathname (64 bytes)
    mov rsi, 64
    call sys_validate_user_ptr
    pop rdx
    pop rsi
    pop rdi
    jc .l_rl_fault

    push rdi
    push rsi
    push rdx
    ; Validate user destination buffer
    mov rdi, rsi
    mov rsi, rdx
    call sys_validate_user_ptr
    pop rdx
    pop rsi
    pop rdi
    jc .l_rl_fault

    ; Check if path points to /proc/self/exe or /proc/<pid>/exe
    cmp dword [rdi], '/pro'
    jne .l_rl_disk

    ; Target string: write current executable path to user buf
    ; Form: "/<task_name>"
    push rdi
    push rsi
    push rdx
    push rcx
    mov r8, rsi                         ; r8 = dest buf
    mov r9, rdx                         ; r9 = bufsiz
    mov byte [r8], '/'
    inc r8
    dec r9
    ; Get current task TCB_NAME
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rsi, [r15 + task_table - kmain + rax + TCB_NAME]
    xor ecx, ecx
.l_rl_cp:
    test r9, r9
    jz .l_rl_cp_done
    lodsb
    test al, al
    jz .l_rl_cp_done
    mov [r8], al
    inc r8
    dec r9
    inc ecx
    jmp .l_rl_cp
.l_rl_cp_done:
    inc ecx                             ; +1 for leading '/'
    mov rax, rcx                        ; return bytes copied (no trailing zero per POSIX readlink)
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    ret

.l_rl_disk:
    ; Lookup pathname in ext4 (without following symlink) and read symlink from inode
    push rsi                            ; buf
    push rdx                            ; bufsiz
    mov rsi, rdi                        ; rsi = pathname
    call ext4_lookup_nofollow
    pop rsi                             ; rsi = bufsiz
    pop rdi                             ; rdi = buf
    test eax, eax
    jz .l_rl_noent
    call ext4_readlink_inode
    cmp rax, -1
    je .l_rl_inval                      ; not a symlink
    ret

.l_rl_noent:
    mov rax, LINUX_ENOENT
    ret
.l_rl_fault:
    mov rax, LINUX_EFAULT
    ret
.l_rl_inval:
    mov rax, LINUX_EINVAL
    ret

.l_open:
    ; RDI = pathname, RSI = flags, RDX = mode
    call vfs_open
    cmp rax, -1
    jne .l_open_ret
    mov rax, LINUX_ENOENT
.l_open_ret:
    ret

.l_openat:
    ; RDI = dirfd, RSI = pathname, RDX = flags, R10 = mode
    mov rdi, rsi                     ; pathname
    mov rsi, rdx                     ; flags
    mov rdx, r10                     ; mode
    call vfs_open
    cmp rax, -1
    jne .l_openat_ret
    mov rax, LINUX_ENOENT
.l_openat_ret:
    ret

.l_close:
    ; RDI = fd
    call vfs_close
    cmp rax, -1
    jne .l_close_ret
    mov rax, LINUX_EBADF                   ; vfs_close only fails on a bad fd
.l_close_ret:
    ret

.l_lseek:
    ; RDI = fd, RSI = offset, RDX = whence
    call vfs_lseek
    ret

.l_brk:
    ; RDI = new brk
    call sys_handler_brk
    ret

.l_dup2:
    ; RDI = oldfd, RSI = newfd
    call vfs_dup2
    ret

.l_getpid:
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rax]
    mov eax, [rbx + TCB_TGID]
    test eax, eax
    jnz @f
    mov eax, [r15 + sched_curr_task - kmain]
    inc eax
@@:
    ret

.l_gettid:
    mov eax, [r15 + sched_curr_task - kmain]
    inc eax                     ; 1-based TID
    ret

.l_exit_group:
    ; RDI = exit code
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rax]
    mov edx, [rbx + TCB_TGID]
    test edx, edx
    jz .l_exit
    xor ecx, ecx
.exit_grp_loop:
    cmp ecx, MAX_TASKS
    jae .l_exit
    cmp ecx, [r15 + sched_curr_task - kmain]
    je .exit_grp_next
    imul eax, ecx, TCB_SIZE
    lea rsi, [r15 + task_table - kmain + rax]
    cmp dword [rsi + TCB_STATE], TASK_UNUSED
    je .exit_grp_next
    cmp dword [rsi + TCB_STATE], TASK_DEAD
    je .exit_grp_next
    cmp [rsi + TCB_TGID], edx
    jne .exit_grp_next
    mov dword [rsi + TCB_STATE], TASK_DEAD
    dec dword [r15 + sched_task_cnt - kmain]
.exit_grp_next:
    inc ecx
    jmp .exit_grp_loop

.l_exit:
    ; RDI = exit code
    push rdi
    lea rsi, [r15 + str_l_exit_log - kmain]
    call puts
    pop rdi
    mov rax, rdi
    call putdec64
    mov al, 10
    call putc
    call task_exit
    ret

; ------------------------------------------------------------------------------
; .l_fork / .l_vfork / .l_clone (simplified): duplicate the current task.
;   fork/vfork -> identical call site, both land here.
;   clone(flags, stack, ...) -> flags and child_stack are IGNORED: the child
;   gets a private copy of the parent's VM and continues on its own stack copy.
;   That is semantically wrong for CLONE_VM/CLONE_THREAD (Tier B), but correct
;   enough for vfork+exec and musl's clone-based spawn helpers (child never
;   shares the parent's stack with the parent running, so no interference).
;
; Child frame is built so sched_restore + iretq resumes it with RAX = 0 at the
; same RIP/RSP as the parent; the parent gets the child pid (slot + 1, the
; same numbering .l_getpid uses).
;
; Register frame of the syscall stub at this point (its pushes are balanced
; before the dispatcher's call, so [rsp+0] = return address into the stub):
;   +8 r10, +16 r9, +24 r8, +32 rdi, +40 rsi, +48 rdx, +56 r15,
;   +64 r14, +72 r13, +80 r12, +88 rbx, +96 rbp, +104 r11 (=RFLAGS),
;   +112 user RIP (rcx), +120 user RSP
; ------------------------------------------------------------------------------
; .l_clone: Linux clone(flags, child_stack, parent_tid, child_tid, tls)
;   flags       = RDI (also [rbx + 32])
;   child_stack = RSI (also [rbx + 40])
;   parent_tid  = RDX (also [rbx + 48])
;   child_tid   = R10 (also [rbx + 8])
;   tls         = R8  (also [rbx + 24])
; ------------------------------------------------------------------------------
.l_clone:
    test edi, 0x00000100              ; CLONE_VM?
    jz .l_fork                        ; non-CLONE_VM -> fork semantics

    lea rbx, [rsp]                    ; rbx = stable syscall frame base

    ; 1. Find a free slot in task_table (1..MAX_TASKS-1)
    mov ecx, 1
.clone_find_slot:
    cmp ecx, MAX_TASKS
    jae .clone_try_dead
    imul eax, ecx, TCB_SIZE
    lea rdi, [r15 + task_table - kmain + rax]
    cmp dword [rdi + TCB_STATE], TASK_UNUSED
    je .clone_slot_found
    inc ecx
    jmp .clone_find_slot

.clone_try_dead:
    mov ecx, 1
.clone_find_dead:
    cmp ecx, MAX_TASKS
    jae .l_clone_eagain
    imul eax, ecx, TCB_SIZE
    lea rdi, [r15 + task_table - kmain + rax]
    cmp dword [rdi + TCB_STATE], TASK_DEAD
    je .clone_slot_found
    inc ecx
    jmp .clone_find_dead

.l_clone_eagain:
    mov rax, LINUX_EAGAIN
    ret

.clone_slot_found:
    ; ecx = child slot (1..MAX_TASKS-1)
    ; rdi = child TCB pointer
    mov r12d, ecx                     ; r12d = child slot
    mov r13, rdi                      ; r13  = child TCB

    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rbp, [r15 + task_table - kmain + rax]   ; rbp = parent TCB

    ; 2. Initialize child TCB fields
    mov [r13 + TCB_ID], r12d
    mov dword [r13 + TCB_SLEEP], 0
    mov dword [r13 + TCB_SLICE], SCHED_QUANTUM
    mov qword [r13 + TCB_TICKS], 0
    mov dword [r13 + TCB_IS_USER], 1
    mov dword [r13 + TCB_EXIT_CODE], 0
    mov dword [r13 + TCB_WAIT_TID], -1
    mov dword [r13 + TCB_ABI_TYPE], 1
    mov dword [r13 + TCB_IS_THREAD], 1

    ; Link parent
    mov eax, [r15 + sched_curr_task - kmain]
    mov [r13 + TCB_PARENT_ID], eax

    ; Shared virtual address space: same CR3
    mov rax, [rbp + TCB_CR3]
    mov [r13 + TCB_CR3], rax

    ; Shared heap brk
    mov eax, [rbp + TCB_HEAP_BRK]
    mov [r13 + TCB_HEAP_BRK], eax

    ; Thread Group ID (TGID)
    mov eax, [rbp + TCB_TGID]
    test eax, eax
    jnz @f
    mov eax, [r15 + sched_curr_task - kmain]
    inc eax
    mov [rbp + TCB_TGID], eax
@@:
    mov [r13 + TCB_TGID], eax

    ; Task name
    mov rax, [rbp + TCB_NAME]
    mov [r13 + TCB_NAME], rax

    ; Copy FXSAVE
    lea rsi, [rbp + TCB_FXSAVE]
    lea rdi, [r13 + TCB_FXSAVE]
    mov ecx, 512 / 8
    rep movsq

    ; Inherit signal handlers and mask
    lea rsi, [rbp + TCB_SIG_PENDING]
    lea rdi, [r13 + TCB_SIG_PENDING]
    mov ecx, (TCB_SIZE - TCB_SIG_PENDING) / 8
    rep movsq
    mov dword [r13 + TCB_ALARM_MS], 0
    mov qword [r13 + TCB_FUTEX_ADDR], 0

    ; 3. TLS setup (CLONE_SETTLS = 0x00080000)
    ; In syscall frame: [rbx + 32] = flags, [rbx + 24] = tls (r8)
    test dword [rbx + 32], 0x00080000
    jz .clone_no_settls
    mov rax, [rbx + 24]               ; tls arg
    mov [r13 + TCB_FS_BASE], rax
    jmp .clone_tls_done
.clone_no_settls:
    mov rax, [rbp + TCB_FS_BASE]
    mov [r13 + TCB_FS_BASE], rax
.clone_tls_done:

    ; 4. CLONE_CHILD_CLEARTID (0x00200000)
    ; In syscall frame: [rbx + 8] = child_tid (r10)
    test dword [rbx + 32], 0x00200000
    jz .clone_no_cleartid
    mov rax, [rbx + 8]
    mov [r13 + TCB_CLEAR_CHILD_TID], rax
    jmp .clone_cleartid_done
.clone_no_cleartid:
    mov qword [r13 + TCB_CLEAR_CHILD_TID], 0
.clone_cleartid_done:

    ; 5. CLONE_PARENT_SETTID (0x00100000)
    test dword [rbx + 32], 0x00100000
    jz .clone_no_parent_settid
    mov rdi, [rbx + 48]               ; parent_tid ptr (rdx)
    test rdi, rdi
    jz .clone_no_parent_settid
    mov esi, 4
    call sys_validate_user_ptr
    jc .clone_no_parent_settid
    lea eax, [r12 + 1]                ; child TID
    mov [rdi], eax
.clone_no_parent_settid:

    ; 6. CLONE_CHILD_SETTID (0x01000000)
    test dword [rbx + 32], 0x01000000
    jz .clone_no_child_settid
    mov rdi, [rbx + 8]                ; child_tid ptr (r10)
    test rdi, rdi
    jz .clone_no_child_settid
    mov esi, 4
    call sys_validate_user_ptr
    jc .clone_no_child_settid
    lea eax, [r12 + 1]                ; child TID
    mov [rdi], eax
.clone_no_child_settid:

    ; 7. User stack for child
    ; [rbx + 40] = child_stack (rsi)
    mov rax, [rbx + 40]
    test rax, rax
    jnz @f
    mov rax, [rbx + 120]              ; fallback to parent user RSP
@@:
    mov [r13 + TCB_USER_RSP], rax

    ; 8. Child kernel stack and iretq / GPR snapshot
    mov eax, r12d
    imul eax, TASK_STACK_SIZE
    lea rax, [r15 + task_stacks - kmain + rax]
    mov [r13 + TCB_STACK_BASE], rax
    add rax, TASK_STACK_SIZE
    mov [r13 + TCB_STACK_TOP], rax
    mov rdi, rax

    ; iretq frame (high -> low): SS, user RSP, RFLAGS, CS, RIP
    sub rdi, 8
    mov qword [rdi], 0x23             ; USER_DATA_SEL | 3
    sub rdi, 8
    mov rsi, [r13 + TCB_USER_RSP]
    mov [rdi], rsi                    ; user RSP
    sub rdi, 8
    mov rsi, [rbx + 104]
    mov [rdi], rsi                    ; RFLAGS
    sub rdi, 8
    mov qword [rdi], 0x2B             ; USER_CODE_SEL | 3
    sub rdi, 8
    mov rsi, [rbx + 112]
    mov [rdi], rsi                    ; user RIP

    ; GPR image in sched_restore pop order reversed (rax highest .. r15 lowest)
    sub rdi, 8
    xor eax, eax
    mov [rdi], rax                    ; rax = 0 (child thread returns 0 from clone)
    sub rdi, 8
    mov rsi, [rbx + 88]
    mov [rdi], rsi                    ; rbx
    sub rdi, 8
    mov rsi, [rbx + 112]
    mov [rdi], rsi                    ; rcx
    sub rdi, 8
    mov rsi, [rbx + 48]
    mov [rdi], rsi                    ; rdx
    sub rdi, 8
    mov rsi, [rbx + 40]
    mov [rdi], rsi                    ; rsi
    sub rdi, 8
    mov rsi, [rbx + 32]
    mov [rdi], rsi                    ; rdi
    sub rdi, 8
    mov rsi, [rbx + 96]
    mov [rdi], rsi                    ; rbp
    sub rdi, 8
    mov rsi, [rbx + 24]
    mov [rdi], rsi                    ; r8
    sub rdi, 8
    mov rsi, [rbx + 16]
    mov [rdi], rsi                    ; r9
    sub rdi, 8
    mov rsi, [rbx + 8]
    mov [rdi], rsi                    ; r10
    sub rdi, 8
    mov rsi, [rbx + 104]
    mov [rdi], rsi                    ; r11
    sub rdi, 8
    mov rsi, [rbx + 80]
    mov [rdi], rsi                    ; r12
    sub rdi, 8
    mov rsi, [rbx + 72]
    mov [rdi], rsi                    ; r13
    sub rdi, 8
    mov rsi, [rbx + 64]
    mov [rdi], rsi                    ; r14
    sub rdi, 8
    mov rsi, [rbx + 56]
    mov [rdi], rsi                    ; r15
    mov [r13 + TCB_RSP], rdi

    ; 9. Publish child thread: READY + increment task count
    inc dword [r15 + sched_task_cnt - kmain]
    mov dword [r13 + TCB_STATE], TASK_READY
    lea eax, [r12 + 1]                ; parent returns child TID (slot + 1)
    ret

.l_fork:
    lea rbx, [rsp]                    ; rbx = stable frame base (keep!)

    ; 1. Fresh slot + page tables (child TCB, CR3, phys code/stack bases)
    push rbx
    call task_alloc_user_slot
    pop rbx
    cmp eax, -1
    je .l_fork_eagain
    ; EAX = slot, RDI = child TCB, R9 = child phys_code, R10 = child phys_stack
    mov r12d, eax                     ; r12 = child slot
    mov r13, rdi                      ; r13 = child TCB
    mov r14, r9                       ; r14 = child phys_code
    mov r11, r10                      ; r11 = child phys_stack

    ; Link the child to this parent before the long copies: a concurrent
    ; waitpid scan must never see the half-built slot as its own zombie.
    mov eax, [r15 + sched_curr_task - kmain]
    mov [r13 + TCB_PARENT_ID], eax

    ; 2. Deep-copy parent code (4MB) and stack (2MB) into the child's slot
    mov ecx, [r15 + sched_curr_task - kmain]
    dec ecx                           ; parent slot -> 0-based
    imul ecx, USER_MEM_STRIDE
    add ecx, USER_PHYS_BASE           ; ecx = parent phys_code
    mov r8, rcx                       ; r8  = parent phys_code
    lea r9, [rcx + 0x00400000]        ; r9  = parent phys_stack
    mov rsi, r8
    mov rdi, r14
    mov ecx, (4 * 1024 * 1024) / 8
    rep movsq                         ; code slot
    mov rsi, r9
    mov rdi, r11
    mov ecx, (2 * 1024 * 1024) / 8
    rep movsq                         ; stack slot

    ; 3. File descriptor table: copy the parent's row, bump pipe refcounts
    ;    (mirrors vfs_dup2; the child's vfs_cleanup_task_fds drops them again)
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, MAX_FDS_PER_TASK * FD_ENTRY_SIZE
    lea rsi, [r15 + task_fd_tables - kmain + rax]
    mov eax, r12d
    imul eax, MAX_FDS_PER_TASK * FD_ENTRY_SIZE
    lea rdi, [r15 + task_fd_tables - kmain + rax]
    mov ecx, (MAX_FDS_PER_TASK * FD_ENTRY_SIZE) / 8
    rep movsq
    mov eax, r12d
    imul eax, MAX_FDS_PER_TASK * FD_ENTRY_SIZE
    lea rdi, [r15 + task_fd_tables - kmain + rax]
    xor ecx, ecx
.l_fork_fd_loop:
    cmp ecx, MAX_FDS_PER_TASK
    jae .l_fork_fd_done
    cmp byte [rdi], FD_TYPE_PIPE
    jne .l_fork_fd_next
    movzx eax, word [rdi + 2]
    imul eax, eax, PIPE_STRUCT_SIZE
    lea rdx, [r15 + pipe_table - kmain + rax]
    cmp byte [rdi + 1], O_RDONLY
    jne .l_fork_fd_wr
    inc word [rdx + 12]               ; readers++
    jmp .l_fork_fd_next
.l_fork_fd_wr:
    inc word [rdx + 14]               ; writers++
.l_fork_fd_next:
    add rdi, FD_ENTRY_SIZE
    inc ecx
    jmp .l_fork_fd_loop
.l_fork_fd_done:

    ; 4. TCB fields: child inherits, parent slot links, spawn-only state reset
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rbp, [r15 + task_table - kmain + rax]   ; rbp = parent TCB
    mov [r13 + TCB_ID], r12d
    mov dword [r13 + TCB_SLEEP], 0
    mov dword [r13 + TCB_SLICE], SCHED_QUANTUM
    mov dword [r13 + TCB_TICKS], 0
    mov dword [r13 + TCB_IS_USER], 1
    mov dword [r13 + TCB_EXIT_CODE], 0
    mov dword [r13 + TCB_WAIT_TID], -1
    mov eax, [rbp + TCB_HEAP_BRK]     ; brk heap is identity-shared anyway,
    mov [r13 + TCB_HEAP_BRK], eax     ; keep the same break point
    mov eax, [rbp + TCB_ABI_TYPE]
    mov [r13 + TCB_ABI_TYPE], eax
    mov rax, [rbp + TCB_FS_BASE]      ; musl TLS base (same memory image)
    mov [r13 + TCB_FS_BASE], rax
    lea eax, [r12 + 1]
    mov [r13 + TCB_TGID], eax         ; child process gets its own PID as TGID
    mov dword [r13 + TCB_IS_THREAD], 0
    mov qword [r13 + TCB_CLEAR_CHILD_TID], 0
    mov rax, [rbx + 120]
    mov [r13 + TCB_USER_RSP], rax     ; parent user RSP
    mov rax, [rbp + TCB_NAME]
    mov [r13 + TCB_NAME], rax
    lea rsi, [rbp + TCB_FXSAVE]       ; FPU/SSE state from the parent
    lea rdi, [r13 + TCB_FXSAVE]
    mov ecx, 512 / 8
    rep movsq
    lea rsi, [rbp + TCB_SIG_PENDING]  ; pending/blocked/sender + action table
    lea rdi, [r13 + TCB_SIG_PENDING]  ; (fork inherits handlers, per POSIX)
    mov ecx, (TCB_SIZE - TCB_SIG_PENDING) / 8
    rep movsq
    mov dword [r13 + TCB_ALARM_MS], 0 ; alarm(2) is NOT inherited (POSIX)

    ; 5. Child kernel stack + initial iretq frame (parent register snapshot)
    mov eax, r12d
    imul eax, TASK_STACK_SIZE
    lea rax, [r15 + task_stacks - kmain + rax]
    mov [r13 + TCB_STACK_BASE], rax
    add rax, TASK_STACK_SIZE
    mov [r13 + TCB_STACK_TOP], rax
    mov rdi, rax                      ; rdi walks downward building the frame

    ; iretq image (high -> low): SS, user RSP, RFLAGS, CS, RIP
    sub rdi, 8
    mov qword [rdi], 0x23             ; USER_DATA_SEL | 3
    sub rdi, 8
    mov rsi, [rbx + 120]
    mov [rdi], rsi                    ; user RSP
    sub rdi, 8
    mov rsi, [rbx + 104]
    mov [rdi], rsi                    ; RFLAGS (r11 slot = flags after SYSCALL)
    sub rdi, 8
    mov qword [rdi], 0x2B             ; USER_CODE_SEL | 3
    sub rdi, 8
    mov rsi, [rbx + 112]
    mov [rdi], rsi                    ; user RIP

    ; GPR image in sched_restore pop order reversed (rax highest .. r15 lowest)
    sub rdi, 8
    xor eax, eax
    mov [rdi], rax                    ; rax = 0: fork returns 0 in the child
    sub rdi, 8
    mov rsi, [rbx + 88]
    mov [rdi], rsi                    ; rbx
    sub rdi, 8
    mov rsi, [rbx + 112]
    mov [rdi], rsi                    ; rcx (user rcx is lost to SYSCALL anyway)
    sub rdi, 8
    mov rsi, [rbx + 48]
    mov [rdi], rsi                    ; rdx
    sub rdi, 8
    mov rsi, [rbx + 40]
    mov [rdi], rsi                    ; rsi
    sub rdi, 8
    mov rsi, [rbx + 32]
    mov [rdi], rsi                    ; rdi
    sub rdi, 8
    mov rsi, [rbx + 96]
    mov [rdi], rsi                    ; rbp
    sub rdi, 8
    mov rsi, [rbx + 24]
    mov [rdi], rsi                    ; r8
    sub rdi, 8
    mov rsi, [rbx + 16]
    mov [rdi], rsi                    ; r9
    sub rdi, 8
    mov rsi, [rbx + 8]
    mov [rdi], rsi                    ; r10
    sub rdi, 8
    mov rsi, [rbx + 104]
    mov [rdi], rsi                    ; r11 (flags copy: same value the parent
                                      ; sees in r11 after sysret)
    sub rdi, 8
    mov rsi, [rbx + 80]
    mov [rdi], rsi                    ; r12
    sub rdi, 8
    mov rsi, [rbx + 72]
    mov [rdi], rsi                    ; r13
    sub rdi, 8
    mov rsi, [rbx + 64]
    mov [rdi], rsi                    ; r14
    sub rdi, 8
    mov rsi, [rbx + 56]
    mov [rdi], rsi                    ; r15 (lowest GPR slot)
    mov [r13 + TCB_RSP], rdi

    ; 6. Publish the child: READY + task counter, then return its pid
    inc dword [r15 + sched_task_cnt - kmain]
    mov dword [r13 + TCB_STATE], TASK_READY
    lea eax, [r12 + 1]                ; pid = slot + 1 (matches .l_getpid)
    ret

.l_fork_eagain:
    mov rax, LINUX_EAGAIN             ; -11: no free task slots
    ret

; ------------------------------------------------------------------------------
; .l_wait4: Linux wait4(pid, status, options, rusage) - rusage (R10) ignored.
; pid > 1 -> 1-based pid translated to slot; pid <= 0 -> any child.
; sys_handler_waitpid already writes POSIX status ((code & 0xFF) << 8) and
; returns the slot; convert back to pid on success.
; ------------------------------------------------------------------------------
.l_wait4:
    cmp rdi, 1
    jl .l_wait4_any                   ; 0 / negative -> any child
    je .l_wait4_echild                ; pid 1 = kernel task, never a child
    dec rdi                           ; slot = pid - 1
    cmp rdi, MAX_TASKS
    jae .l_wait4_echild
.l_wait4_any:
    call sys_handler_waitpid
    test rax, rax
    jle .l_wait4_done                 ; 0 (WNOHANG) or -errno pass through
    inc rax                           ; slot -> pid
.l_wait4_done:
    ret
.l_wait4_echild:
    mov rax, -10                      ; -ECHILD
    ret

.l_getppid:
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    mov eax, [r15 + task_table - kmain + rax + TCB_PARENT_ID]
    inc eax                           ; slot -> pid (slot 0 -> 1 = init)
    ret

; ------------------------------------------------------------------------------
; .l_rt_sigaction (Linux #13): RDI = sig, RSI = act (user, 0 = query),
; RDX = oldact (user, 0 = none). User struct: +0 handler, +8 flags,
; +16 restorer, +24 sa_mask (kernel sigset, 8 bytes).
; ------------------------------------------------------------------------------
.l_rt_sigaction:
    test rdi, rdi
    jle .sa_einval
    cmp rdi, 64
    ja .sa_einval
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rax]    ; rbx = current TCB
    lea rcx, [rdi - 1]
    shl rcx, 5                                   ; (sig-1) * 32
    ; --- write oldact out ---
    test rdx, rdx
    jz .sa_get_act
    push rdi
    push rsi
    push rdx
    mov rdi, rdx
    mov esi, 32
    call sys_validate_user_ptr
    pop rdx
    pop rsi
    pop rdi
    jc .sa_efault
    mov rax, [rbx + TCB_SIG_ACT + rcx + 0]
    mov [rdx + 0], rax             ; handler
    mov rax, [rbx + TCB_SIG_ACT + rcx + 8]
    mov [rdx + 16], rax            ; restorer
    mov rax, [rbx + TCB_SIG_ACT + rcx + 16]
    mov [rdx + 8], rax             ; flags
    mov rax, [rbx + TCB_SIG_ACT + rcx + 24]
    mov [rdx + 24], rax            ; sa_mask
.sa_get_act:
    ; --- install new act ---
    test rsi, rsi
    jz .sa_ok
    push rdi
    push rdx
    push rsi                          ; the act pointer (validate clobbers rsi
    mov rdi, rsi                      ; by passing the size in rsi)
    mov esi, 32
    call sys_validate_user_ptr
    pop rsi
    pop rdx
    pop rdi
    jc .sa_efault
    mov rax, [rsi + 0]             ; handler
    mov [rbx + TCB_SIG_ACT + rcx + 0], rax
    mov rax, [rsi + 8]             ; flags
    mov [rbx + TCB_SIG_ACT + rcx + 16], rax
    mov rax, [rsi + 16]            ; restorer
    mov [rbx + TCB_SIG_ACT + rcx + 8], rax
    mov rax, [rsi + 24]            ; sa_mask
    mov [rbx + TCB_SIG_ACT + rcx + 24], rax
    cmp rdi, LINUX_SIGKILL         ; SIGKILL/SIGSTOP can never be caught
    je .sa_uncatchable
    cmp rdi, LINUX_SIGSTOP
    je .sa_uncatchable
.sa_ok:
    xor eax, eax
    ret
.sa_uncatchable:
    mov qword [rbx + TCB_SIG_ACT + rcx + 0], 0
    xor eax, eax
    ret
.sa_einval:
    mov eax, LINUX_EINVAL
    ret
.sa_efault:
    mov eax, LINUX_EFAULT
    ret

; ------------------------------------------------------------------------------
; .l_rt_sigprocmask (Linux #14): RDI = how (1=BLOCK, 2=UNBLOCK, 3=SETMASK),
; RSI = set (user, 0 = no change), RDX = oldset (user, 0 = none).
; ------------------------------------------------------------------------------
.l_rt_sigprocmask:
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rax]
    test rdx, rdx
    jz .spm_no_old
    push rdi
    push rsi
    mov rdi, rdx
    mov esi, 8
    call sys_validate_user_ptr
    pop rsi
    pop rdi
    jc .spm_fault
    mov rax, [rbx + TCB_SIG_BLOCKED]
    mov [rdx], rax                  ; old mask out
.spm_no_old:
    test rsi, rsi
    jz .spm_fix
    push rdi
    push rdx
    push rsi                          ; keep the set pointer: validate takes
    mov rdi, rsi                      ; the size in rsi and clobbers it
    mov esi, 8
    call sys_validate_user_ptr
    pop rsi
    pop rdx
    pop rdi
    jc .spm_fault
    mov rax, [rsi]                  ; new mask (kernel sigset = 8 bytes)
    cmp rdi, 1
    je .spm_block
    cmp rdi, 2
    je .spm_unblock
    cmp rdi, 3
    je .spm_set
    mov eax, LINUX_EINVAL
    ret
.spm_block:
    or [rbx + TCB_SIG_BLOCKED], rax
    jmp .spm_fix
.spm_unblock:
    not rax
    and [rbx + TCB_SIG_BLOCKED], rax
    jmp .spm_fix
.spm_set:
    mov [rbx + TCB_SIG_BLOCKED], rax
.spm_fix:
    ; SIGKILL/SIGSTOP can never be blocked
    mov rax, [rbx + TCB_SIG_BLOCKED]
    and rax, not ((1 shl 8) or (1 shl 18))
    mov [rbx + TCB_SIG_BLOCKED], rax
    xor eax, eax
    ret
.spm_fault:
    mov eax, LINUX_EFAULT
    ret

; ------------------------------------------------------------------------------
; .l_rt_sigreturn (Linux #15): restore the interrupted context from the
; rt_sigframe. The restorer did `ret` into pretcode before the syscall, so the
; user RSP now points 8 bytes above the frame base (Linux: frame = rsp - 8).
; Frame: +0 pretcode, +8 siginfo(128), +136 ucontext, +176 sigcontext(256),
; +432 uc_sigmask, +440 FXSAVE area.
; ------------------------------------------------------------------------------
.l_rt_sigreturn:
    mov rax, [rsp + 120]            ; user RSP (= FR + 8)
    lea rdi, [rax - 8]              ; FR
    cmp rdi, 0x1FE00000             ; inside the task stack slot?
    jb .sigret_die
    cmp rdi, 0x1FFFFE00
    jae .sigret_die
    lea rsi, [rdi + 176]            ; sigcontext
    ; restore GPRs into the syscall frame (H = rsp)
    mov rax, [rsi + 16]
    mov [rsp + 8], rax              ; r10
    mov rax, [rsi + 8]
    mov [rsp + 16], rax             ; r9
    mov rax, [rsi + 0]
    mov [rsp + 24], rax             ; r8
    mov rax, [rsi + 64]
    mov [rsp + 32], rax             ; rdi
    mov rax, [rsi + 72]
    mov [rsp + 40], rax             ; rsi
    mov rax, [rsi + 96]
    mov [rsp + 48], rax             ; rdx
    mov rax, [rsi + 56]
    mov [rsp + 56], rax             ; r15
    mov rax, [rsi + 48]
    mov [rsp + 64], rax             ; r14
    mov rax, [rsi + 40]
    mov [rsp + 72], rax             ; r13
    mov rax, [rsi + 32]
    mov [rsp + 80], rax             ; r12
    mov rax, [rsi + 88]
    mov [rsp + 88], rax             ; rbx
    mov rax, [rsi + 80]
    mov [rsp + 96], rax             ; rbp
    mov rax, [rsi + 136]
    mov [rsp + 104], rax            ; eflags -> r11 slot
    mov rax, [rsi + 128]
    mov [rsp + 112], rax            ; rip
    mov rax, [rsi + 120]
    mov [rsp + 120], rax            ; rsp
    ; FPU/SSE state (fpstate must be our own FXSAVE area — reject tampering)
    mov rax, [rsi + 184]
    lea rcx, [rdi + 440]
    cmp rax, rcx
    jne @f
    fxrstor [rax]
@@:
    ; restore the pre-handler blocked mask (uc_sigmask), KILL/STOP stay clear
    mov rax, [rdi + 432]
    and rax, not ((1 shl 8) or (1 shl 18))
    mov ecx, [r15 + sched_curr_task - kmain]
    imul ecx, TCB_SIZE
    mov [r15 + task_table - kmain + rcx + TCB_SIG_BLOCKED], rax
    mov rax, [rsi + 104]            ; interrupted rax (syscall result)
    ret
.sigret_die:
    mov edi, 0x80000000 or LINUX_SIGSEGV
    call task_exit                  ; never returns
    ret

; ------------------------------------------------------------------------------
; .l_pause (Linux #34): sleep until an actionable signal arrives -> -EINTR
; (delivery runs at syscall return; a lethal signal never returns to user).
; ------------------------------------------------------------------------------
.l_pause:
    call sig_probe_actionable
    test eax, eax
    jnz .pause_eintr
    sti
    mov ecx, 10
    call task_sleep
    cli
    jmp .l_pause
.pause_eintr:
    mov eax, LINUX_EINTR
    ret

; ------------------------------------------------------------------------------
; .l_alarm (Linux #37): RDI = seconds (0 cancels). Returns the whole seconds
; still left on the previous alarm (0 if none). sched_tick counts down the
; millisecond budget in TCB_ALARM_MS and queues SIGALRM when it hits zero;
; alarm state is cleared on spawn/exec and never inherited across fork.
; ------------------------------------------------------------------------------
.l_alarm:
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rax]    ; rbx = current TCB
    mov eax, [rbx + TCB_ALARM_MS]
    xor edx, edx
    mov ecx, 1000
    div ecx                          ; eax = whole seconds left on old alarm
    test edi, edi
    jz .alarm_off
    imul edi, 1000
    mov [rbx + TCB_ALARM_MS], edi
    ret
.alarm_off:
    mov dword [rbx + TCB_ALARM_MS], 0
    ret

; ------------------------------------------------------------------------------
; .l_futex (Linux #202): RDI = uaddr, ESI = op, RDX = val (expected for WAIT,
; nr_wake for WAKE). FUTEX_PRIVATE_FLAG (0x80) is masked off — all futexes are
; process-private here. FUTEX_WAIT sleeps until FUTEX_WAKE or a signal;
; the scheduler tick never wakes futex waiters (see sched_tick .sleep_loop).
; ------------------------------------------------------------------------------
.l_futex:
    push rbx
    push r12
    push r13
    mov r12, rdi                ; r12 = uaddr
    mov r13d, esi               ; r13 = op
    and r13d, 0x7F              ; mask FUTEX_PRIVATE_FLAG / CLOCK_REALTIME
    cmp r13d, 0                 ; FUTEX_WAIT
    je .l_futex_wait
    cmp r13d, 9                 ; FUTEX_WAIT_BITSET
    je .l_futex_wait
    cmp r13d, 1                 ; FUTEX_WAKE
    je .l_futex_wake
    cmp r13d, 10                ; FUTEX_WAKE_BITSET
    je .l_futex_wake
    cmp r13d, 3                 ; FUTEX_REQUEUE
    je .l_futex_requeue
    cmp r13d, 4                 ; FUTEX_CMP_REQUEUE
    je .l_futex_cmp_requeue
    mov rax, LINUX_EINVAL
    jmp .l_futex_done

.l_futex_wait:
    ; RDX = expected value
    mov rdi, r12
    mov esi, 4
    call sys_validate_user_ptr
    jc .l_futex_efault
    mov eax, [r12]
    cmp eax, edx
    jne .l_futex_eagain         ; value changed: -EAGAIN, no sleep
    ; sleep on the futex address (indefinite; timeout arg ignored for now)
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rax]
    mov [rbx + TCB_FUTEX_ADDR], r12
    mov dword [rbx + TCB_SLEEP], 0
    mov dword [rbx + TCB_STATE], TASK_SLEEPING
    int 0x31                    ; yield: scheduler picks another task
    ; woken by FUTEX_WAKE or a signal
    mov qword [rbx + TCB_FUTEX_ADDR], 0
    call sig_probe_actionable
    test eax, eax
    jnz .l_futex_eintr
    xor eax, eax
    jmp .l_futex_done

.l_futex_wake:
    ; RDX = nr_wake
    mov rdi, r12
    mov esi, 4
    call sys_validate_user_ptr
    jc .l_futex_efault
    call futex_wake_addr
    jmp .l_futex_done

.l_futex_cmp_requeue:
    ; R9 = val3 (expected value at uaddr1)
    mov rdi, r12
    mov esi, 4
    call sys_validate_user_ptr
    jc .l_futex_efault
    mov eax, [r12]
    cmp eax, r9d
    jne .l_futex_eagain         ; value changed: -EAGAIN

.l_futex_requeue:
    ; RDI (r12) = uaddr1
    ; RDX = val (max to wake)
    ; R10 = val2 (max to requeue)
    ; R8  = uaddr2 (target futex address)
    mov rdi, r12
    mov esi, 4
    call sys_validate_user_ptr
    jc .l_futex_efault
    mov rdi, r8
    mov esi, 4
    call sys_validate_user_ptr
    jc .l_futex_efault

    push r14
    push rbp
    mov r14d, edx               ; r14d = max wake
    mov ebp, r10d               ; ebp  = max requeue
    mov r13, r8                 ; r13  = uaddr2
    xor ecx, ecx                ; ecx  = woken count
    xor edx, edx                ; edx  = requeued count
    xor ebx, ebx                ; ebx  = task slot
.l_frq_loop:
    cmp ebx, MAX_TASKS
    jae .l_frq_done
    imul eax, ebx, TCB_SIZE
    lea rdi, [r15 + task_table - kmain + rax]
    cmp dword [rdi + TCB_STATE], TASK_SLEEPING
    jne .l_frq_next
    mov rax, [rdi + TCB_FUTEX_ADDR]
    cmp rax, r12                ; waiting on uaddr1?
    jne .l_frq_next

    cmp ecx, r14d
    jae .l_frq_check_requeue
    mov qword [rdi + TCB_FUTEX_ADDR], 0
    mov dword [rdi + TCB_STATE], TASK_READY
    inc ecx
    jmp .l_frq_next

.l_frq_check_requeue:
    cmp edx, ebp
    jae .l_frq_done
    mov [rdi + TCB_FUTEX_ADDR], r13
    inc edx

.l_frq_next:
    inc ebx
    jmp .l_frq_loop

.l_frq_done:
    mov eax, ecx
    pop rbp
    pop r14
    jmp .l_futex_done

.l_futex_eagain:
    mov rax, LINUX_EAGAIN
    jmp .l_futex_done
.l_futex_eintr:
    mov rax, LINUX_EINTR
    jmp .l_futex_done
.l_futex_efault:
    mov rax, LINUX_EFAULT
.l_futex_done:
    pop r13
    pop r12
    pop rbx
    ret

; ------------------------------------------------------------------------------
; .l_pread64 (Linux #17) / .l_pwrite64 (Linux #18): read/write at explicit
; offset without moving the file position. RDI = fd, RSI = buf, RDX = count,
; R10 = offset.
; ------------------------------------------------------------------------------
.l_pread64:
    push rbx
    push r12
    push r13
    mov r12d, edi
    mov r13, rsi
    mov rdi, rsi
    mov rsi, rdx
    call sys_validate_user_ptr
    jc .l_pread_efault
    mov edi, r12d
    call vfs_get_fd_ptr
    test rax, rax
    jz .l_pread_badfd
    mov rbx, rax
    cmp byte [rbx + 0], FD_TYPE_FILE
    jne .l_pread_badfd
    push qword [rbx + 4]              ; save offset
    mov [rbx + 4], r10d               ; set to pread offset
    mov rdi, r12
    mov rsi, r13
    call vfs_read_fd
    pop rcx
    mov [rbx + 4], ecx                ; restore offset
    jmp .l_pread_done
.l_pread_badfd:
    mov rax, LINUX_EBADF
    jmp .l_pread_done
.l_pread_efault:
    mov rax, LINUX_EFAULT
.l_pread_done:
    pop r13
    pop r12
    pop rbx
    ret

.l_pwrite64:
    push rbx
    push r12
    push r13
    mov r12d, edi
    mov r13, rsi
    mov rdi, rsi
    mov rsi, rdx
    call sys_validate_user_ptr
    jc .l_pwrite_efault
    mov edi, r12d
    call vfs_get_fd_ptr
    test rax, rax
    jz .l_pwrite_badfd
    mov rbx, rax
    cmp byte [rbx + 0], FD_TYPE_FILE
    jne .l_pwrite_badfd
    push qword [rbx + 4]
    mov [rbx + 4], r10d
    mov rdi, r12
    mov rsi, r13
    call vfs_write_fd
    pop rcx
    mov [rbx + 4], ecx
    jmp .l_pwrite_done
.l_pwrite_badfd:
    mov rax, LINUX_EBADF
    jmp .l_pwrite_done
.l_pwrite_efault:
    mov rax, LINUX_EFAULT
.l_pwrite_done:
    pop r13
    pop r12
    pop rbx
    ret

; ------------------------------------------------------------------------------
; .l_pipe (Linux #22): RDI = int pipefd[2] (user). Returns 0 or -errno.
; ------------------------------------------------------------------------------
.l_pipe:
    mov rdi, rdi
    call vfs_pipe
    cmp rax, -1
    jne .l_pipe_ok
    mov rax, LINUX_EMFILE            ; no free pipes/fds
    ret
.l_pipe_ok:
    xor eax, eax
    ret

; ------------------------------------------------------------------------------
; .l_dup (Linux #32): RDI = oldfd. Returns new fd or -errno.
; ------------------------------------------------------------------------------
.l_dup:
    mov edi, edi
    call vfs_dup
    ret

; ------------------------------------------------------------------------------
; .l_fcntl (Linux #72): RDI = fd, ESI = cmd, EDX = arg.
;   F_DUPFD(0) -> dup to first free fd >= arg
;   F_GETFD(1) -> 0 (no per-fd flags tracked)
;   F_SETFD(2) -> 0 (accepted, not tracked)
;   F_GETFL(3) -> file status flags from fd_entry
;   F_SETFL(4) -> update file status flags
; ------------------------------------------------------------------------------
.l_fcntl:
    cmp esi, 0                       ; F_DUPFD
    je .l_fcntl_dupfd
    cmp esi, 1                       ; F_GETFD
    je .l_fcntl_getfd
    cmp esi, 2                       ; F_SETFD
    je .l_fcntl_setfd
    cmp esi, 3                       ; F_GETFL
    je .l_fcntl_getfl
    cmp esi, 4                       ; F_SETFL
    je .l_fcntl_setfl
    cmp esi, 5                       ; F_GETLK
    je .l_fcntl_getlk
    cmp esi, 6                       ; F_SETLK
    je .l_fcntl_setlk
    cmp esi, 7                       ; F_SETLKW
    je .l_fcntl_setlk
    cmp esi, 1024                    ; F_DUPFD_CLOEXEC
    je .l_fcntl_dupfd
    mov rax, LINUX_EINVAL
    ret

.l_flock:
    ; RDI = fd, ESI = operation (LOCK_SH=1, LOCK_EX=2, LOCK_NB=4, LOCK_UN=8)
    call vfs_get_fd_ptr
    test rax, rax
    jz .l_flock_badfd
    cmp byte [rax], FD_TYPE_FILE
    jne .l_flock_badfd
    movzx r8d, word [rax + 2]        ; inode
    test r8d, r8d
    jz .l_flock_badfd
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    mov r9d, [r15 + task_table - kmain + rax + TCB_TGID] ; current tgid
    test esi, 8                      ; LOCK_UN?
    jnz .l_flock_unlock
    ; Lock: find existing entry for this inode or first free
    lea rax, [r15 + inode_lock_table - kmain]
    xor ecx, ecx
.l_flock_search:
    cmp ecx, MAX_INODE_LOCKS * INODE_LOCK_ENTRY_SIZE
    jae .l_flock_find_free
    cmp [rax + rcx + 0], r8d
    je .l_flock_have_entry
    add ecx, INODE_LOCK_ENTRY_SIZE
    jmp .l_flock_search
.l_flock_find_free:
    xor ecx, ecx
.l_flock_free_loop:
    cmp ecx, MAX_INODE_LOCKS * INODE_LOCK_ENTRY_SIZE
    jae .l_flock_enolck
    cmp dword [rax + rcx + 0], 0
    je .l_flock_have_entry
    add ecx, INODE_LOCK_ENTRY_SIZE
    jmp .l_flock_free_loop
.l_flock_have_entry:
    mov [rax + rcx + 0], r8d
    mov [rax + rcx + 4], r9d
    mov dword [rax + rcx + 8], 2 ; exclusive lock
    xor eax, eax
    ret
.l_flock_enolck:
    mov rax, LINUX_ENOLCK
    ret
.l_flock_unlock:
    lea rax, [r15 + inode_lock_table - kmain]
    xor ecx, ecx
.l_flock_un_loop:
    cmp ecx, MAX_INODE_LOCKS * INODE_LOCK_ENTRY_SIZE
    jae .l_flock_un_done
    cmp [rax + rcx + 0], r8d
    jne .l_flock_un_next
    cmp [rax + rcx + 4], r9d
    jne .l_flock_un_next
    mov dword [rax + rcx + 0], 0
    mov dword [rax + rcx + 4], 0
    mov dword [rax + rcx + 8], 0
.l_flock_un_next:
    add ecx, INODE_LOCK_ENTRY_SIZE
    jmp .l_flock_un_loop
.l_flock_un_done:
    xor eax, eax
    ret
.l_flock_badfd:
    mov rax, LINUX_EBADF
    ret

.l_fcntl_getlk:
    ; RDI = fd, RSI = cmd, RDX = flock struct pointer
    test rdx, rdx
    jz .l_fcntl_badfd2
    ; Report unlocked (F_UNLCK = 2 in Linux struct flock)
    mov word [rdx + 0], 2            ; l_type = F_UNLCK
    xor eax, eax
    ret

.l_fcntl_setlk:
    ; RDI = fd, RSI = cmd, RDX = flock struct pointer
    test rdx, rdx
    jz .l_fcntl_badfd2
    call vfs_get_fd_ptr
    test rax, rax
    jz .l_fcntl_badfd2
    cmp byte [rax], FD_TYPE_FILE
    jne .l_fcntl_badfd2
    movzx r8d, word [rax + 2]        ; inode
    test r8d, r8d
    jz .l_fcntl_badfd2
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    mov r9d, [r15 + task_table - kmain + rax + TCB_TGID] ; current tgid
    movzx ecx, word [rdx + 0]        ; l_type: 0=F_RDLCK, 1=F_WRLCK, 2=F_UNLCK
    cmp ecx, 2                       ; F_UNLCK?
    je .l_fcntl_unlock
    ; Lock: find existing entry for this inode or free slot
    lea rax, [r15 + inode_lock_table - kmain]
    xor edx, edx
.l_fsetlk_search:
    cmp edx, MAX_INODE_LOCKS * INODE_LOCK_ENTRY_SIZE
    jae .l_fsetlk_find_free
    cmp [rax + rdx + 0], r8d
    je .l_fsetlk_have_entry
    add edx, INODE_LOCK_ENTRY_SIZE
    jmp .l_fsetlk_search
.l_fsetlk_find_free:
    xor edx, edx
.l_fsetlk_free_loop:
    cmp edx, MAX_INODE_LOCKS * INODE_LOCK_ENTRY_SIZE
    jae .l_fsetlk_done
    cmp dword [rax + rdx + 0], 0
    je .l_fsetlk_have_entry
    add edx, INODE_LOCK_ENTRY_SIZE
    jmp .l_fsetlk_free_loop
.l_fsetlk_have_entry:
    mov [rax + rdx + 0], r8d
    mov [rax + rdx + 4], r9d
    mov [rax + rdx + 8], ecx
.l_fsetlk_done:
    xor eax, eax
    ret
.l_fcntl_unlock:
    lea rax, [r15 + inode_lock_table - kmain]
    xor edx, edx
.l_funlock_loop:
    cmp edx, MAX_INODE_LOCKS * INODE_LOCK_ENTRY_SIZE
    jae .l_funlock_done
    cmp [rax + rdx + 0], r8d
    jne .l_funlock_next
    cmp [rax + rdx + 4], r9d
    jne .l_funlock_next
    mov dword [rax + rdx + 0], 0
    mov dword [rax + rdx + 4], 0
    mov dword [rax + rdx + 8], 0
.l_funlock_next:
    add edx, INODE_LOCK_ENTRY_SIZE
    jmp .l_funlock_loop
.l_funlock_done:
    xor eax, eax
    ret
.l_fcntl_dupfd:
    ; duplicate oldfd to first free fd >= arg
    push rbx
    push r12
    push r13
    mov r12d, edi                    ; oldfd
    mov r13d, edx                    ; arg (min fd)
    call vfs_get_fd_ptr
    test rax, rax
    jz .l_fcntl_badfd
    mov rbx, rax
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, eax, (MAX_FDS_PER_TASK * FD_ENTRY_SIZE)
    lea rax, [r15 + task_fd_tables - kmain + rax]
    mov ecx, r13d
    cmp ecx, 3
    jae @f
    mov ecx, 3
@@:
.l_fcntl_dup_find:
    cmp ecx, MAX_FDS_PER_TASK
    jae .l_fcntl_badfd
    imul edx, ecx, FD_ENTRY_SIZE
    cmp byte [rax + rdx + 0], FD_TYPE_UNUSED
    je .l_fcntl_dup_found
    inc ecx
    jmp .l_fcntl_dup_find
.l_fcntl_dup_found:
    imul edx, ecx, FD_ENTRY_SIZE
    lea rdi, [rax + rdx]
    mov rsi, rbx
    mov ecx, FD_ENTRY_SIZE / 8
    rep movsq
    cmp byte [rdi - FD_ENTRY_SIZE + 0], FD_TYPE_PIPE
    jne .l_fcntl_dup_done
    movzx eax, word [rdi - FD_ENTRY_SIZE + 2]
    imul eax, eax, PIPE_STRUCT_SIZE
    lea rdx, [r15 + pipe_table - kmain + rax]
    cmp byte [rdi - FD_ENTRY_SIZE + 1], O_RDONLY
    jne .l_fcntl_dup_wr
    inc word [rdx + 12]
    jmp .l_fcntl_dup_done
.l_fcntl_dup_wr:
    inc word [rdx + 14]
.l_fcntl_dup_done:
    mov eax, ecx                     ; return new fd
    pop r13
    pop r12
    pop rbx
    ret
.l_fcntl_badfd:
    pop r13
    pop r12
    pop rbx
    mov rax, LINUX_EBADF
    ret
.l_fcntl_getfd:
    xor eax, eax
    ret
.l_fcntl_setfd:
    xor eax, eax
    ret
.l_fcntl_getfl:
    call vfs_get_fd_ptr
    test rax, rax
    jz .l_fcntl_badfd2
    movzx eax, byte [rax + 1]        ; flags from fd_entry
    ret
.l_fcntl_setfl:
    call vfs_get_fd_ptr
    test rax, rax
    jz .l_fcntl_badfd2
    mov [rax + 1], dl                ; update flags
    xor eax, eax
    ret
.l_fcntl_badfd2:
    mov rax, LINUX_EBADF
    ret

; ------------------------------------------------------------------------------
; .l_stat (Linux #4) / .l_lstat (Linux #6) / .l_newfstatat (Linux #262):
;   stat:    RDI = pathname, RSI = statbuf
;   lstat:   same (symlinks not supported yet, so identical to stat)
;   newfstatat: RDI = dirfd (ignored), RSI = pathname, RDX = statbuf,
;              R10 = flags (ignored)
; .l_fstat (Linux #5): RDI = fd, RSI = statbuf
; ------------------------------------------------------------------------------
.l_stat:
    call vfs_stat_path
    ret
.l_lstat:
    call vfs_stat_path
    ret
.l_newfstatat:
    ; RDI = dirfd (ignored), RSI = pathname, RDX = statbuf, R10 = flags
    mov rdi, rsi                     ; pathname
    mov rsi, rdx                     ; statbuf
    call vfs_stat_path
    ret
.l_fstat:
    call vfs_stat_fd
    ret

; ------------------------------------------------------------------------------
; .l_getdents64 (Linux #217): RDI = fd, RSI = buf, RDX = count.
; Reads directory entries as struct linux_dirent64. Returns bytes written
; or -errno.
; ------------------------------------------------------------------------------
.l_getdents64:
    push rbx
    push r12
    push r13
    push rdx                ; [rsp] = count
    mov r12d, edi           ; r12d = fd
    mov r13, rsi            ; r13 = buf
    mov rdi, rsi
    mov rsi, rdx
    call sys_validate_user_ptr
    jc .l_gd_efault
    mov edi, r12d           ; fd
    call vfs_get_fd_ptr
    test rax, rax
    jz .l_gd_badfd
    mov rbx, rax
    cmp byte [rbx + 0], FD_TYPE_FILE
    jne .l_gd_badfd
    movzx eax, word [rbx + 2]
    call ext4_inode_load
    jc .l_gd_efault
    movzx eax, word [r15 + ext4_inode - kmain + 0]
    and eax, 0xF000
    cmp eax, 0x4000         ; S_IFDIR
    jne .l_gd_notdir
    mov r8d, [rbx + 4]      ; starting offset
    movzx eax, word [rbx + 2]  ; inode
    mov rdi, r13            ; buf
    mov rsi, [rsp]          ; count
    mov rdx, r8             ; starting offset
    call ext4_read_dir
    cmp rax, -2
    je .l_gd_einval
    cmp rax, -1
    je .l_gd_err
    mov [rbx + 4], edx      ; advance fd offset to final directory offset
    jmp .l_gd_done
.l_gd_einval:
    mov rax, LINUX_EINVAL
    jmp .l_gd_done
.l_gd_badfd:
    mov rax, LINUX_EBADF
    jmp .l_gd_done
.l_gd_notdir:
    mov rax, LINUX_ENOTDIR
    jmp .l_gd_done
.l_gd_efault:
    mov rax, LINUX_EFAULT
    jmp .l_gd_done
.l_gd_err:
    mov rax, LINUX_EIO
    jmp .l_gd_done
.l_gd_inoderr:
    mov rax, LINUX_EIO
    jmp .l_gd_done
.l_gd_done:
    add rsp, 8              ; pop count
    pop r13
    pop r12
    pop rbx
    ret

; ------------------------------------------------------------------------------
; .l_tgkill (Linux #234) / .l_kill (Linux #62):
;   tgkill: RDI = tgid (ignored — one thread group per task here),
;           RSI = tid, RDX = sig, falls through into kill with tid in RDI.
;   kill:   RDI = pid (> 0 only — no process groups yet), RSI = sig
;           (0 = existence check).
; ------------------------------------------------------------------------------
.l_tgkill:
    mov rdi, rsi                     ; tid
    mov rsi, rdx                     ; sig
.l_kill:
    test rdi, rdi
    jle .kill_esrch
    cmp rdi, MAX_TASKS
    jae .kill_esrch
    test rsi, rsi
    jz .kill_exist
    lea edi, [rdi - 1]               ; pid -> slot
    mov edx, [r15 + sched_curr_task - kmain]
    inc edx                          ; si_pid = caller pid
    call sig_send_to_task
    ret
.kill_exist:
    mov eax, edi
    dec eax
    cmp eax, MAX_TASKS
    jae .kill_esrch
    imul eax, eax, TCB_SIZE
    cmp dword [r15 + task_table - kmain + rax + TCB_STATE], TASK_UNUSED
    je .kill_esrch
    xor eax, eax
    ret
.kill_esrch:
    mov eax, LINUX_ESRCH
    ret

; ------------------------------------------------------------------------------
; Job control (Tier A stubs): every task is its own process group and session
; leader until real pgids/sessions land. setpgid always "succeeds", getpgid/
; getsid validate existence, setsid reports the task's own pid.
; ------------------------------------------------------------------------------
.l_setpgid:
    xor eax, eax
    ret
.l_getpgrp:
.l_setsid:
    mov eax, [r15 + sched_curr_task - kmain]
    inc eax
    ret
.l_getpgid:
.l_getsid:
    test rdi, rdi
    jz .gps_self
    js .gps_esrch
    mov eax, edi
    dec eax
    cmp eax, MAX_TASKS
    jae .gps_esrch
    imul eax, eax, TCB_SIZE
    cmp dword [r15 + task_table - kmain + rax + TCB_STATE], TASK_UNUSED
    je .gps_esrch
    mov eax, edi                     ; pid is its own pgid/sid
    ret
.gps_self:
    mov eax, [r15 + sched_curr_task - kmain]
    inc eax
    ret
.gps_esrch:
    mov eax, LINUX_ESRCH
    ret

; ------------------------------------------------------------------------------
; .l_execve: replace the current process image (Linux #59).
;   RDI = pathname (user), RSI = argv (user array), RDX = envp — IGNORED
;   (the loader provides its fixed PATH/HOME/USER/TERM/SHELL environment).
; The pathname and argv strings are staged in KERNEL memory first — the old
; image is about to be overwritten. The ext4 loader then loads into the RUNNING
; slot (exec mode: same pid, fds, parent and page tables), and on success the
; syscall return frame is patched so sysretq lands at the new entry point with
; a fresh stack. On any failure the old image and frame stay untouched.
; ------------------------------------------------------------------------------
.l_execve:
    lea rbx, [rsp]                    ; syscall frame base (callee-saved)
    mov r12, rdi                      ; pathname (user pointer)
    mov r13, rsi                      ; argv array (user pointer or 0)

    ; Validate + copy the pathname into a kernel buffer (max 63 chars)
    test r12, r12
    jz .l_execve_fault
    mov rdi, r12
    mov rsi, 128
    call sys_validate_user_ptr        ; preserves all regs except RAX
    jc .l_execve_fault
    lea rdi, [r15 + exec_path_buf - kmain]
    mov rsi, r12
    mov ecx, 63
.l_execve_cp:
    lodsb
    stosb
    test al, al
    jz .l_execve_cp_done
    dec ecx
    jnz .l_execve_cp
    mov byte [rdi], 0
.l_execve_cp_done:

    ; Stage argv into kernel memory BEFORE the image is replaced (the strings
    ; may live in the very .rodata/.text the new segments are about to wipe)
    mov qword [r15 + sys_spawn_argv_ptr - kmain], 0     ; 0 -> loader fallback
    test r13, r13
    jz .l_execve_argv_done
    mov rdi, r13
    mov rsi, 16 * 8
    call sys_validate_user_ptr
    jc .l_execve_argv_done
    xor ecx, ecx                      ; arg index
.l_execve_arg_loop:
    cmp ecx, 16
    jae .l_execve_arg_stop
    mov rsi, [r13 + rcx*8]
    test rsi, rsi
    jz .l_execve_arg_stop
    mov rdi, rsi
    mov rsi, 128
    call sys_validate_user_ptr
    jc .l_execve_arg_stop
    mov rax, rcx                      ; dest = exec_argv_stage + idx*128
    shl rax, 7
    lea rdi, [r15 + exec_argv_stage - kmain]
    add rdi, rax
    mov rsi, [r13 + rcx*8]
    mov edx, 127
.l_execve_str:
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    test al, al
    jz .l_execve_str_done
    dec edx
    jnz .l_execve_str
    mov byte [rdi - 1], 0
.l_execve_str_done:
    inc ecx
    jmp .l_execve_arg_loop

.l_execve_arg_stop:
    test ecx, ecx
    jz .l_execve_argv_done
    xor eax, eax                      ; exec_argv_ptrs[i] = stage + i*128
.l_execve_ptr_loop:
    cmp eax, ecx
    jae .l_execve_ptr_done
    mov rsi, rax
    shl rsi, 7
    lea rdi, [r15 + exec_argv_stage - kmain]
    add rsi, rdi
    mov [r15 + exec_argv_ptrs - kmain + rax*8], rsi
    inc rax
    jmp .l_execve_ptr_loop
.l_execve_ptr_done:
    mov qword [r15 + exec_argv_ptrs - kmain + rax*8], 0
    lea rax, [r15 + exec_argv_ptrs - kmain]
    mov [r15 + sys_spawn_argv_ptr - kmain], rax
.l_execve_argv_done:

    ; Load the new image into the RUNNING slot
    mov qword [r15 + sys_spawn_exec_mode - kmain], 1
    lea rsi, [r15 + exec_path_buf - kmain]
    call elf_load_from_ext4           ; EAX = slot (ok) or ELF_ERR_* (negative)
    mov qword [r15 + sys_spawn_exec_mode - kmain], 0
    mov qword [r15 + sys_spawn_argv_ptr - kmain], 0
    test eax, eax
    js .l_execve_errmap

    ; Success: point the syscall return frame at the new program
    mov rax, [r15 + sys_spawn_exec_entry - kmain]
    mov [rbx + 112], rax              ; user RIP
    mov qword [rbx + 120], 0x1FFFFE00 ; user RSP (argc/argv/envp/auxv vector)
    xor eax, eax
    ret

.l_execve_errmap:
    cmp eax, ELF_ERR_NOT_FOUND
    je .l_execve_enoent
    cmp eax, ELF_ERR_READ_FAIL
    je .l_execve_eio
    cmp eax, ELF_ERR_OOM
    je .l_execve_enomem
    cmp eax, ELF_ERR_SPAWN_FAIL
    je .l_execve_eagain
    mov rax, LINUX_ENOEXEC            ; header / segment errors
    ret
.l_execve_enoent:
    mov rax, LINUX_ENOENT
    ret
.l_execve_eio:
    mov rax, LINUX_EIO
    ret
.l_execve_enomem:
    mov rax, LINUX_ENOMEM
    ret
.l_execve_eagain:
    mov rax, LINUX_EAGAIN
    ret
.l_execve_fault:
    mov rax, LINUX_EFAULT
    ret

.l_uname:
    ; RDI = struct utsname pointer (65 bytes per field: sysname, nodename, release, version, machine)
    test rdi, rdi
    jz .l_uname_err
    push rdi
    push rcx
    push rax
    ; C-L3: validate the full 390-byte output range before writing
    mov rsi, 390
    call sys_validate_user_ptr
    jc .l_uname_bad
    ; Zero out 390 bytes (6 * 65)
    mov rcx, 390 / 8
    xor eax, eax
    rep stosq
    pop rax
    pop rcx
    pop rdi

    ; sysname: "Linux"
    mov dword [rdi + 0], 'Linu'
    mov byte  [rdi + 4], 'x'
    ; nodename: "opensweet"
    mov dword [rdi + 65], 'open'
    mov dword [rdi + 69], 'swee'
    mov byte  [rdi + 73], 't'
    ; release: "6.1.0-opensweet"
    mov dword [rdi + 130], '6.1.'
    mov dword [rdi + 134], '0-op'
    mov dword [rdi + 138], 'ensw'
    mov word  [rdi + 142], 'ee'
    mov byte  [rdi + 144], 't'
    ; machine: "x86_64"
    mov dword [rdi + 260], 'x86_'
    mov word  [rdi + 264], '64'
    xor eax, eax
    ret
.l_uname_bad:
    pop rax
    pop rcx
    pop rdi
    jmp .l_uname_err
.l_uname_err:
    mov rax, LINUX_EFAULT
    ret

.l_getcwd:
    ; RDI = buf, RSI = size
    call sys_handler_getcwd
    ret

.l_chdir:
    ; RDI = path
    call sys_handler_chdir
    ret

.l_mkdir:
    ; RDI = path, RSI = mode
    call sys_handler_mkdir
    test eax, eax
    jz @f
    mov rax, LINUX_EACCES
    ret
@@:
    xor eax, eax
    ret

.l_mkdirat:
    ; RDI = dirfd, RSI = path, RDX = mode
    mov rdi, rsi
    mov rsi, rdx
    jmp .l_mkdir

.l_rmdir:
    ; RDI = path
    call sys_handler_rmdir
    test eax, eax
    jz @f
    mov rax, LINUX_ENOENT
    ret
@@:
    xor eax, eax
    ret

.l_unlink:
    ; RDI = path
    call sys_handler_unlink
    test eax, eax
    jz @f
    mov rax, LINUX_ENOENT
    ret
@@:
    xor eax, eax
    ret

.l_unlinkat:
    ; RDI = dirfd, RSI = pathname, RDX = flags (AT_REMOVEDIR = 0x200)
    test edx, 0x200
    jnz .l_unlinkat_rmdir
    mov rdi, rsi
    jmp .l_unlink
.l_unlinkat_rmdir:
    mov rdi, rsi
    jmp .l_rmdir

.l_symlink:
    ; RDI = target, RSI = linkpath
    push rdi
    push rsi
    mov rdi, rdi
    mov rsi, 64
    call sys_validate_user_ptr
    pop rsi
    pop rdi
    jc .l_symlink_fault
    push rdi
    push rsi
    mov rdi, rsi
    mov rsi, 64
    call sys_validate_user_ptr
    pop rsi
    pop rdi
    jc .l_symlink_fault
    ; RDI = target, RSI = linkpath -> ext4_symlink expects RSI=symlink_path, RDX=target
    mov rdx, rdi
    ; RSI already has linkpath
    call ext4_symlink
    cmp rax, -1
    je .l_symlink_err
    xor eax, eax
    ret
.l_symlink_fault:
    mov rax, LINUX_EFAULT
    ret
.l_symlink_err:
    mov rax, LINUX_EEXIST
    ret

.l_symlinkat:
    ; RDI = target, RSI = newdirfd, RDX = linkpath
    mov rdi, rdi                     ; target
    mov rsi, rdx                     ; linkpath
    jmp .l_symlink

.l_link:
    ; RDI = oldpath, RSI = newpath
    push rdi
    push rsi
    mov rdi, rdi
    mov rsi, 64
    call sys_validate_user_ptr
    pop rsi
    pop rdi
    jc .l_link_fault
    push rdi
    push rsi
    mov rdi, rsi
    mov rsi, 64
    call sys_validate_user_ptr
    pop rsi
    pop rdi
    jc .l_link_fault
    ; 1. Lookup oldpath to get target inode
    push rsi                         ; newpath
    mov rsi, rdi
    call vfs_lookup_with_fallback
    pop rsi                          ; newpath
    test eax, eax
    jz .l_link_noent
    ; Ensure oldpath is NOT a directory (hard links to directories forbidden)
    push rax                         ; target inode
    push rsi                         ; newpath
    call ext4_inode_load
    pop rsi
    pop r12                          ; r12 = target inode
    jc .l_link_err
    movzx edx, word [r15 + ext4_inode - kmain + IN_OFF_MODE]
    and edx, 0xF000
    cmp edx, 0x4000                  ; S_IFDIR?
    je .l_link_eperm                 ; EPERM: hard link to dir forbidden!
    ; 2. Check newpath does not exist
    push r12
    push rsi
    call ext4_lookup
    pop rsi                          ; newpath
    pop r12                          ; target inode
    test eax, eax
    jnz .l_link_exist
    ; 3. Resolve parent of newpath
    sub rsp, 288                     ; [rsp] = basename buffer
    push r12                         ; target inode
    lea rdi, [rsp + 8]               ; basename dest
    call ext4_resolve_parent
    pop r12                          ; target inode
    jc .l_link_err_rsp
    mov r8d, eax                     ; r8d = parent inode
    ; 4. Add dir entry in parent dir for newpath pointing to r12d
    mov eax, r8d
    mov edx, r12d
    lea rsi, [rsp]                   ; basename
    mov cl, 1                        ; type = file
    call ext4_add_dir_entry
    jc .l_link_err_rsp
    ; 5. Increment i_links_count on target inode
    mov eax, r12d
    call ext4_inode_load
    jc .l_link_done_flush
    inc word [r15 + ext4_inode - kmain + 26] ; i_links_count++
    mov eax, r12d
    call ext4_inode_save
.l_link_done_flush:
    call disk_flush_cache
    add rsp, 288
    xor eax, eax
    ret
.l_link_err_rsp:
    add rsp, 288
.l_link_err:
    mov rax, LINUX_EIO
    ret
.l_link_eperm:
    mov rax, LINUX_EPERM
    ret
.l_link_noent:
    mov rax, LINUX_ENOENT
    ret
.l_link_exist:
    mov rax, LINUX_EEXIST
    ret
.l_link_fault:
    mov rax, LINUX_EFAULT
    ret

.l_renameat2:
    ; RDI = olddirfd, RSI = oldpath, RDX = newdirfd, R10 = newpath, R8 = flags
    test r8d, r8d
    jnz .l_rename_einval
    mov rdi, rsi                     ; oldpath
    mov rsi, r10                     ; newpath
    jmp .l_rename

.l_renameat:
    ; RDI = olddirfd, RSI = oldpath, RDX = newdirfd, R10 = newpath
    mov rdi, rsi
    mov rsi, r10
    jmp .l_rename

.l_rename:
    ; RDI = oldpath, RSI = newpath
    push rdi
    push rsi
    mov rdi, rdi
    mov rsi, 64
    call sys_validate_user_ptr
    pop rsi
    pop rdi
    jc .l_rename_fault
    push rdi
    push rsi
    mov rdi, rsi
    mov rsi, 64
    call sys_validate_user_ptr
    pop rsi
    pop rdi
    jc .l_rename_fault

    ; 1. Lookup oldpath to get source inode (do not follow symlink: we want to rename the symlink itself!)
    push rsi                         ; newpath
    mov rsi, rdi
    call ext4_lookup_nofollow
    pop rsi                          ; newpath
    test eax, eax
    jz .l_rename_noent
    mov r12d, eax                    ; r12d = source inode

    ; 2. Lookup newpath to see if target already exists
    push r12                         ; source inode
    push rsi                         ; newpath
    mov rsi, rsi
    call ext4_lookup
    pop rsi                          ; newpath
    pop r12                          ; source inode
    test eax, eax
    jz .l_rename_no_target

    ; Target exists (in EAX): remove target directory entry and decrement link count
    mov r13d, eax                    ; r13d = existing target inode
    push r13
    push r12
    push rsi
    push rdi
    sub rsp, 256                     ; [rsp] = basename buffer (256 bytes)
    lea rdi, [rsp]                   ; dest buffer
    ; RSI already has newpath!
    call ext4_resolve_parent
    add rsp, 256
    pop rdi                          ; oldpath
    pop rsi                          ; newpath
    pop r12                          ; source inode
    pop r13                          ; target inode
    jc .l_rename_err
    mov r8d, eax                     ; r8d = target parent dir
    ; Remove old entry from target parent
    mov eax, r8d
    mov edx, r13d
    call ext4_remove_dir_entry
    ; Decrement target i_links_count
    mov eax, r13d
    call ext4_inode_load
    jc @f
    dec word [r15 + ext4_inode - kmain + 26]
    mov eax, r13d
    call ext4_inode_save
@@:

.l_rename_no_target:
    ; 3. Add directory entry in newpath's parent directory pointing to source inode (r12d)
    push r12
    push rdi                         ; oldpath
    push rsi                         ; newpath
    sub rsp, 256                     ; [rsp] = basename buffer
    lea rdi, [rsp]                   ; dest buffer
    ; RSI = newpath
    call ext4_resolve_parent
    jc .l_rename_step3_err
    mov r8d, eax                     ; new parent inode
    mov eax, r8d
    mov edx, r12d
    lea rsi, [rsp]                   ; basename of newpath
    mov cl, 1                        ; file type
    call ext4_add_dir_entry
    jc .l_rename_step3_err
    add rsp, 256
    pop rsi                          ; newpath
    pop rdi                          ; oldpath
    pop r12                          ; source inode

    ; 4. Remove entry for oldpath from old parent directory
    push r12
    push rdi
    push rsi
    sub rsp, 256
    mov rsi, rdi                     ; oldpath
    lea rdi, [rsp]                   ; basename
    call ext4_resolve_parent
    jc .l_rename_step4_err
    mov r8d, eax                     ; old parent inode
    mov eax, r8d
    mov edx, r12d
    call ext4_remove_dir_entry
    add rsp, 256
    pop rsi
    pop rdi
    pop r12

    call disk_flush_cache
    xor eax, eax
    ret

.l_rename_step3_err:
    add rsp, 256
    pop rsi
    pop rdi
    pop r12
    jmp .l_rename_err

.l_rename_step4_err:
    add rsp, 256
    pop rsi
    pop rdi
    pop r12
    jmp .l_rename_err
.l_rename_err:
    mov rax, LINUX_EIO
    ret
.l_rename_einval:
    mov rax, LINUX_EINVAL
    ret
.l_rename_noent:
    mov rax, LINUX_ENOENT
    ret
.l_rename_fault:
    mov rax, LINUX_EFAULT
    ret

.l_linkat:
    ; RDI = olddirfd, RSI = oldpath, RDX = newdirfd, R10 = newpath, R8 = flags
    mov rdi, rsi
    mov rsi, r10
    jmp .l_link

.l_arch_prctl:
    ; RDI = code (ARCH_SET_FS = 0x1002, ARCH_GET_FS = 0x1003)
    ; RSI = address
    cmp edi, 0x1002             ; ARCH_SET_FS
    je .l_set_fs
    cmp edi, 0x1003             ; ARCH_GET_FS
    je .l_get_fs
    mov rax, LINUX_EINVAL
    ret

.l_set_fs:
    ; Update current task's TCB_FS_BASE and write MSR 0xC0000100
    push rdx
    push rcx
    push rbx
    push rax
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rax]
    mov [rbx + TCB_FS_BASE], rsi
    pop rax
    pop rbx

    mov rax, rsi
    mov rdx, rsi
    shr rdx, 32
    mov ecx, 0xC0000100         ; IA32_FS_BASE
    wrmsr
    pop rcx
    pop rdx
    xor eax, eax
    ret

.l_get_fs:
    ; C-L4: validate the 8-byte out-pointer before writing
    test rsi, rsi
    jz .l_uname_err
    push rdx
    push rcx
    push rsi                       ; save user address
    mov rdi, rsi
    mov rsi, 8
    call sys_validate_user_ptr
    jc .l_get_fs_bad
    mov ecx, 0xC0000100
    rdmsr
    shl rdx, 32
    or rax, rdx
    pop rdi                        ; user address
    mov [rdi], rax
    pop rcx
    pop rdx
    xor eax, eax
    ret
.l_get_fs_bad:
    pop rsi
    pop rcx
    pop rdx
    jmp .l_uname_err


; Global allocator pointer for anonymous mmap (0x20000000 = 512MB)

.l_mprotect:
    ; Mock mprotect: always succeed
    xor eax, eax
    ret

.l_mmap:
    ; Linux x86_64 mmap(addr=RDI, length=RSI, prot=RDX, flags=R10, fd=R8, offset=R9)
    ; Validate length
    test rsi, rsi
    jz .l_mmap_einval
    cmp rsi, 0x20000000             ; C-L1: cap length at the zone size (512MB)
    ja .l_mmap_einval

    ; Save parameters on stack to preserve across page loop and vmm_map calls
    push r9                             ; [rsp + 56] = offset
    push r8                             ; [rsp + 48] = fd
    push r10                            ; [rsp + 40] = flags
    push rsi                            ; [rsp + 32] = original length
    push r12                            ; [rsp + 24]
    push r13                            ; [rsp + 16]
    push r14                            ; [rsp + 8]
    push r15                            ; [rsp + 0]
    push rbx                            ; [rsp - 8] -> after push rbx: [rsp + 0] = rbx, etc.

    xor r14d, r14d                      ; r14 = bump-mode flag (addr == 0)
    test rdi, rdi
    jnz .l_mmap_base_ok
    mov rdi, [r15 + linux_mmap_next - kmain]
    inc r14d
.l_mmap_base_ok:
    ; Align addr to 4KB
    and rdi, -4096
    ; C-L1: only the managed Linux zone [0x20000000, 0x40000000) may be mapped.
    ; Rejects kernel/identity addresses, code/stack/heap slots and zone overflow.
    cmp rdi, 0x20000000
    jb .l_mmap_einval_p
    mov rax, rsi
    add rax, 0xFFF
    and rax, -4096                  ; page-rounded length
    add rax, rdi                    ; base + rounded length
    jc .l_mmap_einval_p             ; addr + len overflow
    cmp rax, 0x40000000
    ja .l_mmap_einval_p             ; runs past the zone end

    mov rbx, rdi                ; rbx = original virt base address
    mov r12, rdi                ; r12 = current virt addr
    mov r13, rsi                ; r13 = remaining length
    mov r15, 0xFFFF800000010000

.l_mmap_page_loop:
    test r13, r13
    jz .l_mmap_done

    ; If page already mapped (e.g. MAP_FIXED over reserved mapping), unmap and free old phys page
    mov rdi, r12
    call vmm_unmap                      ; RAX = existing phys page (0 if none)
    test rax, rax
    jz @f
    cmp rax, 0x10000000                 ; only PMM bitmap range
    jae @f
    mov rdi, rax
    call pmm_free
@@:
    ; Allocate zeroed physical page
    call pmm_alloc_zero
    test rax, rax
    jz .l_mmap_enomem

    ; Map virtual page r12 to physical page rax
    ; Flags: Present (1) | Writable (2) | User (4) = 7
    mov rdi, r12
    mov r11b, 7
    call vmm_map

    add r12, 4096
    sub r13, 4096
    ja .l_mmap_page_loop

.l_mmap_done:
    ; Saved on stack:
    ;   [rsp + 0]  = rbx
    ;   [rsp + 8]  = r15
    ;   [rsp + 16] = r14
    ;   [rsp + 24] = r13
    ;   [rsp + 32] = r12
    ;   [rsp + 40] = original length (rsi)
    ;   [rsp + 48] = flags (r10)
    ;   [rsp + 56] = fd (r8)
    ;   [rsp + 64] = offset (r9)

    ; Check if file-backed mapping: fd (in [rsp + 56]) >= 0 and not anonymous
    ; Linux MAP_ANONYMOUS is 0x20. If fd != -1 and not MAP_ANON:
    test dword [rsp + 48], 0x20         ; MAP_ANONYMOUS?
    jnz .l_mmap_commit
    mov r8, [rsp + 56]                  ; saved fd
    cmp r8, -1
    je .l_mmap_commit
    cmp r8, MAX_FDS_PER_TASK
    jae .l_mmap_commit

    ; Get fd_entry for fd
    mov edi, r8d
    call vfs_get_fd_ptr
    test rax, rax
    jz .l_mmap_commit
    cmp byte [rax + 0], FD_TYPE_FILE
    jne .l_mmap_commit

    movzx ebp, word [rax + 2]           ; ebp = inode
    mov edx, [rax + 8]                  ; edx = file size
    mov r9, [rsp + 64]                  ; saved offset (offset 64 from rsp)
    cmp r9, rdx                         ; offset >= size?
    jae .l_mmap_commit
    sub edx, r9d                        ; remaining bytes in file from offset
    mov ecx, edx                        ; ecx = max bytes to read from file
    cmp rcx, [rsp + 40]                 ; min(file_remaining, length)
    jbe @f
    mov rcx, [rsp + 40]
@@:
    test rcx, rcx
    jz .l_mmap_commit

    ; Read file chunk directly into mapped virtual memory at rbx
    ; ext4_read_file_chunk_inode(EAX=inode, RCX=max_bytes, RDX=offset, RDI=dest)
    push rbx
    push rcx
    push rdx
    mov eax, ebp                        ; inode
    mov rdx, [rsp + 64 + 24]            ; file offset from stack (+24 for 3 pushes)
    mov rdi, rbx                        ; mapped virtual destination
    call ext4_read_file_chunk_inode
    pop rdx
    pop rcx
    pop rbx

.l_mmap_commit:
    test r14d, r14d
    jz .l_mmap_ret
    ; Bump mode only: commit [base, r12) now that every page is mapped, so a
    ; failed mapping can never leave linux_mmap_next past half-mapped VA.
    mov rax, r12
    sub rax, rbx
    add [r15 + linux_mmap_next - kmain], rax
.l_mmap_ret:
    mov rax, rbx                ; Return original base in RAX
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
    add rsp, 32                 ; discard saved rsi, r10, r8, r9
    ret

.l_mmap_enomem:
    ; Roll back pages already mapped in [rbx, r12): unmap them and give the
    ; physical pages back to the PMM (no leak, no VA reserved for a mapping
    ; that never happened).
    mov r14, rbx
.l_mmap_rb:
    cmp r14, r12
    jae .l_mmap_rb_done
    mov rdi, r14
    call vmm_unmap                 ; RAX = physical page (0 = nothing)
    test rax, rax
    jz .l_mmap_rb_next
    cmp rax, 0x10000000            ; only PMM pages (bitmap range) get freed;
    jae .l_mmap_rb_next            ; identity zone pages are not ours to free
    mov rdi, rax
    call pmm_free
.l_mmap_rb_next:
    add r14, 4096
    jmp .l_mmap_rb
.l_mmap_rb_done:
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
    add rsp, 32
    mov rax, LINUX_ENOMEM
    ret

.l_mmap_einval_p:
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
    add rsp, 32
.l_mmap_einval:
    mov rax, LINUX_EINVAL
    ret

.l_munmap:
    ; Linux x86_64 munmap(addr=RDI, length=RSI)
    test rsi, rsi
    jz .l_munmap_done
    and rdi, -4096
    ; C-L2: only the managed Linux mmap zone [0x20000000, 0x40000000) may be
    ; unmapped; never kernel tables, identity map or the task's own code/stack.
    cmp rdi, 0x20000000
    jb .l_munmap_einval
    mov rax, rdi
    add rax, rsi
    jc .l_munmap_einval
    cmp rax, 0x40000000
    ja .l_munmap_einval
    push r12
    push r13
    mov r12, rdi
    mov r13, rsi
.l_munmap_loop:
    test r13, r13
    jz .l_munmap_pop
    mov rdi, r12
    call vmm_unmap                 ; RAX = physical page that was mapped
    test rax, rax
    jz .l_munmap_next
    ; PMM pages all live below 0x10000000 (8KB bitmap -> 256MB); pages that
    ; merely carry the identity map of the zone (phys >= 0x20000000) are not
    ; ours and must never reach pmm_free (it would scribble past the bitmap).
    cmp rax, 0x10000000
    jae .l_munmap_next
    mov rdi, rax
    call pmm_free
.l_munmap_next:
    add r12, 4096
    sub r13, 4096
    ja .l_munmap_loop
.l_munmap_pop:
    pop r13
    pop r12
.l_munmap_done:
    xor eax, eax
    ret
.l_munmap_einval:
    mov rax, LINUX_EINVAL
    ret

.l_set_tid_address:
    ; RDI = tidptr
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rax]
    mov [rbx + TCB_CLEAR_CHILD_TID], rdi
    mov eax, [r15 + sched_curr_task - kmain]
    inc eax
    ret

.l_getuid:
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    mov eax, [r15 + task_table - kmain + rax + TCB_UID]
    ret

.l_getgid:
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    mov eax, [r15 + task_table - kmain + rax + TCB_GID]
    ret

.l_geteuid:
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    mov eax, [r15 + task_table - kmain + rax + TCB_EUID]
    ret

.l_getegid:
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    mov eax, [r15 + task_table - kmain + rax + TCB_EGID]
    ret

.l_setuid:
    ; RDI = uid
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rax]
    mov [rbx + TCB_UID], edi
    mov [rbx + TCB_EUID], edi
    mov [rbx + TCB_SUID], edi
    xor eax, eax
    ret

.l_setgid:
    ; RDI = gid
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rax]
    mov [rbx + TCB_GID], edi
    mov [rbx + TCB_EGID], edi
    mov [rbx + TCB_SGID], edi
    xor eax, eax
    ret

.l_setreuid:
    ; RDI = ruid, RSI = euid
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rax]
    cmp edi, -1
    je @f
    mov [rbx + TCB_UID], edi
@@:
    cmp esi, -1
    je @f
    mov [rbx + TCB_EUID], esi
@@:
    xor eax, eax
    ret

.l_setregid:
    ; RDI = rgid, RSI = egid
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rax]
    cmp edi, -1
    je @f
    mov [rbx + TCB_GID], edi
@@:
    cmp esi, -1
    je @f
    mov [rbx + TCB_EGID], esi
@@:
    xor eax, eax
    ret

.l_setresuid:
    ; RDI = ruid, RSI = euid, RDX = suid
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rax]
    cmp edi, -1
    je @f
    mov [rbx + TCB_UID], edi
@@:
    cmp esi, -1
    je @f
    mov [rbx + TCB_EUID], esi
@@:
    cmp edx, -1
    je @f
    mov [rbx + TCB_SUID], edx
@@:
    xor eax, eax
    ret

.l_getresuid:
    ; RDI = *ruid, RSI = *euid, RDX = *suid
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rax]
    test rdi, rdi
    jz @f
    mov ecx, [rbx + TCB_UID]
    mov [rdi], ecx
@@:
    test rsi, rsi
    jz @f
    mov ecx, [rbx + TCB_EUID]
    mov [rsi], ecx
@@:
    test rdx, rdx
    jz @f
    mov ecx, [rbx + TCB_SUID]
    mov [rdx], ecx
@@:
    xor eax, eax
    ret

.l_setresGID:
    ; RDI = rgid, RSI = egid, RDX = sgid
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rax]
    cmp edi, -1
    je @f
    mov [rbx + TCB_GID], edi
@@:
    cmp esi, -1
    je @f
    mov [rbx + TCB_EGID], esi
@@:
    cmp edx, -1
    je @f
    mov [rbx + TCB_SGID], edx
@@:
    xor eax, eax
    ret

.l_getresGID:
    ; RDI = *rgid, RSI = *egid, RDX = *sgid
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rax]
    test rdi, rdi
    jz @f
    mov ecx, [rbx + TCB_GID]
    mov [rdi], ecx
@@:
    test rsi, rsi
    jz @f
    mov ecx, [rbx + TCB_EGID]
    mov [rsi], ecx
@@:
    test rdx, rdx
    jz @f
    mov ecx, [rbx + TCB_SGID]
    mov [rdx], ecx
@@:
    xor eax, eax
    ret

.l_getgroups:
    ; int size = RDI, gid_t *list = RSI
    ; If size == 0, return number of supplementary groups (1: gid 0)
    test edi, edi
    jz .l_gg_count
    test rsi, rsi
    jz .l_gg_fault
    mov dword [rsi], 0               ; root group
    mov eax, 1
    ret
.l_gg_count:
    mov eax, 1
    ret
.l_gg_fault:
    mov rax, LINUX_EFAULT
    ret

.l_setgroups:
    ; size_t size = RDI, const gid_t *list = RSI
    xor eax, eax
    ret

.l_umask:
    ; mode_t mask = EDI
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rax]
    mov eax, [rbx + TCB_UMASK]        ; return old mask
    and edi, 0777o
    mov [rbx + TCB_UMASK], edi        ; set new mask
    ret

.l_chroot:
    ; const char *path = RDI
    ; DP-kg / debootstrap uses chroot; mock success for root
    xor eax, eax
    ret

.l_fsync:
    ; RDI = fd
    call vfs_get_fd_ptr
    test rax, rax
    jz .l_fsync_ebadf
    xor eax, eax
    ret
.l_fsync_ebadf:
    mov rax, LINUX_EBADF
    ret

.l_ftruncate:
    ; RDI = fd, RSI = length
    call vfs_get_fd_ptr
    test rax, rax
    jz .l_trunc_ebadf
    cmp byte [rax], FD_TYPE_FILE
    jne .l_trunc_ebadf
    movzx eax, word [rax + 2]        ; inode
    test eax, eax
    jz .l_trunc_ebadf
    mov rdx, rsi                     ; target size
    call ext4_truncate_file
    xor eax, eax
    ret
.l_trunc_ebadf:
    mov rax, LINUX_EBADF
    ret

.l_truncate:
    ; RDI = path, RSI = length
    push rsi
    call ext4_lookup
    pop rsi
    test eax, eax
    jz .l_trunc_enoent
    mov rdx, rsi                     ; target size
    call ext4_truncate_file
    xor eax, eax
    ret
.l_trunc_enoent:
    mov rax, LINUX_ENOENT
    ret

.l_chmod:
    ; RDI = pathname, RSI = mode
    mov eax, esi                     ; mode
    call .l_attr_lookup
    test eax, eax
    jz .l_attr_enoent
    ; EAX = inode, EBX = mode
    push rax
    call ext4_inode_load
    pop rdi
    jc .l_attr_eio
    ; Preserve file type (top 4 bits), replace low 12 bits with new mode
    and word [r15 + ext4_inode - kmain + IN_OFF_MODE], 0xF000
    and bx, 0x0FFF
    or  word [r15 + ext4_inode - kmain + IN_OFF_MODE], bx
    mov eax, edi
    call ext4_inode_save
    jc .l_attr_eio
    xor eax, eax
    ret

.l_fchmod:
    ; RDI = fd, RSI = mode
    mov ebx, esi
    call vfs_get_fd_ptr
    test rax, rax
    jz .l_attr_ebadf
    cmp byte [rax], FD_TYPE_FILE
    jne .l_attr_ebadf
    movzx eax, word [rax + 2]        ; inode
    test eax, eax
    jz .l_attr_ebadf
    push rax
    call ext4_inode_load
    pop rdi
    jc .l_attr_eio
    and word [r15 + ext4_inode - kmain + IN_OFF_MODE], 0xF000
    and bx, 0x0FFF
    or  word [r15 + ext4_inode - kmain + IN_OFF_MODE], bx
    mov eax, edi
    call ext4_inode_save
    jc .l_attr_eio
    xor eax, eax
    ret

.l_fchmodat:
    ; RDI = dirfd, RSI = pathname, RDX = mode, R10 = flags
    mov rdi, rsi
    mov rsi, rdx
    jmp .l_chmod

.l_chown:
    ; RDI = pathname, ESI = owner (uid), EDX = group (gid)
    push rdx                         ; gid
    push rsi                         ; uid
    call vfs_lookup_with_fallback
    pop rsi
    pop rdx
    test eax, eax
    jz .l_attr_enoent
    push rax                         ; inode
    call ext4_inode_load
    pop rdi
    jc .l_attr_eio
    cmp esi, -1
    je @f
    mov word [r15 + ext4_inode - kmain + 2], si   ; i_uid
@@:
    cmp edx, -1
    je @f
    mov word [r15 + ext4_inode - kmain + 24], dx  ; i_gid
@@:
    mov eax, edi
    call ext4_inode_save
    jc .l_attr_eio
    xor eax, eax
    ret

.l_fchown:
    ; RDI = fd, ESI = owner, EDX = group
    push rdx
    push rsi
    call vfs_get_fd_ptr
    pop rsi
    pop rdx
    test rax, rax
    jz .l_attr_ebadf
    cmp byte [rax], FD_TYPE_FILE
    jne .l_attr_ebadf
    movzx eax, word [rax + 2]        ; inode
    test eax, eax
    jz .l_attr_ebadf
    push rax
    call ext4_inode_load
    pop rdi
    jc .l_attr_eio
    cmp esi, -1
    je @f
    mov word [r15 + ext4_inode - kmain + 2], si   ; i_uid
@@:
    cmp edx, -1
    je @f
    mov word [r15 + ext4_inode - kmain + 24], dx  ; i_gid
@@:
    mov eax, edi
    call ext4_inode_save
    jc .l_attr_eio
    xor eax, eax
    ret

.l_fchownat:
    ; RDI = dirfd, RSI = pathname, RDX = owner, R10 = group, R8 = flags
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, r10
    jmp .l_chown

.l_utimensat:
    ; RDI = dirfd, RSI = pathname, RDX = struct timespec times[2], R10 = flags
    test rdx, rdx
    jz .l_utimens_now
    push rdx
    mov rdi, rdx
    mov rsi, 32
    call sys_validate_user_ptr
    pop rdx
    jc .l_attr_efault
    ; RDX = valid timespec array
    ; times[0] = atime, times[1] = mtime
    mov r8,  [rdx + 0]               ; atime.tv_sec
    mov r9,  [rdx + 16]              ; mtime.tv_sec
    jmp .l_utimens_apply

.l_utimens_now:
    ; Fake timestamp if NULL
    mov r8d, 0x68E50000              ; fixed epoch timestamp
    mov r9d, 0x68E50000

.l_utimens_apply:
    ; Resolve pathname in RSI
    push r9
    push r8
    mov rdi, rsi
    call vfs_lookup_with_fallback
    pop r8
    pop r9
    test eax, eax
    jz .l_attr_enoent
    push rax
    call ext4_inode_load
    pop rdi
    jc .l_attr_eio
    mov dword [r15 + ext4_inode - kmain + 8], r8d   ; i_atime
    mov dword [r15 + ext4_inode - kmain + 16], r9d  ; i_mtime
    mov dword [r15 + ext4_inode - kmain + 12], r9d  ; i_ctime
    mov eax, edi
    call ext4_inode_save
    jc .l_attr_eio
    xor eax, eax
    ret

.l_attr_lookup:
    ; RDI = pathname, EAX = mode. Returns EAX = inode, EBX = mode
    mov ebx, eax
    push rbx
    mov rsi, rdi
    call vfs_lookup_with_fallback
    pop rbx
    ret

.l_attr_enoent:
    mov rax, LINUX_ENOENT
    ret
.l_attr_ebadf:
    mov rax, LINUX_EBADF
    ret
.l_attr_eio:
    mov rax, LINUX_EIO
    ret
.l_attr_efault:
    mov rax, LINUX_EFAULT
    ret

.l_statx:
    ; glibc attempts statx first on Linux 4.11+; returning -ENOSYS cleanly falls back to stat/fstat/newfstatat
    mov rax, LINUX_ENOSYS
    ret

.l_poll:
    ; struct pollfd *fds = RDI, nfds_t nfds = RSI, int timeout = RDX
    ; Mock: each fd is ready for POLLIN | POLLOUT (revents = events)
    test rdi, rdi
    jz .poll_ret_zero
    test rsi, rsi
    jz .poll_ret_zero
    ; C-L5: clamp nfds and validate the whole pollfd array (nfds * 8 bytes)
    cmp rsi, 4096
    ja .poll_einval
    push rsi
    shl rsi, 3                   ; nfds * sizeof(struct pollfd)
    call sys_validate_user_ptr
    pop rsi
    jc .poll_efault
    mov rcx, rsi
    xor r8, r8                  ; r8 = ready count
.poll_loop:
    mov ax, [rdi + 4]           ; events
    mov [rdi + 6], ax           ; revents = events
    inc r8
    add rdi, 8                  ; sizeof(struct pollfd) = 8
    dec rcx
    jnz .poll_loop
    mov rax, r8
    ret
.poll_ret_zero:
    xor eax, eax
    ret
.poll_einval:
    mov rax, LINUX_EINVAL
    ret
.poll_efault:
    mov rax, LINUX_EFAULT
    ret

str_l_unimpl db "[Linux ABI] Unhandled syscall: #", 0
str_l_call_log db "[Linux ABI] Call: #", 0
str_l_fb_log db "[Linux ABI] fallback: #", 0
str_l_exit_log db "[Linux ABI] Exit with code: ", 0
str_l_sigkill db "[Linux ABI] Terminated by signal ", 0
str_l_write_dbg db "[Linux ABI] write: fd=", 0
str_l_write_dbg2 db " -> ", 0

align 8
linux_mmap_next dq 0x20000000

MAX_INODE_LOCKS = 16
INODE_LOCK_ENTRY_SIZE = 16
; Each entry:
; +0: uint32 inode (0 = free)
; +4: uint32 owner_tgid
; +8: uint32 lock_type (1 = read / shared, 2 = write / exclusive)
; +12: uint32 reserved
align 16
inode_lock_table: rb (MAX_INODE_LOCKS * INODE_LOCK_ENTRY_SIZE)
