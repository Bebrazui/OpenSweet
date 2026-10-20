; ==============================================================================
; kernel/tty.inc - minimal terminal (pty) layer on top of pipes
;
; A "tty" here is a pair of pipes plus termios state:
;   in_pipe  : terminal (keyboard) -> child stdin
;   out_pipe : child stdout/stderr -> terminal
;
; Writes to the keyboard side get CR->NL translation and optional echo, then
; the bytes go straight into the input pipe.  isatty()/TCGETS/TCSETS work and
; let readline drive an interactive shell.  There is no controlling terminal,
; session or process-group layer yet; TIOCGPGRP/TIOCSPGRP are accepted so bash
; does not bail out of job-control setup.
; ==============================================================================

MAX_TTYS        = 4

TTY_IN_PIPE     = 0
TTY_OUT_PIPE    = 4
TTY_IFLAG       = 8
TTY_OFLAG       = 12
TTY_CFLAG       = 16
TTY_LFLAG       = 20
TTY_STAGE_LEN   = 24
TTY_STAGE       = 32
TTY_SIZE        = 32 + 256

TTY_ICANON      = 0x0002
TTY_ECHO        = 0x0008

; ==============================================================================
; tty_bind: attach a tty to an input/output pipe pair
; EAX = input pipe id (terminal -> child), EDX = output pipe id
; Returns: EAX = 0 on success, -1 when no slot is free
; ==============================================================================
tty_bind:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    cmp byte [r15 + tty_ready - kmain], 0
    jne .tb_search
    mov byte [r15 + tty_ready - kmain], 1
    xor ecx, ecx
.tb_init:
    cmp ecx, MAX_TTYS
    jae .tb_search
    imul esi, ecx, TTY_SIZE
    mov dword [r15 + tty_table - kmain + rsi + TTY_IN_PIPE], -1
    inc ecx
    jmp .tb_init

.tb_search:
    xor ecx, ecx
.tb_find:
    cmp ecx, MAX_TTYS
    jae .tb_fail
    imul esi, ecx, TTY_SIZE
    cmp dword [r15 + tty_table - kmain + rsi + TTY_IN_PIPE], -1
    je .tb_found
    inc ecx
    jmp .tb_find

.tb_found:
    lea rbx, [r15 + tty_table - kmain + rsi]
    mov [rbx + TTY_IN_PIPE], eax
    mov [rbx + TTY_OUT_PIPE], edx
    mov dword [rbx + TTY_IFLAG], 0x0500       ; ICRNL | IXON
    mov dword [rbx + TTY_OFLAG], 0x0005       ; OPOST | ONLCR
    mov dword [rbx + TTY_CFLAG], 0x00BF       ; CS8 | CREAD
    mov dword [rbx + TTY_LFLAG], 0x000B       ; ISIG | ICANON | ECHO
    mov dword [rbx + TTY_STAGE_LEN], 0

    ; tty_pipe_map stores tty id + 1 so that zero still means "not a tty".
    lea rdi, [r15 + tty_pipe_map - kmain]
    mov r8d, ecx
    inc r8d
    mov [rdi + rax], r8b
    mov [rdi + rdx], r8b

    xor eax, eax
    jmp .tb_out
.tb_fail:
    mov eax, -1
.tb_out:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

; ==============================================================================
; tty_forget_pipe: a pipe slot is being reused, drop any stale tty binding
; EDI = pipe id
; ==============================================================================
tty_forget_pipe:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi

    cmp edi, MAX_PIPES
    jae .fp_out
    movzx eax, byte [r15 + tty_pipe_map - kmain + rdi]
    test eax, eax
    jz .fp_out
    dec eax
    mov byte [r15 + tty_pipe_map - kmain + rdi], 0
    imul esi, eax, TTY_SIZE
    lea rbx, [r15 + tty_table - kmain + rsi]
    cmp dword [rbx + TTY_IN_PIPE], -1
    je .fp_out

    ; clear the map entry of the partner pipe as well
    mov ecx, [rbx + TTY_IN_PIPE]
    cmp ecx, MAX_PIPES
    jae @f
    mov byte [r15 + tty_pipe_map - kmain + rcx], 0
