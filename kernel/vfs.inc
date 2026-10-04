; ==============================================================================
; Opensweet OS - Virtual File System & File Descriptor Subsystem (kernel/vfs.inc)
; Implements POSIX-compatible Process File Descriptor Table:
;   - Standard Streams: fd 0 = stdin, fd 1 = stdout, fd 2 = stderr
;   - Process File Descriptors: fd 3..15 for ext4 files and pipes
;   - System calls: open, close, read, write, lseek, fstat, stat
; ==============================================================================

MAX_FDS_PER_TASK = 16
FD_ENTRY_SIZE    = 48

; FD Types
FD_TYPE_UNUSED   = 0
FD_TYPE_CONSOLE  = 1   ; stdin/stdout/stderr
FD_TYPE_FILE     = 2   ; regular ext4 file
FD_TYPE_PIPE     = 3   ; pipe

; POSIX Open Flags
O_RDONLY         = 0
O_WRONLY         = 1
O_RDWR           = 2
O_CREAT          = 0x0040
O_TRUNC          = 0x0200
O_APPEND         = 0x0400

; POSIX Seek Whence
SEEK_SET         = 0
SEEK_CUR         = 1
SEEK_END         = 2

; Structure of fd_entry (48 bytes):
;   +0: uint8  type       (FD_TYPE_*)
;   +1: uint8  flags      (O_*)
;   +2: uint16 inode      (ext4 inode number or pipe_id)
;   +4: uint32 offset     (current byte offset)
;   +8: uint32 size       (cached file size)
;   +12: 36 bytes path    (ASCIIZ name)

MAX_PIPES        = 8
PIPE_BUF_SIZE    = 4096
PIPE_STRUCT_SIZE = 16 + PIPE_BUF_SIZE

align 16
task_fd_tables rb (MAX_TASKS * MAX_FDS_PER_TASK * FD_ENTRY_SIZE)
pipe_table     rb (MAX_PIPES * PIPE_STRUCT_SIZE)

; ==============================================================================
; vfs_init_task_fds: Initialize standard streams 0, 1, 2 and clear 3..15
; EBX = task slot ID (0..MAX_TASKS-1)
; ==============================================================================
vfs_init_task_fds:
    push rbx
    push rcx
    push rdi
    push rax

    imul eax, ebx, (MAX_FDS_PER_TASK * FD_ENTRY_SIZE)
    lea rdi, [r15 + task_fd_tables - kmain + rax]

    ; Clear entire table for this task
    push rdi
    mov ecx, (MAX_FDS_PER_TASK * FD_ENTRY_SIZE) / 4
    xor eax, eax
    rep stosd
    pop rdi

    ; FD 0: STDIN (Console read)
    mov byte [rdi + 0*FD_ENTRY_SIZE + 0], FD_TYPE_CONSOLE
    mov byte [rdi + 0*FD_ENTRY_SIZE + 1], O_RDONLY

    ; FD 1: STDOUT (Console write)
    mov byte [rdi + 1*FD_ENTRY_SIZE + 0], FD_TYPE_CONSOLE
    mov byte [rdi + 1*FD_ENTRY_SIZE + 1], O_WRONLY

    ; FD 2: STDERR (Console write)
    mov byte [rdi + 2*FD_ENTRY_SIZE + 0], FD_TYPE_CONSOLE
    mov byte [rdi + 2*FD_ENTRY_SIZE + 1], O_WRONLY

    pop rax
    pop rdi
    pop rcx
    pop rbx
    ret

; ==============================================================================
; vfs_get_fd_ptr: Get pointer to fd_entry for current task
; EDI = fd (0..MAX_FDS_PER_TASK-1)
; Returns: RAX = pointer to fd_entry (or 0 if invalid fd)
; ==============================================================================
vfs_get_fd_ptr:
    cmp edi, MAX_FDS_PER_TASK
    jae .bad_fd

    push rdx
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, eax, (MAX_FDS_PER_TASK * FD_ENTRY_SIZE)
    imul edx, edi, FD_ENTRY_SIZE
    add eax, edx
    pop rdx
    lea rax, [r15 + task_fd_tables - kmain + rax]
    ret

