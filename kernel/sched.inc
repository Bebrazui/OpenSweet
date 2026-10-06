; ==============================================================================
; Opensweet OS - Preemptive Multitasking Scheduler (kernel/sched.inc)
; Round-Robin Preemption via Local APIC Timer (1000 Hz, 10ms Quantum).
; Full 64-bit CPU context save/restore (SS, RSP, RFLAGS, CS, RIP + 15 GPRs).
; ==============================================================================

MAX_TASKS       = 8
SCHED_QUANTUM   = 10                  ; 10 ms time-slice (at 1000 Hz tick rate)

TASK_UNUSED     = 0
TASK_READY      = 1
TASK_RUNNING    = 2
TASK_SLEEPING   = 3
TASK_DEAD       = 4

USER_PAGE_TABLES_BASE = 0x00200000    ; 2MB: Physical base for per-process page tables (96KB for 8 slots)
USER_PHYS_BASE        = 0x05000000    ; 80MB: Physical base for per-process code/stack (42MB for 7 slots)
USER_MEM_STRIDE       = 0x00600000    ; 6MB physical stride per task slot (4MB code + 2MB stack)

; TCB offsets (608 bytes per TCB, 16-byte aligned)
TCB_RSP         = 0x00                ; uint64: Saved stack pointer
TCB_STATE       = 0x08                ; uint32: TASK_* state
TCB_ID          = 0x0C                ; uint32: Task ID
TCB_SLEEP       = 0x10                ; uint32: Sleep ticks remaining (ms)
TCB_SLICE       = 0x14                ; uint32: Remaining quantum in ticks
TCB_TICKS       = 0x18                ; uint64: Total CPU ticks accumulated
TCB_STACK_BASE  = 0x20                ; uint64: Bottom of stack memory
TCB_STACK_TOP   = 0x28                ; uint64: Top of stack memory
TCB_NAME        = 0x30                ; 8 bytes: ASCIIZ task name
TCB_CR3         = 0x38                ; uint64: CR3 page directory base (phys)
TCB_USER_RSP    = 0x40                ; uint64: Saved user RSP for Ring 3 task
TCB_IS_USER     = 0x48                ; uint32: 1 if Ring 3 user task, 0 if kernel
TCB_HEAP_BRK    = 0x4C                ; uint32: Current heap brk virtual address for Ring 3 task
TCB_PARENT_ID   = 0x50                ; uint32: Parent task ID (who spawned this process)
TCB_EXIT_CODE   = 0x54                ; uint32: Process exit status code
TCB_WAIT_TID    = 0x58                ; int32: Target child TID being waited on (-1 if none)
TCB_ABI_TYPE    = 0x5C                ; uint32: ABI type (0 = ABI_NATIVE, 1 = ABI_LINUX)
TCB_FS_BASE     = 0x60                ; uint64: Thread-local storage base (FS_BASE for Linux / TLS)
TCB_PAD         = 0x68                ; uint64: alignment padding
TCB_FXSAVE      = 0x70                ; 512 bytes: 16-byte aligned FXSAVE/FXRSTOR area
TCB_SIZE        = 624                 ; 0x60 + 512 = 608 bytes (multiple of 16)

TASK_STACK_SIZE = 8192                ; 8 KB stack per task

align 16
; Task Table & Global Scheduler State
sched_enabled   dd 0
sched_curr_task dd 0                  ; Index 0..MAX_TASKS-1
sched_task_cnt  dd 1                  ; 1 task initially (Task 0 = Shell)

align 16
task_table:     rb (MAX_TASKS * TCB_SIZE)

align 16
; Dedicated 16KB stack memory pool for 8 tasks (128 KB total)
task_stacks:    rb (MAX_TASKS * TASK_STACK_SIZE)

; Strings
str_task_hdr    db "TID  NAME             STATE      TICKS     STACK_RSP", 10, 0
str_task_sep    db "--------------------------------------------------------", 10, 0
str_st_ready    db "READY    ", 0
str_st_running  db "RUNNING  ", 0
str_st_sleeping db "SLEEPING ", 0
str_st_dead     db "DEAD     ", 0
str_st_unused   db "UNUSED   ", 0

str_name_shell  db "shell", 0
str_name_clock  db "clock", 0
str_name_sysmon db "sysmon", 0

