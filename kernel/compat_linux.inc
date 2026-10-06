; ==============================================================================
; Opensweet OS - Linux ABI Compatibility Layer (kernel/compat_linux.inc)
; Provides drop-in modular binary compatibility for Linux x86_64 executables.
; Can be fully toggled via CONFIG_LINUX_COMPAT.
; ==============================================================================

CONFIG_LINUX_COMPAT = 1

; Linux x86_64 Syscall Numbers
LINUX_SYS_READ        = 0
LINUX_SYS_WRITE       = 1
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
LINUX_SYS_VFORK       = 58
LINUX_SYS_GETPPID     = 110

; Linux Error Codes (negative return values in RAX)
LINUX_EPERM   = -1
LINUX_EAGAIN  = -11
LINUX_ENOEXEC = -8
LINUX_ENOENT  = -2
LINUX_ESRCH   = -3
LINUX_EINTR   = -4
LINUX_EIO     = -5
LINUX_EBADF   = -9
LINUX_ENOMEM  = -12
LINUX_EACCES  = -13
LINUX_EFAULT  = -14
LINUX_EEXIST  = -17
LINUX_ENODEV  = -19
LINUX_ENOTDIR = -20
LINUX_EISDIR  = -21
LINUX_EINVAL  = -22
LINUX_ENFILE  = -23
LINUX_EMFILE  = -24
LINUX_ENOTTY = -25
LINUX_EPIPE  = -32
LINUX_ENOSYS  = -38

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
    je .l_exit
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
    cmp eax, LINUX_SYS_GETUID
    je .l_zero_id
    cmp eax, LINUX_SYS_GETGID
    je .l_zero_id
    cmp eax, LINUX_SYS_GETEUID
    je .l_zero_id
    cmp eax, LINUX_SYS_GETEGID
    je .l_zero_id
    cmp eax, LINUX_SYS_FORK
    je .l_fork
    cmp eax, LINUX_SYS_VFORK
    je .l_fork
    cmp eax, LINUX_SYS_CLONE
    je .l_fork
    cmp eax, LINUX_SYS_WAIT4
    je .l_wait4
    cmp eax, LINUX_SYS_GETPPID
    je .l_getppid
    cmp eax, LINUX_SYS_EXECVE
    je .l_execve

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
    call vfs_write_fd
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
    call vfs_write_fd
    test rax, rax
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

.l_ioctl:
    ; RDI = fd, RSI = request, RDX = arg
    ; For TIOCGWINSZ (0x5413), fill 80x25 or return ENOTTY (-25)
    mov rax, -25                ; -ENOTTY
    ret

.l_open:
    ; RDI = pathname, RSI = flags, RDX = mode
    call vfs_open
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
    inc eax                     ; 1-based PID
    ret

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
    mov rax, [rbx + 120]
    mov [r13 + TCB_USER_RSP], rax     ; parent user RSP
    mov rax, [rbp + TCB_NAME]
    mov [r13 + TCB_NAME], rax
    lea rsi, [rbp + TCB_FXSAVE]       ; FPU/SSE state from the parent
    lea rdi, [r13 + TCB_FXSAVE]
    mov ecx, 512 / 8
    rep movsq

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
    ret

.l_rmdir:
    ; RDI = path
    call sys_handler_rmdir
    ret

.l_unlink:
    ; RDI = path
    call sys_handler_unlink
    ret

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

    ; Loop over length in 4KB pages and map with vmm_map
    push r12
    push r13
    push r14
    push r15
    push rbx

    xor r14d, r14d                  ; r14 = bump-mode flag (addr == 0)
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
    mov rax, LINUX_ENOMEM
    ret

.l_mmap_einval_p:
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
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
    ; RDI = tidptr (mock: return current PID)
    mov eax, [r15 + sched_curr_task - kmain]
    inc eax
    ret

.l_zero_id:
    xor eax, eax
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

align 8
linux_mmap_next dq 0x20000000
