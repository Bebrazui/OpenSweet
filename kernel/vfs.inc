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
FD_TYPE_DEV_NULL = 4   ; /dev/null, /dev/zero, /dev/urandom

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
; vfs_lookup_with_fallback: lookup path in ext4; if not found and contains '/',
; fallback to looking up just the basename in the root directory.
; RSI = path (ASCIIZ)
; Returns: EAX = inode or 0. Preserves RSI.
; ==============================================================================
vfs_lookup_with_fallback:
    push rsi
    push rcx
    call ext4_lookup
    test eax, eax
    jnz .lwf_done

    ; If lookup failed, find basename (character after last '/')
    mov rcx, [rsp + 8]                ; original rsi
    xor eax, eax                      ; eax = last slash ptr (or 0)
.lwf_scan:
    mov dl, [rcx]
    test dl, dl
    jz .lwf_scan_done
    cmp dl, '/'
    jne @f
    lea rax, [rcx + 1]
@@:
    inc rcx
    jmp .lwf_scan
.lwf_scan_done:
    test rax, rax
    jz .lwf_fail                      ; no slash in path
    cmp byte [rax], 0
    jz .lwf_fail                      ; trailing slash with no name
    mov rsi, rax
    call ext4_lookup
    jmp .lwf_done

.lwf_fail:
    xor eax, eax
.lwf_done:
    pop rcx
    pop rsi
    ret

; ==============================================================================
; vfs_open: Open a file and allocate a file descriptor
; RDI = path (ASCIIZ), ESI = flags, EDX = mode
; Returns: EAX = fd (3..15), or -1 on error
; ==============================================================================
vfs_open:
    pushfq
    cli
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
    ; Check for virtual devices: /dev/null, /dev/zero, /dev/urandom
    cmp dword [rsp], '/dev'
    jne .not_dev
    cmp word [rsp + 4], '/n'
    je .is_dev_null
    cmp word [rsp + 4], '/z'
    je .is_dev_null
    cmp word [rsp + 4], '/u'
    je .is_dev_null
    jmp .not_dev

.is_dev_null:
    mov byte [r14 + 0], FD_TYPE_DEV_NULL
    mov byte [r14 + 1], r13b            ; flags
    mov word [r14 + 2], 0               ; inode 0
    mov dword [r14 + 4], 0              ; offset 0
    mov dword [r14 + 8], 0              ; size 0
    mov rax, [rsp + 56]                 ; allocated fd
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
    popfq
    ret

.not_dev:
    ; Check if file exists in ext4
    lea rsi, [rsp]
    call vfs_lookup_with_fallback
    test eax, eax
    jnz .have_inode

    ; File not found: check if O_CREAT (bit 6 = 0x40)
    test r13d, O_CREAT
    jz .open_fail

    ; Create new file
    lea rsi, [rsp]
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

    ; Check O_TRUNC (0x200): truncate file to 0 on disk
    test r13d, O_TRUNC
    jz @f
    push rax
    movzx eax, word [r14 + 2]           ; inode
    xor edx, edx                        ; new size = 0
    call ext4_truncate_file
    pop rax
    mov dword [r14 + 8], 0              ; reset cached size
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
    popfq
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
    popfq
    mov rax, -1
    ret