; ==============================================================================
; sched_init: Initialize scheduler, register Task 0 ("shell") as running
; ==============================================================================
sched_init:
    push rax
    push rbx
    push rcx
    push rdi
    push rsi

    ; Clear entire task table
    lea rdi, [r15 + task_table - kmain]
    mov ecx, (MAX_TASKS * TCB_SIZE) / 4
    xor eax, eax
    rep stosd

    ; Initialize Task 0 (Interactive Shell & Desktop)
    lea rbx, [r15 + task_table - kmain]
    mov dword [rbx + TCB_STATE], TASK_RUNNING
    mov dword [rbx + TCB_ID], 0
    mov dword [rbx + TCB_SLEEP], 0
    mov dword [rbx + TCB_SLICE], SCHED_QUANTUM
    mov qword [rbx + TCB_TICKS], 0
    mov qword [rbx + TCB_CR3], 0x1000  ; Task 0 uses boot kernel PML4
    mov dword [rbx + TCB_IS_USER], 0

    ; Stack 0 uses initial boot stack 0x90000
    mov qword [rbx + TCB_STACK_BASE], 0x80000
    mov qword [rbx + TCB_STACK_TOP],  0x90000
    mov dword [rbx + TCB_ABI_TYPE],   0
    mov qword [rbx + TCB_FS_BASE],    0

    ; Initialize syscall stack to Task 0's stack top
    mov rax, 0x90000
    mov [r15 + sys_kernel_stack - kmain], rax

    ; Initialize FPU/SSE state for Task 0
    fxsave [rbx + TCB_FXSAVE]

    ; Copy name "shell"
    lea rsi, [r15 + str_name_shell - kmain]
    lea rdi, [rbx + TCB_NAME]
    mov ecx, 6
    rep movsb

    mov dword [r15 + sched_curr_task - kmain], 0
    mov dword [r15 + sched_task_cnt - kmain], 1
    mov dword [r15 + sched_enabled - kmain], 1

    ; Initialize Task 0 (Shell) file descriptors (stdin, stdout, stderr)
    xor ebx, ebx
    call vfs_init_task_fds

    pop rsi
    pop rdi
    pop rcx
    pop rbx
    pop rax
    ret

; ==============================================================================
; task_create: Create a new kernel thread
; RSI = name string (ASCIIZ)
; RDX = entry point function pointer
; Returns: EAX = task ID (or -1 on error)
; ==============================================================================
task_create:
    push rbx
    push rcx
    push rdi
    push rsi
    push r8
    push r9

    ; Find free TCB slot (1..MAX_TASKS-1)
    mov ebx, 1
.find_slot:
    cmp ebx, MAX_TASKS
    jae .create_fail

    imul eax, ebx, TCB_SIZE
    lea rdi, [r15 + task_table - kmain + rax]
    cmp dword [rdi + TCB_STATE], TASK_UNUSED
    je .slot_found
    cmp dword [rdi + TCB_STATE], TASK_DEAD
    je .slot_found
    inc ebx
    jmp .find_slot

.slot_found:
    ; RDI points to TCB for slot ebx
    mov dword [rdi + TCB_ID], ebx
    mov dword [rdi + TCB_SLEEP], 0
    mov dword [rdi + TCB_SLICE], SCHED_QUANTUM
    mov qword [rdi + TCB_TICKS], 0

    ; Copy name (up to 15 chars)
    push rsi
    push rdi
    lea rdi, [rdi + TCB_NAME]
    xor ecx, ecx
.copy_name:
    lodsb
    stosb
    test al, al
    jz .name_done
    inc ecx
    cmp ecx, 15
    jb .copy_name
    mov byte [rdi], 0
.name_done:
    pop rdi
    pop rsi

    ; Setup private stack for task ebx
    imul eax, ebx, TASK_STACK_SIZE
    lea r8, [r15 + task_stacks - kmain + rax]
    mov [rdi + TCB_STACK_BASE], r8
    add r8, TASK_STACK_SIZE
    mov [rdi + TCB_STACK_TOP], r8
    mov dword [rdi + TCB_ABI_TYPE], 0
    mov qword [rdi + TCB_FS_BASE], 0

    ; Initialize stack frame to simulate hardware interrupt return (iretq)
    ; Hardware frame: SS, RSP, RFLAGS, CS, RIP
    ; Followed by 15 GPRs: RAX, RBX, RCX, RDX, RSI, RDI, RBP, R8..R15
    mov r9, r8                        ; r9 = current top of stack

    ; Push SS (DATA64_SEL = 0x10)
    sub r9, 8
    mov qword [r9], DATA64_SEL

    ; Push RSP (initial stack top)
    sub r9, 8
    mov qword [r9], r8

    ; Push RFLAGS (0x202 = IF=1, interrupts enabled)
    sub r9, 8
    mov qword [r9], 0x202

    ; Push CS (CODE64_SEL = 0x08)
    sub r9, 8
    mov qword [r9], CODE64_SEL

    ; Push RIP (task entry point in RDX)
    sub r9, 8
    mov qword [r9], rdx

    ; Now 15 GPRs (RAX, RBX, RCX, RDX, RSI, RDI, RBP, R8, R9, R10, R11, R12, R13, R14, R15)
    ; RAX..R13 = 0
    sub r9, 8*13
    push rdi
    lea rdi, [r9]
    xor eax, eax
    mov ecx, 26                       ; 13 qwords = 26 dwords
    rep stosd
    pop rdi

    ; R14 = 0 (low memory base)
    sub r9, 8
    mov qword [r9], 0

    ; R15 = 0xFFFF800000010000 (Kernel Image Base, crucial for relative refs!)
    sub r9, 8
    mov [r9], r15

    ; Saved RSP in TCB points to the top of the GPR frame
    mov [rdi + TCB_RSP], r9

    ; Initialize task FPU/SSE state from clean Task 0 state
    push rsi
    push rdi
    push rcx
    lea rsi, [r15 + task_table - kmain + TCB_FXSAVE]
    lea rdi, [rdi + TCB_FXSAVE]
    mov ecx, 512 / 8
    rep movsq
    pop rcx
    pop rdi
    pop rsi

    ; Mark task as READY
    mov dword [rdi + TCB_STATE], TASK_READY
    inc dword [r15 + sched_task_cnt - kmain]

    mov eax, ebx                      ; return TID
    jmp .create_done