@@:
    mov ecx, [rbx + TTY_OUT_PIPE]
    cmp ecx, MAX_PIPES
    jae @f
    mov byte [r15 + tty_pipe_map - kmain + rcx], 0
@@:
    mov dword [rbx + TTY_IN_PIPE], -1
.fp_out:
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; ==============================================================================
; tty_from_fd: which tty backs this fd entry?
; RDI = fd entry, Returns: EAX = tty id or -1
; ==============================================================================
tty_from_fd:
    cmp byte [rdi + 0], FD_TYPE_PIPE
    jne .tf_no
    movzx eax, word [rdi + 2]
    cmp eax, MAX_PIPES
    jae .tf_no
    movzx eax, byte [r15 + tty_pipe_map - kmain + rax]
    test eax, eax
    jz .tf_no
    dec eax
    ret
.tf_no:
    mov eax, -1
    ret

; ==============================================================================
; tty_try_input: line discipline for writes to the keyboard side.
; EDI = pipe id, RSI = buffer, RDX = count
; Returns: CF = 1 when the pipe is a tty input (RAX = bytes accepted),
;          CF = 0 when it is an ordinary pipe.
; Kept flat (no calls) so the stack frame stays trivially balanced.
; ==============================================================================
tty_try_input:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    mov r11, rdx                    ; r11 = original byte count
    cmp edi, MAX_PIPES
    jae .ti_no
    movzx eax, byte [r15 + tty_pipe_map - kmain + rdi]
    test eax, eax
    jz .ti_no
    dec eax
    imul ebx, eax, TTY_SIZE
    lea rbx, [r15 + tty_table - kmain + rbx]
    cmp dword [rbx + TTY_IN_PIPE], edi
    jne .ti_no

    mov eax, [rbx + TTY_IN_PIPE]
    imul eax, eax, PIPE_STRUCT_SIZE
    lea r8, [r15 + pipe_table - kmain + rax]   ; r8 = input pipe struct

.ti_loop:
    test rdx, rdx
    jz .ti_ok
    mov eax, [r8 + 8]               ; bytes buffered
    cmp eax, PIPE_BUF_SIZE
    jae .ti_ok                      ; pipe full: drop the rest

    mov al, [rsi]
    inc rsi
    dec rdx
    cmp al, 13                      ; ICRNL: CR -> NL
    jne .ti_store
    mov al, 10
.ti_store:
    mov ecx, [r8 + 0]               ; head
    and ecx, PIPE_BUF_SIZE - 1
    mov [r8 + 16 + rcx], al
    inc ecx
    and ecx, PIPE_BUF_SIZE - 1
    mov [r8 + 0], ecx
    inc dword [r8 + 8]

    ; ECHO: mirror the byte back to the terminal's output pipe.
    test dword [rbx + TTY_LFLAG], TTY_ECHO
    jz .ti_loop
    mov ecx, [rbx + TTY_OUT_PIPE]
    cmp ecx, MAX_PIPES
    jae .ti_loop
    imul ecx, ecx, PIPE_STRUCT_SIZE
    lea r10, [r15 + pipe_table - kmain + rcx]
    mov ecx, [r10 + 8]
    cmp ecx, PIPE_BUF_SIZE
    jae .ti_loop
    mov ecx, [r10 + 0]
    and ecx, PIPE_BUF_SIZE - 1
    mov [r10 + 16 + rcx], al
    inc ecx
    and ecx, PIPE_BUF_SIZE - 1
    mov [r10 + 0], ecx
    inc dword [r10 + 8]
    jmp .ti_loop

.ti_ok:
    mov rdx, r11
    mov rax, r11
    stc
    jmp .ti_out
.ti_no:
    clc
.ti_out:
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
align 16
tty_table       rb (MAX_TTYS * TTY_SIZE)
tty_pipe_map    rb MAX_PIPES
tty_ready       db 0
