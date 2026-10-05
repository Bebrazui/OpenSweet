; ==============================================================================
; Opensweet OS - First 64-bit Ring 3 User Application (kernel/test_user.inc)
; Assembled for execution at virtual address 0x400000 with user stack at 0x1FFFFF00.
; Uses 100% pure syscall / sysret interface!
; ==============================================================================

align 16
user_app_bin:
    ; 1. Print welcome banner from Ring 3
    mov eax, 1                        ; SYS_WRITE
    mov edi, 1                        ; stdout (fd=1)
    mov esi, 0x400000 + (.msg_welcome - user_app_bin)
    mov edx, .msg_welcome_len
    syscall

    ; 2. Run heartbeat loop (4 ticks with 400ms sleep)
    mov ebx, 4
.heartbeat_loop:
    ; Write heartbeat message
    mov eax, 1                        ; SYS_WRITE
    mov edi, 1
    mov esi, 0x400000 + (.msg_beat - user_app_bin)
    mov edx, .msg_beat_len
    syscall

    ; Sleep 400 ms
    mov eax, 3                        ; SYS_SLEEP
    mov edi, 400
    syscall

    dec ebx
    jnz .heartbeat_loop

    ; 3. Print goodbye message
    mov eax, 1                        ; SYS_WRITE
    mov edi, 1
    mov esi, 0x400000 + (.msg_exit - user_app_bin)
    mov edx, .msg_exit_len
    syscall

    ; 4. Exit cleanly
    mov eax, 0                        ; SYS_EXIT
    xor edi, edi                      ; exit code 0
    syscall

    ; Safety halt loop (should never be reached)
.u_halt:
    jmp .u_halt

.msg_welcome:
    db 10, "[Ring 3 User Mode] Process started successfully! (CPL=3)", 10
    db "[Ring 3 User Mode] Memory mapped @ 0x400000, Stack @ 0x1FFFFF00", 10, 0
.msg_welcome_len = $ - .msg_welcome - 1

.msg_beat:
    db "[Ring 3 User Mode] Heartbeat tick via sys_write + sys_sleep...", 10, 0
.msg_beat_len = $ - .msg_beat - 1

.msg_exit:
    db "[Ring 3 User Mode] Execution completed. Calling sys_exit...", 10, 0
.msg_exit_len = $ - .msg_exit - 1

user_app_bin_end:
USER_APP_BIN_SIZE = user_app_bin_end - user_app_bin