.create_fail:
    mov eax, -1

.create_done:
    pop r9
    pop r8
    pop rsi
    pop rdi
    pop rcx
    pop rbx
    ret

; ==============================================================================
; task_setup_pagetables: Initialize dedicated 4-level PML4/PDPT/PD for task slot EBX
; EBX = slot ID (1..MAX_TASKS-1)
; Returns: RAX = PML4 physical address
; ==============================================================================
task_setup_pagetables:
    push rbx
    push rcx
    push rdi
    push rsi
    push r8
    push r9
    push r10

    ; Physical addresses:
    ; PML4 = USER_PAGE_TABLES_BASE + EBX * 12288
    ; PDPT = PML4 + 4096
    ; PD   = PDPT + 4096
    mov eax, ebx
    imul eax, 12288
    add eax, USER_PAGE_TABLES_BASE
    mov r8d, eax                      ; r8d = pml4_phys
    lea r9d, [r8d + 4096]             ; r9d = pdpt_phys
    lea r10d, [r9d + 4096]            ; r10d = pd_phys

    ; Zero all 3 tables (12288 bytes = 3072 dwords)
    mov edi, r8d
    mov ecx, 12288 / 4
    xor eax, eax
    cld
    rep stosd

    ; 1. PML4:
    ; PML4[0] -> PDPT (flags 0x07 = Present | Writable | User)
    mov eax, r9d
    or eax, 0x07
    mov [r8], rax

    ; PML4[256] -> higher-half kernel PDPT (0x4001: PDPT_B @ 0x4000)
    mov dword [r8 + 0x800], 0x4001

    ; 2. PDPT:
    ; PDPT[0] -> PD (flags 0x07 = Present | Writable | User)
    mov eax, r10d
    or eax, 0x07
    mov [r9], rax

    ; C3 Mitigation: PDPT[1..3] -> supervisor-only identity maps for 1..4GB (LAPIC/VBE LFB)
    ; Flags 0x83 = Present | Writable | Supervisor | Huge (no User bit 2)
    mov dword [r9 + 0x08], 0x40000083
    mov dword [r9 + 0x10], 0x80000083
    mov dword [r9 + 0x18], 0xC0000083

    ; 3. PD:
    ; Fill all 512 entries with 2MB identity mapping (supervisor 0x83)
    ; Ensures kernel in Ring 0 has full identity access to 0..1GB physical memory
    mov edi, r10d
    mov eax, 0x83                     ; phys 0, Present | Writable | Supervisor | Huge
    xor ecx, ecx
.fill_pd_loop:
    mov [rdi + rcx*8], eax
    mov dword [rdi + rcx*8 + 4], 0
    add eax, 0x00200000
    inc ecx
    cmp ecx, 512
    jb .fill_pd_loop

    ; Customize user-accessible entries in PD:
    ; Entries 2..3 (virtual 0x00400000..0x007FFFFF): User Code/Data/BSS (4MB)
    ; Map to slot's private physical code: USER_PHYS_BASE + (ebx - 1) * USER_MEM_STRIDE
    lea eax, [ebx - 1]
    imul eax, USER_MEM_STRIDE
    add eax, USER_PHYS_BASE
    or eax, 0x87                      ; Present | Writable | User | Huge
    mov [r10 + 2*8], eax
    mov dword [r10 + 2*8 + 4], 0
    add eax, 0x00200000               ; second 2MB of the code slot
    mov [r10 + 3*8], eax
    mov dword [r10 + 3*8 + 4], 0

    ; Entry 255 (virtual 0x1FE00000..0x1FFFFFFF): User Stack (top 0x1FFFFF00),
    ; parked just below the Linux mmap zone. Moving it out of 0x600000..0x800000
    ; gives the code slot the full 4MB at 0x400000..0x7FFFFF (brk heap owns
    ; 16..64MB, canvases own 8..16MB - there was nowhere else to grow).
    ; Map to slot's private physical stack: phys_code + 0x00400000
    lea eax, [ebx - 1]
    imul eax, USER_MEM_STRIDE
    add eax, USER_PHYS_BASE + 0x00400000
    or eax, 0x87                      ; Present | Writable | User | Huge
    mov [r10 + 255*8], eax
    mov dword [r10 + 255*8 + 4], 0

    ; Entries 4..7 (virtual 0x00800000..0x00E00000): Window Canvases (8MB..16MB)
    ; Give user access (0x87) so user app can render to canvas
    mov eax, 0x00800087
    mov ecx, 4