; ==============================================================================
; vfs_stat_fill: fill struct stat (144 bytes, x86_64 layout) at RDI from the
; loaded ext4_inode. EAX = inode number (st_ino). Clobbers rax, rcx.
; ==============================================================================
vfs_stat_fill:
    push rdi
    push rax
    push rcx
    mov rcx, 144 / 8
    xor eax, eax
    rep stosq                         ; zero the buffer
    pop rcx
    pop rax
    sub rdi, 144                      ; rep stosq advanced rdi
    mov [rdi + 8], rax                ; st_ino
    movzx ecx, word [r15 + ext4_inode - kmain + 26]
    mov [rdi + 16], rcx               ; st_nlink = i_links_count
    movzx ecx, word [r15 + ext4_inode - kmain + 0]
    mov [rdi + 24], ecx               ; st_mode = i_mode
    movzx ecx, word [r15 + ext4_inode - kmain + 2]
    mov [rdi + 28], ecx               ; st_uid = i_uid
    movzx ecx, word [r15 + ext4_inode - kmain + 24]
    mov [rdi + 32], ecx               ; st_gid = i_gid
    mov ecx, dword [r15 + ext4_inode - kmain + 4]
    mov [rdi + 48], ecx               ; st_size low
    mov ecx, dword [r15 + ext4_inode - kmain + 108]
    mov [rdi + 52], ecx               ; st_size high
    mov qword [rdi + 56], 1024        ; st_blksize
    mov ecx, dword [r15 + ext4_inode - kmain + 28]
    mov [rdi + 64], rcx               ; st_blocks (512B sectors)
    mov ecx, dword [r15 + ext4_inode - kmain + 8]
    mov [rdi + 72], rcx               ; st_atim.tv_sec
    mov ecx, dword [r15 + ext4_inode - kmain + 16]
    mov [rdi + 88], rcx               ; st_mtim.tv_sec
    mov ecx, dword [r15 + ext4_inode - kmain + 12]
    mov [rdi + 104], rcx              ; st_ctim.tv_sec
    pop rdi
    ret

; ==============================================================================
; vfs_stat_path: stat a path. RDI = pathname, RSI = statbuf (user, 144 bytes).
; Returns EAX = 0 or -errno. Clobbers rbx, r12, r13.
; ==============================================================================
vfs_stat_path:
    push rbx
    push r12
    push r13
    mov r12, rdi
    mov r13, rsi
    mov rdi, r13
    mov esi, 144
    call sys_validate_user_ptr
    jc .statp_efault
    mov rdi, r12
    mov esi, 64
    call sys_validate_user_ptr
    jc .statp_efault
    mov rsi, r12
    call vfs_lookup_with_fallback
    test eax, eax
    jz .statp_enoent
    call ext4_inode_load
    jc .statp_efault
    sub rsp, 144
    mov rdi, rsp
    call vfs_stat_fill
    mov rsi, rsp
    mov rdi, r13
    mov ecx, 144 / 8
    rep movsq
    add rsp, 144
    xor eax, eax
    jmp .statp_done
.statp_enoent:
    mov eax, LINUX_ENOENT
    neg rax
    jmp .statp_done
.statp_efault:
    mov eax, LINUX_EFAULT
    neg rax
.statp_done:
    pop r13
    pop r12
    pop rbx
    ret

; ==============================================================================
; vfs_stat_fd: stat an open fd. EDI = fd, RSI = statbuf (user, 144 bytes).
; Returns EAX = 0 or -errno.
; ==============================================================================
vfs_stat_fd:
    push rbx
    push r12
    push r13
    mov r12d, edi
    mov r13, rsi
    mov rdi, r13
    mov esi, 144
    call sys_validate_user_ptr
    jc .statf_efault
    mov edi, r12d
    call vfs_get_fd_ptr
    test rax, rax
    jz .statf_badfd
    mov rbx, rax
    cmp byte [rbx + 0], FD_TYPE_FILE
    jne .statf_badfd
    movzx eax, word [rbx + 2]         ; inode number
    call ext4_inode_load
    jc .statf_efault
    sub rsp, 144
    mov rdi, rsp
    call vfs_stat_fill
    mov rsi, rsp
    mov rdi, r13
    mov ecx, 144 / 8
    rep movsq
    add rsp, 144
    xor eax, eax
    jmp .statf_done
.statf_badfd:
    mov eax, LINUX_EBADF
    neg rax
    jmp .statf_done
.statf_efault:
    mov eax, LINUX_EFAULT
    neg rax
.statf_done:
    pop r13
    pop r12
    pop rbx
    ret

; ==============================================================================
; vfs_dup: duplicate oldfd to the first free fd >= 3. EDI = oldfd.
; Returns EAX = new fd or -errno.
; ==============================================================================
vfs_dup:
    pushfq
    cli
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r12
    mov r12d, edi                     ; r12d = oldfd
    call vfs_get_fd_ptr
    test rax, rax
    jz .dup_badfd
    mov rbx, rax                      ; rbx = old fd_entry
    mov eax, [r15 + sched_curr_task - kmain]
    imul eax, eax, (MAX_FDS_PER_TASK * FD_ENTRY_SIZE)
    lea rdx, [r15 + task_fd_tables - kmain + rax]  ; rdx = fd table base
    mov ecx, 3