.bad_fd:
    xor eax, eax
    ret

; ==============================================================================
; vfs_open: Open a file and allocate a file descriptor
; RDI = path (ASCIIZ), ESI = flags, EDX = mode
; Returns: EAX = fd (3..15), or -1 on error
; ==============================================================================
vfs_open:
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

    mov r12, rdi                        ; path
    mov r13d, esi                       ; flags
    mov r14d, edx                       ; mode

    ; Validate path pointer (64 bytes max)
    mov rdi, r12
    mov rsi, 64
    call sys_validate_user_ptr
    jc .open_fail

    ; Find free FD in current task (3..15)
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, eax, (MAX_FDS_PER_TASK * FD_ENTRY_SIZE)
    lea rbx, [r15 + task_fd_tables - kmain + rax]

    mov ecx, 3                          ; start at fd 3
.find_fd:
    cmp ecx, MAX_FDS_PER_TASK
    jae .open_fail                      ; no free FDs!
    imul edx, ecx, FD_ENTRY_SIZE
    cmp byte [rbx + rdx + 0], FD_TYPE_UNUSED
    je .found_fd
    inc ecx
    jmp .find_fd

.found_fd:
    mov [rsp + 56], rcx                 ; save allocated fd number in scratch
    imul edx, ecx, FD_ENTRY_SIZE
    lea r14, [rbx + rdx]                ; r14 = pointer to fd_entry

    ; Copy path to local buffer and fd_entry structure (+12)
    mov rsi, r12
    lea rdi, [rsp]
    lea rdx, [r14 + 12]
    mov ecx, 35
.cp_p:
    lodsb
    stosb
    mov [rdx], al
    inc rdx
    test al, al
    jz .cp_p_done
    dec ecx
    jnz .cp_p
    mov byte [rdi], 0
    mov byte [rdx], 0
.cp_p_done:

    ; Skip leading '/'
    lea rsi, [rsp]
    cmp byte [rsi], '/'
    jne @f
    inc rsi
@@:

    ; Check if file exists in ext4
    call ext4_lookup
    test eax, eax
    jnz .have_inode

    ; File not found: check if O_CREAT (bit 6 = 0x40)
    test r13d, O_CREAT
    jz .open_fail

    ; Create new file
    call ext4_create_file
    cmp eax, -1
    je .open_fail

.have_inode:
    ; EAX = inode number
    mov word [r14 + 2], ax              ; store inode

    ; Load inode to get size
    call ext4_inode_load
    jc .open_fail

    mov rax, qword [r15 + ext4_inode - kmain + IN_OFF_SIZE]
    mov dword [r14 + 8], eax            ; file size

    ; Check O_TRUNC (0x200): reset size to 0
    test r13d, O_TRUNC
    jz @f
    mov dword [r14 + 8], 0
@@:

    ; Initial offset: 0 (or size if O_APPEND)
    mov dword [r14 + 4], 0
    test r13d, O_APPEND
    jz @f
    mov eax, [r14 + 8]
    mov dword [r14 + 4], eax
@@:

    ; Mark entry as active regular file
    mov byte [r14 + 0], FD_TYPE_FILE
    mov byte [r14 + 1], r13b            ; flags

    ; Return allocated FD
    mov rax, [rsp + 56]
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