.map_canvas_loop:
    mov [r10 + rcx*8], eax
    mov dword [r10 + rcx*8 + 4], 0
    add eax, 0x00200000
    inc ecx
    cmp ecx, 8
    jb .map_canvas_loop

    ; Entries 8..31 (virtual 0x01000000..0x04000000 = 16MB..64MB, 48MB): User Heap (for Doom / Games)
    mov eax, 0x01000087
    mov ecx, 8
.map_ext_heap_loop:
    mov [r10 + rcx*8], eax
    mov dword [r10 + rcx*8 + 4], 0
    add eax, 0x00200000
    inc ecx
    cmp ecx, 32
    jb .map_ext_heap_loop

    ; Return PML4 physical address in RAX
    mov rax, r8

    pop r10
    pop r9
    pop r8
    pop rsi
    pop rdi
    pop rcx
    pop rbx
    ret

; ==============================================================================
; task_alloc_user_slot: Allocate an unused task slot and initialize its page tables
; Returns:
;   EAX = slot ID (1..MAX_TASKS-1), or -1 if no free slot
;   RDI = pointer to TCB for this slot
;   R8  = PML4 physical address (CR3)
;   R9  = physical code base (2MB)
;   R10 = physical stack base (2MB)
; ==============================================================================
task_alloc_user_slot:
    push rcx
    push rdx
    push rsi

    ; Pass 1: Find completely unused slot (1..MAX_TASKS-1)
    mov ebx, 1
.find_unused:
    cmp ebx, MAX_TASKS
    jae .try_dead

    imul eax, ebx, TCB_SIZE
    lea rdi, [r15 + task_table - kmain + rax]
    cmp dword [rdi + TCB_STATE], TASK_UNUSED
    je .found
    inc ebx
    jmp .find_unused

.try_dead:
    ; Pass 2: Reclaim unreaped dead slot
    mov ebx, 1
.find_dead:
    cmp ebx, MAX_TASKS
    jae .no_slot

    imul eax, ebx, TCB_SIZE
    lea rdi, [r15 + task_table - kmain + rax]
    cmp dword [rdi + TCB_STATE], TASK_DEAD
    je .found
    inc ebx
    jmp .find_dead

.no_slot:
    pop rsi
    pop rdx
    pop rcx
    mov eax, -1
    mov ebx, -1
    ret

.found:
    ; Setup page tables for slot ebx
    call task_setup_pagetables
    ; RAX = PML4 physical address
    mov r8, rax
    mov [rdi + TCB_CR3], rax

    ; Compute physical code and stack bases
    lea eax, [ebx - 1]
    imul eax, USER_MEM_STRIDE
    add eax, USER_PHYS_BASE
    mov r9d, eax                      ; r9  = phys_code
    lea r10d, [r9d + 0x00400000]      ; r10 = phys_stack (4MB code slot + 2MB)

    mov eax, ebx                      ; return slot ID in EAX and EBX

    pop rsi
    pop rdx
    pop rcx
    ret

; ==============================================================================
; task_finalize_user: Finish setting up TCB and kernel stack for a Ring 3 user task
; EBX = slot ID (1..MAX_TASKS-1)
; RSI = name string (ASCIIZ)
; RDX = user RIP entry point (e.g. 0x400000)
; R8  = user RSP initial stack top (e.g. 0x1FFFFF00)
; Returns: EAX = task ID
; ==============================================================================
task_finalize_user:
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

    mov r10, rdx                      ; user RIP
    mov r11, r8                       ; user RSP

    imul eax, ebx, TCB_SIZE
    lea rdi, [r15 + task_table - kmain + rax]

    mov dword [rdi + TCB_ID], ebx
    mov dword [rdi + TCB_SLEEP], 0
    mov dword [rdi + TCB_SLICE], SCHED_QUANTUM
    mov qword [rdi + TCB_TICKS], 0
    mov dword [rdi + TCB_IS_USER], 1
    mov dword [rdi + TCB_HEAP_BRK], 0x01000000 ; Initial user heap base (16MB)
    ; execve: PARENT_ID must survive (this is a re-exec, not a new child)
    cmp qword [r15 + sys_spawn_exec_mode - kmain], 0
    jne @f
    mov eax, [r15 + sched_curr_task - kmain]
    mov dword [rdi + TCB_PARENT_ID], eax