.dup_find:
    cmp ecx, MAX_FDS_PER_TASK
    jae .dup_badfd
    imul eax, ecx, FD_ENTRY_SIZE
    cmp byte [rdx + rax + 0], FD_TYPE_UNUSED
    je .dup_found
    inc ecx
    jmp .dup_find
.dup_found:
    ; copy old fd_entry to new slot
    imul eax, ecx, FD_ENTRY_SIZE
    lea rdi, [rdx + rax]
    mov rsi, rbx
    mov ecx, FD_ENTRY_SIZE / 8
    rep movsq
    ; bump pipe refcounts if it's a pipe
    cmp byte [rdi - FD_ENTRY_SIZE + 0], FD_TYPE_PIPE
    jne .dup_done
    movzx eax, word [rdi - FD_ENTRY_SIZE + 2]
    imul eax, eax, PIPE_STRUCT_SIZE
    lea rdx, [r15 + pipe_table - kmain + rax]
    cmp byte [rdi - FD_ENTRY_SIZE + 1], O_RDONLY
    jne .dup_wr
    inc word [rdx + 12]               ; readers++
    jmp .dup_done
.dup_wr:
    inc word [rdx + 14]               ; writers++
.dup_done:
    mov eax, ecx                      ; return new fd
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    popfq
    ret
.dup_badfd:
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    popfq
    mov eax, LINUX_EBADF
    neg rax
    ret

; ==============================================================================
; vfs_close: Close a file descriptor
; EDI = fd
; Returns: EAX = 0 on success, -1 on error
; ==============================================================================
vfs_close:
    pushfq
    cli
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
    popfq
    ret

.close_ok:
    xor eax, eax
    popfq
    ret

.close_err:
    popfq
    mov rax, -1
    ret

; ==============================================================================
; vfs_lseek: Reposition file read/write offset
; EDI = fd, RSI = offset, EDX = whence
; Returns: RAX = new offset, or -1 on error
; ==============================================================================
vfs_lseek:
    pushfq
    cli
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
    popfq
    ret

.lseek_err:
    pop rbx
    popfq
    mov rax, -1
    ret

; ==============================================================================
; Last failure code of vfs_read_fd/vfs_write_fd: a *positive* POSIX errno
; (EBADF, EFAULT, EPIPE, EIO).  The Linux compat layer negates it into the
; -errno form ring3 expects; native syscalls keep returning plain -1.
; ==============================================================================
vfs_last_err: dd 5

; ==============================================================================
; vfs_read_fd: Read bytes from fd
; EDI = fd, RSI = user buffer, RDX = count
; Returns: RAX = bytes read, 0 on EOF, -1 on error
; ==============================================================================
vfs_read_fd:
    pushfq
    cli
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    ; Fast path for count == 0
    test rdx, rdx
    jz .rfd_zero

    ; Clamp count to max 64KB (65536)
    cmp rdx, 65536
    jbe @f
    mov rdx, 65536
@@:

    ; Validate user buffer [rsi .. rsi + rdx]
    push rdi
    push rsi
    mov rdi, rsi                        ; rdi = user buffer
    mov rsi, rdx                        ; rsi = count
    call sys_validate_user_ptr
    pop rsi
    pop rdi
    jc .rfd_err_fault

    call vfs_get_fd_ptr
    test rax, rax
    jz .rfd_err_badfd

    mov rbx, rax                        ; rbx = fd_entry

    cmp byte [rbx + 0], FD_TYPE_CONSOLE
    jne @f
    popfq                               ; restore IF for console read
    call sys_handler_read
    pushfq
    cli
    jmp .rfd_done