.open_fail:
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
; vfs_close: Close a file descriptor
; EDI = fd
; Returns: EAX = 0 on success, -1 on error
; ==============================================================================
vfs_close:
    cmp edi, MAX_FDS_PER_TASK
    jae .close_err

    call vfs_get_fd_ptr
    test rax, rax
    jz .close_err

    ; If pipe, decrement reader/writer count
    cmp byte [rax + 0], FD_TYPE_PIPE
    jne @f
    movzx ecx, word [rax + 2]           ; pipe_id
    imul ecx, ecx, PIPE_STRUCT_SIZE
    lea rdx, [r15 + pipe_table - kmain + rcx]
    cmp byte [rax + 1], O_RDONLY
    jne .close_pipe_wr
    cmp word [rdx + 12], 0
    je @f
    dec word [rdx + 12]                 ; readers--
    jmp @f
.close_pipe_wr:
    cmp word [rdx + 14], 0
    je @f
    dec word [rdx + 14]                 ; writers--
@@:

    cmp edi, 3
    jae .close_normal

    ; For standard streams (0..2), restore default console
    mov byte [rax + 0], FD_TYPE_CONSOLE
    mov byte [rax + 1], O_RDONLY
    cmp edi, 0
    je .close_ok
    mov byte [rax + 1], O_WRONLY
    jmp .close_ok

.close_normal:
    mov byte [rax + 0], FD_TYPE_UNUSED
    xor eax, eax
    ret

.close_ok:
    xor eax, eax
    ret

.close_err:
    mov rax, -1
    ret

; ==============================================================================
; vfs_lseek: Reposition file read/write offset
; EDI = fd, RSI = offset, EDX = whence
; Returns: RAX = new offset, or -1 on error
; ==============================================================================
vfs_lseek:
    push rbx
    call vfs_get_fd_ptr
    test rax, rax
    jz .lseek_err
    cmp byte [rax + 0], FD_TYPE_FILE
    jne .lseek_err

    mov rbx, rax                        ; rbx = fd_entry

    cmp edx, SEEK_SET
    je .seek_set
    cmp edx, SEEK_CUR
    je .seek_cur
    cmp edx, SEEK_END
    je .seek_end
    jmp .lseek_err

.seek_set:
    mov rax, rsi
    jmp .seek_apply

.seek_cur:
    mov eax, [rbx + 4]
    add rax, rsi
    jmp .seek_apply

.seek_end:
    mov eax, [rbx + 8]
    add rax, rsi

.seek_apply:
    test rax, rax
    js .lseek_err                       ; negative offset not allowed
    mov [rbx + 4], eax                  ; update offset
    pop rbx
    ret

.lseek_err:
    pop rbx
    mov rax, -1
    ret

; ==============================================================================
; vfs_read_fd: Read bytes from fd
; EDI = fd, RSI = user buffer, RDX = count
; Returns: RAX = bytes read, 0 on EOF, -1 on error
; ==============================================================================
vfs_read_fd:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    call vfs_get_fd_ptr
    test rax, rax
    jz .rfd_err

    mov rbx, rax                        ; rbx = fd_entry

    cmp byte [rbx + 0], FD_TYPE_CONSOLE
    jne @f
    call sys_handler_read
    jmp .rfd_done
@@:

    cmp byte [rbx + 0], FD_TYPE_PIPE
    je .rfd_pipe

    cmp byte [rbx + 0], FD_TYPE_FILE
    jne .rfd_err

    ; Check EOF
    mov eax, [rbx + 4]                  ; current offset
    cmp eax, [rbx + 8]                  ; compare with size
    jae .rfd_eof

    ; Calculate bytes to read = min(count, size - offset)
    mov ecx, [rbx + 8]
    sub ecx, eax                        ; ecx = bytes left in file
    cmp rdx, rcx
    jbe @f
    mov rdx, rcx
@@:
    test rdx, rdx
    jz .rfd_eof

    ; Read chunk using ext4
    movzx eax, word [rbx + 2]           ; inode
    call ext4_inode_load
    jc .rfd_err

    ; Read file content into user buffer
    push rsi
    push rdx
    mov rdi, rsi                        ; rdi = user buffer
    mov rcx, rdx                        ; rcx = max bytes
    lea rsi, [rbx + 12]                 ; rsi = file path from fd_entry (+12)
    call ext4_read_file_to_mem          ; reads into memory buffer
    pop rdx
    pop rsi
    jc .rfd_err

    ; Advance offset
    add [rbx + 4], eax
    jmp .rfd_done