@@:
    mov dword [rdi + TCB_EXIT_CODE], 0
    mov dword [rdi + TCB_WAIT_TID], -1
    ; ABI type staged by the caller (elf spawn detection) BEFORE finalize:
    ; the task is marked READY below and may start running immediately.
    mov eax, dword [r15 + sys_spawn_abi - kmain]
    mov dword [rdi + TCB_ABI_TYPE], eax
    mov qword [r15 + sys_spawn_abi - kmain], 0
    mov qword [rdi + TCB_FS_BASE], 0
    mov [rdi + TCB_USER_RSP], r11

    ; Copy name (up to 7 chars)
    push rsi
    push rdi
    lea rdi, [rdi + TCB_NAME]
    xor ecx, ecx
.copy_name:
    lodsb
    stosb
    test al, al
    jz .name_done
    inc ecx
    cmp ecx, 7
    jb .copy_name
    mov byte [rdi], 0
.name_done:
    pop rdi
    pop rsi

    ; Setup private kernel stack for task ebx (for IRQs, TSS.RSP0, and SYSCALL)
    imul eax, ebx, TASK_STACK_SIZE
    lea r8, [r15 + task_stacks - kmain + rax]
    mov [rdi + TCB_STACK_BASE], r8
    add r8, TASK_STACK_SIZE
    mov [rdi + TCB_STACK_TOP], r8
    mov qword [rdi + TCB_FS_BASE], 0

    ; execve: the return path is the (patched) syscall frame, not an iretq
    ; image — and the task is already RUNNING, so skip the frame build.
    cmp qword [r15 + sys_spawn_exec_mode - kmain], 0
    jne .fxsave_setup

    ; Initialize kernel stack frame for iretq transition to Ring 3
    mov r9, r8

    ; Push SS (USER_DATA_SEL | 3 = 0x23)
    sub r9, 8
    mov qword [r9], (USER_DATA_SEL or 3)

    ; Push RSP (User RSP in r11)
    sub r9, 8
    mov qword [r9], r11

    ; Push RFLAGS (0x202 = IF=1, interrupts enabled)
    sub r9, 8
    mov qword [r9], 0x202

    ; Push CS (USER_CODE_SEL | 3 = 0x2B)
    sub r9, 8
    mov qword [r9], (USER_CODE_SEL or 3)

    ; Push RIP (User RIP in r10)
    sub r9, 8
    mov qword [r9], r10

    ; Push 15 GPRs (RAX..R15 = 0)
    sub r9, 8*15
    push rdi
    lea rdi, [r9]
    xor eax, eax
    mov ecx, 30
    rep stosd
    pop rdi

    ; Saved RSP in TCB points to top of GPR frame
    mov [rdi + TCB_RSP], r9

    ; Initialize task FPU/SSE state from clean Task 0 state
.fxsave_setup:
    push rsi
    push rdi
    push rcx
    lea rsi, [r15 + task_table - kmain + TCB_FXSAVE]
    lea rdi, [rdi + TCB_FXSAVE]
    mov ecx, 512 / 8
    rep movsq
    pop rcx
    pop rdi
    pop rsi

    ; execve: state stays RUNNING, task counter and fd table are already in
    ; place — jump straight to the return value.
    cmp qword [r15 + sys_spawn_exec_mode - kmain], 0
    jne .exec_done

    ; Mark task as READY
    mov dword [rdi + TCB_STATE], TASK_READY
    inc dword [r15 + sched_task_cnt - kmain]

    ; Initialize standard file descriptors (stdin, stdout, stderr) for task ebx
    push rbx
    call vfs_init_task_fds
    pop rbx

    ; Check if parent requested stdin/stdout/stderr redirections
    mov edx, [r15 + sys_spawn_in_fd - kmain]
    cmp edx, 0
    jl @f
    mov eax, [r15 + sched_curr_task - kmain]
    mov ecx, 0                          ; child fd 0
    call vfs_redirect_child_fd
@@:
    mov edx, [r15 + sys_spawn_out_fd - kmain]
    cmp edx, 0
    jl @f
    mov eax, [r15 + sched_curr_task - kmain]
    mov ecx, 1                          ; child fd 1
    call vfs_redirect_child_fd
@@:
    mov edx, [r15 + sys_spawn_err_fd - kmain]
    cmp edx, 0
    jl @f
    mov eax, [r15 + sched_curr_task - kmain]
    mov ecx, 2                          ; child fd 2
    call vfs_redirect_child_fd
@@:

.exec_done:
    mov eax, ebx                      ; return TID

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
; task_create_user: Legacy wrapper to create a Ring 3 user task
; RSI = name string (ASCIIZ)
; RDX = user RIP entry point (e.g. 0x400000)
; R8  = user RSP initial stack top (e.g. 0x1FFFFF00)
; Returns: EAX = task ID (or -1 on error)
; ==============================================================================
task_create_user:
    push rbx
    push r12
    push r13
    push r8
    push r9
    push r10

    call task_alloc_user_slot
    cmp eax, -1
    je .tcu_fail

    mov ebx, eax
    call task_finalize_user
    jmp .tcu_done