@@:

    cmp byte [rbx + 0], FD_TYPE_PIPE
    je .rfd_pipe

    cmp byte [rbx + 0], FD_TYPE_DEV_NULL
    je .rfd_dev_null

    cmp byte [rbx + 0], FD_TYPE_FILE
    jne .rfd_err_badfd

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

    ; Read chunk using ext4_read_file_chunk_inode starting from file offset [rbx + 4]
    push rsi
    push rdx
    mov rdi, rsi                        ; rdi = user buffer
    mov rcx, rdx                        ; rcx = max bytes to read
    mov edx, [rbx + 4]                  ; rdx = current file offset
    movzx eax, word [rbx + 2]           ; eax = file inode from fd_entry (+2)
    call ext4_read_file_chunk_inode     ; reads chunk starting at offset rdx directly by inode
    pop rdx
    pop rsi
    jc .rfd_err_io

    ; Advance offset by bytes actually read
    add [rbx + 4], eax
    jmp .rfd_done

.rfd_pipe:
    movzx eax, word [rbx + 2]
    cmp eax, MAX_PIPES
    jae .rfd_err_badfd
    imul eax, eax, PIPE_STRUCT_SIZE
    lea r8, [r15 + pipe_table - kmain + rax]

    mov ecx, [r8 + 8]
    test ecx, ecx
    jnz .rfd_p_data
    ; Buffer empty: writers == 0 -> EOF
    cmp word [r8 + 14], 0
    je .rfd_eof
    ; Empty pipe with live writers: POSIX read blocks until data or EOF
    call sig_probe_actionable        ; signal pending? -> EINTR instead of block
    test eax, eax
    jnz .rfd_err_intr
    sti
    mov ecx, 2
    call task_sleep           ; preserves rax/rbx; context switch preserves the rest
    cli
    jmp .rfd_pipe

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

.rfd_zero:
    xor eax, eax
    jmp .rfd_done

.rfd_eof:
    xor eax, eax
    jmp .rfd_done

.rfd_dev_null:
    ; rsi = user buffer, rdx = count
    ; If pathname is /dev/null, return 0 (EOF immediately)
    cmp byte [rbx + 12 + 5], 'n'
    je .rfd_eof
    ; For /dev/zero or /dev/urandom: fill with zeroes / pseudo-random bytes
    push rdi
    push rcx
    push rax
    mov rdi, rsi
    mov rcx, rdx
    xor eax, eax
    rep stosb
    pop rax
    pop rcx
    pop rdi
    mov rax, rdx
    jmp .rfd_done

.rfd_err_fault:
    mov dword [r15 + vfs_last_err - kmain], 14      ; EFAULT
    jmp .rfd_err
.rfd_err_intr:
    mov dword [r15 + vfs_last_err - kmain], 4       ; EINTR
    jmp .rfd_err
.rfd_err_badfd:
    mov dword [r15 + vfs_last_err - kmain], 9       ; EBADF
    jmp .rfd_err
.rfd_err_io:
    mov dword [r15 + vfs_last_err - kmain], 5       ; EIO
.rfd_err:
    mov rax, -1

.rfd_done:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    popfq
    ret

; ==============================================================================
; vfs_write_fd: Write bytes to fd
; EDI = fd, RSI = user buffer, RDX = count
; Returns: RAX = bytes written, -1 on error
; ==============================================================================
vfs_write_fd:
    pushfq
    cli
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    ; Fast path for count == 0
    test rdx, rdx
    jz .wfd_zero

    ; Clamp count to max 64KB (65536)
    cmp rdx, 65536
    jbe @f
    mov rdx, 65536
@@:

    ; Validate user buffer [rsi .. rsi + rdx]
    push rdi
    push rsi
    mov rdi, rsi                        ; rdi = user buffer
    mov rsi, rdx                        ; rsi = count
    call sys_validate_user_ptr
    pop rsi
    pop rdi
    jc .wfd_err_fault

    call vfs_get_fd_ptr
    test rax, rax
    jz .wfd_err_badfd
    mov rbx, rax

    cmp byte [rbx + 0], FD_TYPE_CONSOLE
    jne @f
    call sys_handler_write
    jmp .wfd_done