.rfd_pipe:
    movzx eax, word [rbx + 2]
    cmp eax, MAX_PIPES
    jae .rfd_err
    imul eax, eax, PIPE_STRUCT_SIZE
    lea r8, [r15 + pipe_table - kmain + rax]

    mov ecx, [r8 + 8]
    test ecx, ecx
    jnz .rfd_p_data
    ; Buffer empty: if writers == 0 -> EOF
    cmp word [r8 + 14], 0
    je .rfd_eof
    xor eax, eax
    jmp .rfd_done

.rfd_p_data:
    cmp rdx, rcx
    jbe @f
    mov rdx, rcx
@@:
    mov r9, rdx
    mov r10d, [r8 + 4]
    push rsi
.rfd_p_loop:
    test r9, r9
    jz .rfd_p_loop_done
    mov al, [r8 + 16 + r10]
    mov [rsi], al
    inc rsi
    inc r10d
    cmp r10d, PIPE_BUF_SIZE
    jb @f
    xor r10d, r10d
@@:
    dec dword [r8 + 8]
    dec r9
    jmp .rfd_p_loop
.rfd_p_loop_done:
    pop rsi
    mov [r8 + 4], r10d
    mov rax, rdx
    jmp .rfd_done

.rfd_eof:
    xor eax, eax
    jmp .rfd_done

.rfd_err:
    mov rax, -1

.rfd_done:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

; ==============================================================================
; vfs_write_fd: Write bytes to fd
; EDI = fd, RSI = user buffer, RDX = count
; Returns: RAX = bytes written, -1 on error
; ==============================================================================
vfs_write_fd:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    call vfs_get_fd_ptr
    test rax, rax
    jz .wfd_err

    mov rbx, rax

    cmp byte [rbx + 0], FD_TYPE_CONSOLE
    jne @f
    call sys_handler_write
    jmp .wfd_done
@@:

    cmp byte [rbx + 0], FD_TYPE_PIPE
    je .wfd_pipe

    cmp byte [rbx + 0], FD_TYPE_FILE
    jne .wfd_err

    ; Write via ext4_write_file
    movzx eax, word [rbx + 2]           ; inode
    call ext4_write_file
    cmp rax, -1
    je .wfd_err

    ; Advance offset and size
    add [rbx + 4], eax
    mov ecx, [rbx + 4]
    cmp ecx, [rbx + 8]
    jbe @f
    mov [rbx + 8], ecx                  ; grew file size
@@:
    jmp .wfd_done

.wfd_pipe:
    movzx eax, word [rbx + 2]
    cmp eax, MAX_PIPES
    jae .wfd_err
    imul eax, eax, PIPE_STRUCT_SIZE
    lea r8, [r15 + pipe_table - kmain + rax]

    cmp word [r8 + 12], 0              ; any readers left?
    je .wfd_err

    mov ecx, PIPE_BUF_SIZE
    sub ecx, [r8 + 8]
    test ecx, ecx
    jle .wfd_pipe_full

    cmp rdx, rcx
    jbe @f
    mov rdx, rcx
@@:
    mov r9, rdx
    mov r10d, [r8 + 0]                  ; head
    push rsi
.wfd_p_loop:
    test r9, r9
    jz .wfd_p_loop_done
    mov al, [rsi]
    mov [r8 + 16 + r10], al
    inc rsi
    inc r10d
    cmp r10d, PIPE_BUF_SIZE
    jb @f
    xor r10d, r10d
@@:
    inc dword [r8 + 8]
    dec r9
    jmp .wfd_p_loop
.wfd_p_loop_done:
    pop rsi
    mov [r8 + 0], r10d
    mov rax, rdx
    jmp .wfd_done