.tcu_fail:
    mov eax, -1
.tcu_done:
    pop r10
    pop r9
    pop r8
    pop r13
    pop r12
    pop rbx
    ret

; ==============================================================================
; task_sleep: Put current task to sleep for ECX milliseconds
; ECX = duration in milliseconds (timer ticks)
; ==============================================================================
task_sleep:
    push rax
    push rbx

    test ecx, ecx
    jz .sleep_done

    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rax]

    mov [rbx + TCB_SLEEP], ecx
    mov dword [rbx + TCB_STATE], TASK_SLEEPING

    ; Yield CPU immediately to next ready task
    int 0x31

.sleep_done:
    pop rbx
    pop rax
    ret

; ==============================================================================
; task_yield: Voluntarily yield remaining quantum to next ready task
; ==============================================================================
task_yield:
    int 0x31
    ret

; ==============================================================================
; task_exit: Terminate current task
; ==============================================================================
task_exit:
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rbx, [r15 + task_table - kmain + rax]
    mov [rbx + TCB_EXIT_CODE], edi     ; Store user exit status
    mov dword [rbx + TCB_STATE], TASK_DEAD
    dec dword [r15 + sched_task_cnt - kmain]

    ; Close all file descriptors for exiting task
    call vfs_cleanup_task_fds

    int 0x31
.halt:
    hlt
    jmp .halt

; ==============================================================================
; sched_tick: Called from APIC timer ISR (Vector 0x30)
; Saves current RSP, manages sleep timers and quantum, switches RSP if preempting
; Input: RSP points to 15 pushed GPRs on stack
; Output: RSP potentially changed to next task's stack!
; ==============================================================================
sched_tick:
    ; 1. Wake up sleeping tasks
    xor ebx, ebx
.sleep_loop:
    cmp ebx, MAX_TASKS
    jae .sleep_check_done

    imul eax, ebx, TCB_SIZE
    lea rsi, [r15 + task_table - kmain + rax]
    cmp dword [rsi + TCB_STATE], TASK_SLEEPING
    jne .next_sleep

    cmp dword [rsi + TCB_SLEEP], 0
    je .wake_task
    dec dword [rsi + TCB_SLEEP]
    jnz .next_sleep

.wake_task:
    mov dword [rsi + TCB_STATE], TASK_READY

.next_sleep:
    inc ebx
    jmp .sleep_loop

.sleep_check_done:
    ; Check if scheduler is active
    cmp dword [r15 + sched_enabled - kmain], 1
    jne .sched_no_switch

    ; Current running task
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rsi, [r15 + task_table - kmain + rax]

    ; Track total CPU ticks
    inc qword [rsi + TCB_TICKS]

    ; Decrement quantum
    cmp dword [rsi + TCB_SLICE], 0
    je .quantum_expired
    dec dword [rsi + TCB_SLICE]
    jnz .sched_no_switch              ; Still has quantum left on this task!

.quantum_expired:
    ; Time slice exhausted -> switch to next task
    mov dword [rsi + TCB_SLICE], SCHED_QUANTUM
    cmp dword [rsi + TCB_STATE], TASK_RUNNING
    jne .find_next
    mov dword [rsi + TCB_STATE], TASK_READY

.find_next:
    ; Save current task RSP, FS_BASE, and FPU/SSE state
    mov [rsi + TCB_RSP], rsp
    mov ecx, 0xC0000100
    rdmsr
    shl rdx, 32
    or rax, rdx
    mov [rsi + TCB_FS_BASE], rax
    fxsave [rsi + TCB_FXSAVE]

    ; Pick next task in circular order
    mov edx, [r15 + sched_curr_task - kmain]
    mov ecx, MAX_TASKS
.search_next:
    inc edx
    cmp edx, MAX_TASKS
    jb @f
    xor edx, edx
@@:
    imul eax, edx, TCB_SIZE
    lea rdi, [r15 + task_table - kmain + rax]
    cmp dword [rdi + TCB_STATE], TASK_READY
    je .found_next
    cmp dword [rdi + TCB_STATE], TASK_RUNNING
    je .found_next
    loop .search_next

    ; If no other ready task found, fallback to Task 0 (shell / idle)
    xor edx, edx
    imul eax, edx, TCB_SIZE
    lea rdi, [r15 + task_table - kmain + rax]

.found_next:
    ; Switch to task EDX
    mov [r15 + sched_curr_task - kmain], edx
    mov dword [rdi + TCB_STATE], TASK_RUNNING
    mov dword [rdi + TCB_SLICE], SCHED_QUANTUM

    ; Update TSS RSP0 and sys_kernel_stack to incoming task's kernel stack top
    mov rax, [rdi + TCB_STACK_TOP]
    mov [r15 + tss64 + 4 - kmain], rax
    mov [r15 + sys_kernel_stack - kmain], rax

    ; Switch address space (CR3) if task has private page tables
    mov rax, [rdi + TCB_CR3]
    test rax, rax
    jnz .tick_has_cr3
    mov rax, 0x1000