@@:

    cmp byte [rbx + 0], FD_TYPE_PIPE
    je .wfd_pipe

    cmp byte [rbx + 0], FD_TYPE_DEV_NULL
    je .wfd_dev_null

    cmp byte [rbx + 0], FD_TYPE_FILE
    jne .wfd_err_badfd

    ; Write via ext4_write_file_chunk at current offset [rbx + 4]
    push rdx                            ; rdx = count
    movzx eax, word [rbx + 2]           ; inode
    mov edx, [rbx + 4]                  ; current file offset
    pop rcx                             ; rcx = count
    ; rsi = user buffer
    call ext4_write_file_chunk
    cmp rax, -1
    je .wfd_err_io

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
    jae .wfd_err_badfd
    imul eax, eax, PIPE_STRUCT_SIZE
    lea r8, [r15 + pipe_table - kmain + rax]

    ; POSIX: write blocks while the buffer is full and fails only when the
    ; readers are gone (was: a full pipe returned 0, which makes
    ; busybox-style full_write loops spin forever).
.wfd_p_wait:
    cmp word [r8 + 12], 0              ; any readers left?
    je .wfd_err_epipe
    mov ecx, PIPE_BUF_SIZE
    sub ecx, [r8 + 8]
    test ecx, ecx
    jg .wfd_p_room
    ; full: sleep until the reader drains (interruptible by signals)
    call sig_probe_actionable
    test eax, eax
    jnz .wfd_err_intr
    sti
    mov ecx, 2
    call task_sleep                     ; preserves rax/rbx; ctx switch the rest
    cli
    jmp .wfd_p_wait
.wfd_p_room:
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

.wfd_dev_null:
    mov rax, rdx                        ; successfully discarded all bytes
    jmp .wfd_done

.wfd_zero:
    xor eax, eax
    jmp .wfd_done

.wfd_err_epipe:
    mov dword [r15 + vfs_last_err - kmain], 32      ; EPIPE
    ; Linux: writing to a pipe with no readers raises SIGPIPE for the writer
    ; (default action terminates it) IN ADDITION to returning -EPIPE.
    mov edi, [r15 + sched_curr_task - kmain]
    mov esi, 13                         ; SIGPIPE
    xor edx, edx                        ; kernel-generated
    call sig_send_to_task
    jmp .wfd_err
.wfd_err_intr:
    mov dword [r15 + vfs_last_err - kmain], 4       ; EINTR
    jmp .wfd_err
.wfd_err_fault:
    mov dword [r15 + vfs_last_err - kmain], 14      ; EFAULT
    jmp .wfd_err
.wfd_err_badfd:
    mov dword [r15 + vfs_last_err - kmain], 9       ; EBADF
    jmp .wfd_err
.wfd_err_io:
    mov dword [r15 + vfs_last_err - kmain], 5       ; EIO
.wfd_err:
    mov rax, -1

.wfd_done:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    popfq
    ret

; ==============================================================================
; vfs_pipe: Create an inter-process pipe
; RDI = user pointer to int pipefd[2]
; Returns: EAX = 0 on success, -1 on error
; ==============================================================================
vfs_pipe:
    pushfq
    cli
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
    popfq
    ret

; ==============================================================================
; vfs_dup2: Duplicate oldfd to newfd
; EDI = oldfd, ESI = newfd
; Returns: EAX = newfd, or -1 on error
; ==============================================================================
vfs_dup2:
    pushfq
    cli
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
    popfq
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
    ; If it is a pipe, decrement pipe refcount
    call vfs_get_fd_ptr
    test rax, rax
    jz .clean_next
    cmp byte [rax + 0], FD_TYPE_PIPE
    jne @f
    movzx ecx, word [rax + 2]
    imul ecx, ecx, PIPE_STRUCT_SIZE
    lea rdx, [r15 + pipe_table - kmain + rcx]
    cmp byte [rax + 1], O_RDONLY
    jne .clean_pipe_wr
    cmp word [rdx + 12], 0
    je @f
    dec word [rdx + 12]
    jmp @f
.clean_pipe_wr:
    cmp word [rdx + 14], 0
    je @f
    dec word [rdx + 14]
@@:
    mov byte [rax + 0], FD_TYPE_UNUSED
.clean_next:
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
    push rdi                 ; save entry start: rep movsd advances rdi by 48,
    mov ecx, FD_ENTRY_SIZE / 4
    rep movsd                ; and the pipe check below must see the COPY,
    pop rdi                  ; not the following (unused) fd entry
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
