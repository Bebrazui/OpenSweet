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

; TCB offsets (64 bytes per TCB)
TCB_RSP         = 0x00                ; uint64: Saved stack pointer
TCB_STATE       = 0x08                ; uint32: TASK_* state
TCB_ID          = 0x0C                ; uint32: Task ID
TCB_SLEEP       = 0x10                ; uint32: Sleep ticks remaining (ms)
TCB_SLICE       = 0x14                ; uint32: Remaining quantum in ticks
TCB_TICKS       = 0x18                ; uint64: Total CPU ticks accumulated
TCB_STACK_BASE  = 0x20                ; uint64: Bottom of stack memory
TCB_STACK_TOP   = 0x28                ; uint64: Top of stack memory
TCB_NAME        = 0x30                ; 16 bytes: ASCIIZ task name
TCB_SIZE        = 64

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

    ; Stack 0 uses initial boot stack 0x90000
    mov qword [rbx + TCB_STACK_BASE], 0x80000
    mov qword [rbx + TCB_STACK_TOP],  0x90000

    ; Copy name "shell"
    lea rsi, [r15 + str_name_shell - kmain]
    lea rdi, [rbx + TCB_NAME]
    mov ecx, 6
    rep movsb

    mov dword [r15 + sched_curr_task - kmain], 0
    mov dword [r15 + sched_task_cnt - kmain], 1
    mov dword [r15 + sched_enabled - kmain], 1

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

    ; Initialize stack frame to simulate hardware interrupt return (iretq)
    ; Hardware frame: SS, RSP, RFLAGS, CS, RIP
    ; Followed by 15 GPRs: RAX, RBX, RCX, RDX, RSI, RDI, RBP, R8..R15
    mov r9, r8                        ; r9 = current top of stack

    ; Push SS (0x10)
    sub r9, 8
    mov qword [r9], 0x10

    ; Push RSP (initial stack top)
    sub r9, 8
    mov qword [r9], r8

    ; Push RFLAGS (0x202 = IF=1, interrupts enabled)
    sub r9, 8
    mov qword [r9], 0x202

    ; Push CS (0x18 = 64-bit kernel code selector)
    sub r9, 8
    mov qword [r9], 0x18

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

    ; Mark task ready
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
    mov dword [rbx + TCB_STATE], TASK_DEAD
    dec dword [r15 + sched_task_cnt - kmain]
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
    ; Save current task RSP
    mov [rsi + TCB_RSP], rsp

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

    ; Save current task RSP
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, TCB_SIZE
    lea rsi, [r15 + task_table - kmain + rax]
    mov [rsi + TCB_RSP], rsp

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

    ; Trigger desktop render if sysmon is visible
    mov byte [r15 + md_term_dirty - kmain], 1

    jmp .sys_loop