.tick_has_cr3:
    mov rdx, cr3
    cmp rax, rdx
    je .tick_cr3_done
    mov cr3, rax
.tick_cr3_done:

    ; Restore FS_BASE (MSR 0xC0000100) for thread-local storage
    mov rax, [rdi + TCB_FS_BASE]
    mov rdx, rax
    shr rdx, 32
    mov ecx, 0xC0000100
    wrmsr

    ; Restore incoming task's FPU/SSE context
    fxrstor [rdi + TCB_FXSAVE]

    ; Load new RSP!
    mov rsp, [rdi + TCB_RSP]

.sched_no_switch:
    jmp sched_restore

; ==============================================================================
; isr_yield: Software interrupt 0x31 handler (task_yield / task_sleep)
; ==============================================================================
isr_yield:
    ; Push all 15 GPRs to match common_irq frame
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov r15, 0xFFFF800000010000 ; Establish kernel image base in R15

    ; Save current task RSP, FS_BASE, and FPU/SSE state
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rsi, [r15 + task_table - kmain + rax]
    mov [rsi + TCB_RSP], rsp
    mov ecx, 0xC0000100
    rdmsr
    shl rdx, 32
    or rax, rdx
    mov [rsi + TCB_FS_BASE], rax
    fxsave [rsi + TCB_FXSAVE]

    ; If current task was RUNNING, set to READY (if SLEEPING, leave as SLEEPING)
    cmp dword [rsi + TCB_STATE], TASK_RUNNING
    jne @f
    mov dword [rsi + TCB_STATE], TASK_READY
@@:

    ; Find next ready task
    mov edx, [r15 + sched_curr_task - kmain]
    mov ecx, MAX_TASKS
.find_yield_next:
    inc edx
    cmp edx, MAX_TASKS
    jb @f
    xor edx, edx
@@:
    imul eax, edx, TCB_SIZE
    lea rdi, [r15 + task_table - kmain + rax]
    cmp dword [rdi + TCB_STATE], TASK_READY
    je .yield_next_found
    cmp dword [rdi + TCB_STATE], TASK_RUNNING
    je .yield_next_found
    loop .find_yield_next

    ; Fallback to Task 0 (shell / idle)
    xor edx, edx
    imul eax, edx, TCB_SIZE
    lea rdi, [r15 + task_table - kmain + rax]

.yield_next_found:
    mov [r15 + sched_curr_task - kmain], edx
    mov dword [rdi + TCB_STATE], TASK_RUNNING
    mov dword [rdi + TCB_SLICE], SCHED_QUANTUM

    ; Update TSS RSP0 and sys_kernel_stack to incoming task's kernel stack top
    mov rax, [rdi + TCB_STACK_TOP]
    mov [r15 + tss64 + 4 - kmain], rax
    mov [r15 + sys_kernel_stack - kmain], rax

    ; Switch address space (CR3) if task has private page tables
    mov rax, [rdi + TCB_CR3]
    test rax, rax
    jnz .yield_has_cr3
    mov rax, 0x1000
.yield_has_cr3:
    mov rdx, cr3
    cmp rax, rdx
    je .yield_cr3_done
    mov cr3, rax
.yield_cr3_done:

    ; Restore FS_BASE (MSR 0xC0000100) for thread-local storage
    mov rax, [rdi + TCB_FS_BASE]
    mov rdx, rax
    shr rdx, 32
    mov ecx, 0xC0000100
    wrmsr

    ; Restore incoming task's FPU/SSE context
    fxrstor [rdi + TCB_FXSAVE]

    ; Switch to next task's stack!
    mov rsp, [rdi + TCB_RSP]

    jmp sched_restore

; ==============================================================================
; sched_restore: Unified context restore & return (pop 15 GPRs + iretq)
; ==============================================================================
align 16
sched_restore:
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq

; ==============================================================================
; sched_print_tasks: Display active tasks table in shell
; ==============================================================================
sched_print_tasks:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    mov eax, FB_CLR_HEADER
    call fb_console_set_color
    lea rsi, [r15 + str_task_hdr - kmain]
    call puts
    lea rsi, [r15 + str_task_sep - kmain]
    call puts
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color

    xor ebx, ebx                      ; ebx = task index (0..MAX_TASKS-1)
.pt_loop:
    cmp ebx, MAX_TASKS
    jae .pt_done

    imul eax, ebx, TCB_SIZE
    lea rsi, [r15 + task_table - kmain + rax]
    cmp dword [rsi + TCB_STATE], TASK_UNUSED
    je .pt_next

    ; Print TID
    mov eax, [rsi + TCB_ID]
    call putdec64
    mov al, ' '
    call putc
    call putc
    cmp dword [rsi + TCB_ID], 10
    jae @f
    call putc