.wfd_pipe_full:
    xor eax, eax
    jmp .wfd_done

.wfd_err:
    mov rax, -1

.wfd_done:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

; ==============================================================================
; vfs_pipe: Create an inter-process pipe
; RDI = user pointer to int pipefd[2]
; Returns: EAX = 0 on success, -1 on error
; ==============================================================================
vfs_pipe:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15

    mov r12, rdi
    mov rsi, 8
    call sys_validate_user_ptr
    jc .pipe_fail

    ; Find free pipe slot
    xor ecx, ecx
.find_pipe:
    cmp ecx, MAX_PIPES
    jae .pipe_fail
    imul eax, ecx, PIPE_STRUCT_SIZE
    lea rdx, [r15 + pipe_table - kmain + rax]
    cmp word [rdx + 12], 0              ; readers == 0?
    jne .next_p
    cmp word [rdx + 14], 0              ; writers == 0?
    je .found_pipe
.next_p:
    inc ecx
    jmp .find_pipe

.found_pipe:
    mov r13d, ecx                       ; r13d = pipe_id
    lea r14, [rdx]                      ; r14 = pipe struct

    ; Find two free FDs in current task
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, eax, (MAX_FDS_PER_TASK * FD_ENTRY_SIZE)
    lea rbx, [r15 + task_fd_tables - kmain + rax]

    mov ecx, 3
    mov edx, -1
    mov esi, -1
.find_pipe_fds:
    cmp ecx, MAX_FDS_PER_TASK
    jae .pipe_fds_done
    imul eax, ecx, FD_ENTRY_SIZE
    cmp byte [rbx + rax + 0], FD_TYPE_UNUSED
    jne .skip_fd
    cmp edx, -1
    jne .found_fd_w
    mov edx, ecx
    jmp .skip_fd
.found_fd_w:
    mov esi, ecx
    jmp .pipe_fds_done
.skip_fd:
    inc ecx
    jmp .find_pipe_fds
.pipe_fds_done:
    cmp edx, -1
    je .pipe_fail
    cmp esi, -1
    je .pipe_fail

    ; Reset pipe buffer
    mov dword [r14 + 0], 0              ; head = 0
    mov dword [r14 + 4], 0              ; tail = 0
    mov dword [r14 + 8], 0              ; count = 0
    mov word  [r14 + 12], 1             ; readers = 1
    mov word  [r14 + 14], 1             ; writers = 1

    ; Configure fd_r
    imul eax, edx, FD_ENTRY_SIZE
    lea rdi, [rbx + rax]
    mov byte [rdi + 0], FD_TYPE_PIPE
    mov byte [rdi + 1], O_RDONLY
    mov word [rdi + 2], r13w
    mov dword [rdi + 4], 0
    mov dword [rdi + 8], 0

    ; Configure fd_w
    imul eax, esi, FD_ENTRY_SIZE
    lea rdi, [rbx + rax]
    mov byte [rdi + 0], FD_TYPE_PIPE
    mov byte [rdi + 1], O_WRONLY
    mov word [rdi + 2], r13w
    mov dword [rdi + 4], 0
    mov dword [rdi + 8], 0

    ; Store [edx, esi] into user pipefd[2]
    mov [r12 + 0], edx
    mov [r12 + 4], esi

    xor eax, eax
    jmp .pipe_done

.pipe_fail:
    mov rax, -1

.pipe_done:
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

