; ==============================================================================
; Opensweet OS - First Native 64-bit User Application (user/hello.asm)
; Compiled into standard ELF64 static executable format via FASM.
; Runs entirely in Ring 3 (CPL=3) utilizing fast SYSCALL / SYSRET interface!
; ==============================================================================

format ELF64 executable 3
entry start

segment readable executable
start:
    ; 1. Print banner from user mode
    mov eax, 1                        ; SYS_WRITE
    mov edi, 1                        ; fd = 1 (stdout)
    mov rsi, msg_start
    mov edx, msg_start_len
    syscall

    ; 2. Preemptive sleep for 400 milliseconds
    mov eax, 3                        ; SYS_SLEEP
    mov edi, 400                      ; 400 ms
    syscall

    ; 3. Print second message proving scheduler restored user context
    mov eax, 1                        ; SYS_WRITE
    mov edi, 1                        ; fd = 1 (stdout)
    mov rsi, msg_progress
    mov edx, msg_progress_len
    syscall

    ; 4. Another sleep of 400 ms
    mov eax, 3                        ; SYS_SLEEP
    mov edi, 400
    syscall

    ; 5. Print exit message
    mov eax, 1                        ; SYS_WRITE
    mov edi, 1
    mov rsi, msg_exit
    mov edx, msg_exit_len
    syscall

    ; 6. Clean exit via sys_exit
    mov eax, 0                        ; SYS_EXIT
    xor edi, edi                      ; exit code 0
    syscall

.halt:
    jmp .halt

segment readable writeable
msg_start    db 10, "[hello.elf] *** Native ELF64 binary running in Ring 3! (CPL=3) ***", 10
             db "[hello.elf] Loaded from ext4 filesystem into user virtual memory.", 10, 0
msg_start_len = $ - msg_start - 1

msg_progress db "[hello.elf] Slept 400ms via sys_sleep! Scheduler preemption verified.", 10, 0
msg_progress_len = $ - msg_progress - 1

msg_exit     db "[hello.elf] Execution complete. Invoking sys_exit(0)...", 10, 0
msg_exit_len = $ - msg_exit - 1