@@:

    ; Print Name (padded to 16 chars)
    push rsi
    lea rsi, [rsi + TCB_NAME]
    xor ecx, ecx
.name_loop:
    lodsb
    test al, al
    jz .pad_name
    call putc
    inc ecx
    jmp .name_loop
.pad_name:
    mov al, ' '
    call putc
    inc ecx
    cmp ecx, 16
    jb .pad_name
    pop rsi

    ; Print State with color
    mov edx, [rsi + TCB_STATE]
    cmp edx, TASK_RUNNING
    jne @f
    mov eax, FB_CLR_SUCCESS
    call fb_console_set_color
    push rsi
    lea rsi, [r15 + str_st_running - kmain]
    call puts
    pop rsi
    jmp .st_printed
@@:
    cmp edx, TASK_SLEEPING
    jne @f
    mov eax, FB_CLR_LABEL
    call fb_console_set_color
    push rsi
    lea rsi, [r15 + str_st_sleeping - kmain]
    call puts
    pop rsi
    jmp .st_printed
@@:
    cmp edx, TASK_READY
    jne @f
    mov eax, 0xFF38BDF8               ; Sky Blue
    call fb_console_set_color
    push rsi
    lea rsi, [r15 + str_st_ready - kmain]
    call puts
    pop rsi
    jmp .st_printed
@@:
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    push rsi
    lea rsi, [r15 + str_st_dead - kmain]
    call puts
    pop rsi

.st_printed:
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    mov al, ' '
    call putc

    ; Print CPU Ticks
    mov rax, [rsi + TCB_TICKS]
    call putdec64
    mov al, ' '
    call putc
    call putc

    ; Print Stack RSP in hex
    mov al, '0'
    call putc
    mov al, 'x'
    call putc
    mov rax, [rsi + TCB_RSP]
    call puthex64

    mov al, 10
    call putc

.pt_next:
    inc ebx
    jmp .pt_loop

.pt_done:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; ==============================================================================
; task_clock_daemon: Background thread that updates menubar clock every 1 second
; ==============================================================================
task_clock_daemon:
.clk_loop:
    ; Sleep 1000 ms
    mov ecx, 1000
    call task_sleep

    ; Update clock string dynamically based on timer_ticks
    ; Total seconds = 52680 (14:38:00) + (timer_ticks / 1000)
    mov eax, [r15 + timer_ticks - kmain]
    xor edx, edx
    mov ecx, 1000
    div ecx                           ; eax = elapsed seconds
    add eax, 52680                    ; base 14:38:00

    ; Hours = (eax / 3600) % 24
    push rax
    xor edx, edx
    mov ecx, 3600
    div ecx
    xor edx, edx
    mov ecx, 24
    div ecx                           ; edx = hour
    mov eax, edx
    xor edx, edx
    mov ecx, 10
    div ecx                           ; al = ten, dl = unit
    add al, '0'
    add dl, '0'
    mov byte [r15 + str_md_clock - kmain + 0], al
    mov byte [r15 + str_md_clock - kmain + 1], dl
    mov byte [r15 + str_md_clock - kmain + 2], ':'
    pop rax

    ; Minutes = (eax / 60) % 60
    push rax
    xor edx, edx
    mov ecx, 60
    div ecx
    xor edx, edx
    mov ecx, 60
    div ecx                           ; edx = minute
    mov eax, edx
    xor edx, edx
    mov ecx, 10
    div ecx
    add al, '0'
    add dl, '0'
    mov byte [r15 + str_md_clock - kmain + 3], al
    mov byte [r15 + str_md_clock - kmain + 4], dl
    mov byte [r15 + str_md_clock - kmain + 5], ':'
    pop rax

    ; Seconds = eax % 60
    xor edx, edx
    mov ecx, 60
    div ecx                           ; edx = second
    mov eax, edx
    xor edx, edx
    mov ecx, 10
    div ecx
    add al, '0'
    add dl, '0'
    mov byte [r15 + str_md_clock - kmain + 6], al
    mov byte [r15 + str_md_clock - kmain + 7], dl
    mov byte [r15 + str_md_clock - kmain + 8], 0

    ; Redraw clock directly to menubar in VRAM LFB!
    call r2d_redraw_clock_lfb

    jmp .clk_loop

; ==============================================================================
; task_sysmon_daemon: Background thread that updates System Monitor in real time
; ==============================================================================
task_sysmon_daemon:
.sys_loop:
    ; Sleep 500 ms
    mov ecx, 500
    call task_sleep

    ; Check if System Monitor is open
    cmp byte [r15 + wm_state - kmain + APP_SYS], WM_STATE_OPEN
    jne .sys_loop

    ; Skip triggering full render if a window is being dragged
    cmp byte [r15 + wm_drag_app - kmain], 0xFF
    jne .sys_loop

    ; Trigger desktop render if sysmon is visible
    mov byte [r15 + md_term_dirty - kmain], 1

    jmp .sys_loop