; ==============================================================================
; vfs_dup2: Duplicate oldfd to newfd
; EDI = oldfd, ESI = newfd
; Returns: EAX = newfd, or -1 on error
; ==============================================================================
vfs_dup2:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    cmp edi, MAX_FDS_PER_TASK
    jae .dup_err
    cmp esi, MAX_FDS_PER_TASK
    jae .dup_err

    cmp edi, esi
    je .dup_same

    ; Get oldfd ptr
    call vfs_get_fd_ptr
    test rax, rax
    jz .dup_err
    cmp byte [rax + 0], FD_TYPE_UNUSED
    je .dup_err
    mov r8, rax                         ; r8 = oldfd ptr

    ; Close newfd first if open
    mov edi, esi
    call vfs_close

    ; Get newfd ptr
    mov edi, esi
    call vfs_get_fd_ptr
    mov r9, rax                         ; r9 = newfd ptr

    ; Copy 48 bytes from oldfd to newfd
    push rsi
    push rdi
    lea rsi, [r8]
    lea rdi, [r9]
    mov ecx, FD_ENTRY_SIZE / 4
    rep movsd
    pop rdi
    pop rsi

    ; If pipe, increment reader/writer refcount
    cmp byte [r9 + 0], FD_TYPE_PIPE
    jne @f
    movzx eax, word [r9 + 2]
    imul eax, eax, PIPE_STRUCT_SIZE
    lea rdx, [r15 + pipe_table - kmain + rax]
    cmp byte [r9 + 1], O_RDONLY
    jne .dup_pipe_w
    inc word [rdx + 12]                 ; readers++
    jmp @f
.dup_pipe_w:
    inc word [rdx + 14]                 ; writers++
@@:

    mov eax, esi                        ; return newfd
    jmp .dup_done

.dup_same:
    mov eax, esi
    jmp .dup_done

.dup_err:
    mov rax, -1

.dup_done:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

; ==============================================================================
; vfs_cleanup_task_fds: Close all open file descriptors (0..15) for current task
; ==============================================================================
vfs_cleanup_task_fds:
    push rbx
    push rdi
    xor edi, edi
.clean_loop:
    cmp edi, MAX_FDS_PER_TASK
    jae .clean_done
    call vfs_close
    inc edi
    jmp .clean_loop
.clean_done:
    pop rdi
    pop rbx
    ret

; ==============================================================================
; vfs_redirect_child_fd: Redirect child stream to parent's open file descriptor
; EBX = child slot ID (1..MAX_TASKS-1)
; EAX = parent slot ID (0..MAX_TASKS-1)
; ECX = child fd (0, 1, or 2)
; EDX = parent fd (0..MAX_FDS_PER_TASK-1)
; ==============================================================================
vfs_redirect_child_fd:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    cmp edx, MAX_FDS_PER_TASK
    jae .rc_done
    cmp ecx, 3
    jae .rc_done

    ; RSI = parent's fd_entry
    push rdx
    imul eax, eax, (MAX_FDS_PER_TASK * FD_ENTRY_SIZE)
    imul edx, edx, FD_ENTRY_SIZE
    add eax, edx
    lea rsi, [r15 + task_fd_tables - kmain + rax]
    pop rdx

    cmp byte [rsi + 0], FD_TYPE_UNUSED
    je .rc_done

    ; RDI = child's fd_entry
    imul eax, ebx, (MAX_FDS_PER_TASK * FD_ENTRY_SIZE)
    imul ecx, ecx, FD_ENTRY_SIZE
    add eax, ecx
    lea rdi, [r15 + task_fd_tables - kmain + rax]

    ; Copy FD_ENTRY_SIZE bytes
    push rcx
    mov ecx, FD_ENTRY_SIZE / 4
    rep movsd
    pop rcx

    ; If pipe, increment reader/writer refcount
    cmp byte [rdi + 0], FD_TYPE_PIPE
    jne .rc_done
    movzx eax, word [rdi + 2]
    imul eax, eax, PIPE_STRUCT_SIZE
    lea rdx, [r15 + pipe_table - kmain + rax]
    cmp byte [rdi + 1], O_RDONLY
    jne .rc_pipe_wr
    inc word [rdx + 12]                 ; readers++
    jmp .rc_done
.rc_pipe_wr:
    inc word [rdx + 14]                 ; writers++

.rc_done:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret
