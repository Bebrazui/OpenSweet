; ==============================================================================
; Opensweet OS - procfs (kernel/procfs.inc)
;
; A read-only synthetic /proc.  Real Linux userland expects it: `free` reads
; /proc/meminfo, `df` reads /proc/mounts, `ps` walks /proc, glibc reads
; /proc/cpuinfo and /proc/stat.  Without it those tools simply fail.
;
; Content is generated on demand into proc_buf (one shared kernel buffer, and
; the read path runs with interrupts off), so nothing is stored per fd.
; ==============================================================================

PROC_BUF_SIZE    = 2048
PROC_DIR_ENTRIES = 10               ; '.', '..' and the eight files below

FD_TYPE_PROCFS   = 5
FD_TYPE_PROCDIR  = 6

PROC_ID_MEMINFO  = 1
PROC_ID_MOUNTS   = 2
PROC_ID_CPUINFO  = 3
PROC_ID_VERSION  = 4
PROC_ID_UPTIME   = 5
PROC_ID_STAT     = 6
PROC_ID_LOADAVG  = 7
PROC_ID_CMDLINE  = 8

; ------------------------------------------------------------------------------
; proc_putc: append AL to proc_buf.  Clobbers RCX, RDX.
; ------------------------------------------------------------------------------
proc_putc:
    mov ecx, [r15 + proc_len - kmain]
    cmp ecx, PROC_BUF_SIZE - 1
    jae .ppc_ret
    lea rdx, [r15 + proc_buf - kmain]
    mov [rdx + rcx], al
    inc ecx
    mov [r15 + proc_len - kmain], ecx
.ppc_ret:
    ret

; ------------------------------------------------------------------------------
; proc_puts: append the ASCIIZ string at RSI.  Preserves RSI.
; ------------------------------------------------------------------------------
proc_puts:
    push rsi
.pp_loop:
    lodsb
    test al, al
    jz .pp_done
    call proc_putc
    jmp .pp_loop
.pp_done:
    pop rsi
    ret

; ------------------------------------------------------------------------------
; proc_putdec: append RAX as an unsigned decimal number.
; ------------------------------------------------------------------------------
proc_putdec:
    push rbx
    push rcx
    push rdx
    sub rsp, 40
    lea rdi, [rsp + 32]
    mov byte [rdi], 0
    mov rbx, 10
    test rax, rax
    jnz .pd_loop
    dec rdi
    mov byte [rdi], '0'
    jmp .pd_out
.pd_loop:
    xor edx, edx
    div rbx
    add dl, '0'
    dec rdi
    mov [rdi], dl
    test rax, rax
    jnz .pd_loop
.pd_out:
    mov rsi, rdi
    call proc_puts
    add rsp, 40
    pop rdx
    pop rcx
    pop rbx
    ret

; ------------------------------------------------------------------------------
; proc_strcmp: compare ASCIIZ RSI and RDI.  CF = 0 when equal, CF = 1 when not.
; Preserves RSI and RDI.
; ------------------------------------------------------------------------------
proc_strcmp:
    push rsi
    push rdi
.ps_loop:
    mov al, [rsi]
    cmp al, [rdi]
    jne .ps_ne
    test al, al
    jz .ps_eq
    inc rsi
    inc rdi
    jmp .ps_loop
.ps_eq:
    pop rdi
    pop rsi
    clc
    ret
.ps_ne:
    pop rdi
    pop rsi
    stc
    ret

; ------------------------------------------------------------------------------
; proc_find: RSI = path.
;   EAX = 0  -> the path is not under /proc
;   EAX = -1 -> /proc itself (the directory)
;   EAX > 0  -> proc file id
; Preserves everything except RAX.
; ------------------------------------------------------------------------------
proc_find:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    cmp dword [rsi], '/pro'
    jne .pf_none
    cmp byte [rsi + 4], 'c'
    jne .pf_none
    mov al, [rsi + 5]
    test al, al
    jz .pf_dir
    cmp al, '/'
    jne .pf_none
    add rsi, 6
    cmp byte [rsi], 0
    je .pf_dir

    lea rbx, [r15 + proc_table - kmain]
.pf_loop:
    mov rdi, [rbx]
    test rdi, rdi
    jz .pf_none
    call proc_strcmp
    jnc .pf_found
    add rbx, 16
    jmp .pf_loop
.pf_found:
    mov eax, [rbx + 8]
    jmp .pf_pop
.pf_none:
    xor eax, eax
    jmp .pf_pop
.pf_dir:
    mov rax, -1
.pf_pop:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

; ------------------------------------------------------------------------------
; proc_gen: EDI = proc file id.  Fills proc_buf, returns EAX = length.
; Preserves RBX, R12-R15.
; ------------------------------------------------------------------------------
proc_gen:
    push rbx
    push r12
    mov dword [r15 + proc_len - kmain], 0

    cmp edi, PROC_ID_MEMINFO
    je .g_meminfo
    cmp edi, PROC_ID_MOUNTS
    je .g_mounts
    cmp edi, PROC_ID_CPUINFO
    je .g_cpuinfo
    cmp edi, PROC_ID_VERSION
    je .g_version
    cmp edi, PROC_ID_UPTIME
    je .g_uptime
    cmp edi, PROC_ID_STAT
    je .g_stat
    cmp edi, PROC_ID_LOADAVG
    je .g_loadavg
    cmp edi, PROC_ID_CMDLINE
    je .g_cmdline
    jmp .g_done

.g_meminfo:
    call count_free                     ; R8D = free 4KB pages
    mov r12d, r8d
    shr r12d, 8                         ; -> megabytes
    lea rsi, [r15 + pstr_mem_total - kmain]
    call proc_puts
    mov eax, 262144                     ; the PMM manages 256 MB
    call proc_putdec
    lea rsi, [r15 + pstr_kb - kmain]
    call proc_puts
    lea rsi, [r15 + pstr_mem_free - kmain]
    call proc_puts
    mov eax, r12d
    shl eax, 10
    call proc_putdec
    lea rsi, [r15 + pstr_kb - kmain]
    call proc_puts
    lea rsi, [r15 + pstr_mem_avail - kmain]
    call proc_puts
    mov eax, r12d
    shl eax, 10
    call proc_putdec
    lea rsi, [r15 + pstr_kb - kmain]
    call proc_puts
    lea rsi, [r15 + pstr_mem_rest - kmain]
    call proc_puts
    jmp .g_done

.g_mounts:
    lea rsi, [r15 + pstr_mounts - kmain]
    call proc_puts
    jmp .g_done

.g_cpuinfo:
    lea rsi, [r15 + pstr_cpuinfo - kmain]
    call proc_puts
    jmp .g_done

.g_version:
    lea rsi, [r15 + pstr_version - kmain]
    call proc_puts
    jmp .g_done

.g_uptime:
    mov eax, [r15 + timer_ticks - kmain]
    xor edx, edx
    mov ecx, 1000
    div ecx                             ; EAX = seconds, EDX = ms
    mov r12d, edx
    call proc_putdec
    mov al, '.'
    call proc_putc
    mov eax, r12d
    mov ecx, 10
    xor edx, edx
    div ecx                             ; EAX = hundredths (0..99)
    cmp eax, 10
    jae .g_up_nopad
    push rax
    mov al, '0'
    call proc_putc
    pop rax
.g_up_nopad:
    call proc_putdec
    mov al, ' '
    call proc_putc
    mov eax, [r15 + timer_ticks - kmain]
    xor edx, edx
    mov ecx, 1000
    div ecx
    call proc_putdec
    lea rsi, [r15 + pstr_dot00 - kmain]
    call proc_puts
    jmp .g_done

.g_stat:
    lea rsi, [r15 + pstr_stat_cpu - kmain]
    call proc_puts
    call rtc_get_epoch
    mov r12d, eax
    lea rsi, [r15 + pstr_stat_btime - kmain]
    call proc_puts
    mov eax, r12d
    call proc_putdec
    mov al, 10
    call proc_putc
    lea rsi, [r15 + pstr_stat_tail - kmain]
    call proc_puts
    jmp .g_done

.g_loadavg:
    lea rsi, [r15 + pstr_loadavg_a - kmain]
    call proc_puts
    mov eax, [r15 + sched_task_cnt - kmain]
    call proc_putdec
    mov al, ' '
    call proc_putc
    mov eax, [r15 + sched_curr_task - kmain]
    call proc_putdec
    mov al, 10
    call proc_putc
    jmp .g_done

.g_cmdline:
    lea rsi, [r15 + pstr_cmdline - kmain]
    call proc_puts

.g_done:
    mov eax, [r15 + proc_len - kmain]
    pop r12
    pop rbx
    ret

; ------------------------------------------------------------------------------
; proc_stat_fill: RDI = stat buffer (144 bytes), EAX = proc id (0 = /proc).
; ------------------------------------------------------------------------------
proc_stat_fill:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    mov ebx, eax
    mov rdi, [rsp]                      ; rdi was pushed last -> the buffer
    mov ecx, 144 / 8
    xor eax, eax
    rep stosq
    mov rdi, [rsp]
    mov qword [rdi + 0], 0x0901         ; st_dev = makedev(9, 1) = procfs
    test ebx, ebx
    jnz .psf_file
    mov qword [rdi + 8], 1              ; st_ino
    mov qword [rdi + 16], 2             ; st_nlink
    mov qword [rdi + 24], 0x41ED        ; S_IFDIR | 0755
    mov qword [rdi + 56], 1024          ; st_blksize
    jmp .psf_done
.psf_file:
    mov [rdi + 8], rbx                  ; st_ino = id
    mov qword [rdi + 16], 1
    mov qword [rdi + 24], 0x81A4        ; S_IFREG | 0644
    mov qword [rdi + 56], 1024
    push rdi
    mov edi, ebx
    call proc_gen
    pop rdi
    mov [rdi + 48], rax                 ; st_size
.psf_done:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

; ------------------------------------------------------------------------------
; proc_dir_read: emit dirent64 records for /proc.
;   RDI = user buffer, RSI = byte count, RDX = start index.
;   Returns RAX = bytes written, RDX = index of the next entry.
; ------------------------------------------------------------------------------
proc_dir_read:
    push rbx
    push rcx
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13

    mov r12, rdi                        ; buffer
    mov r13, rsi                        ; capacity
    mov rbx, rdx                        ; entry index
    xor r8d, r8d                        ; bytes written
    lea r10, [r15 + proc_dir_table - kmain]

.pdr_loop:
    cmp rbx, PROC_DIR_ENTRIES
    jae .pdr_done
    mov r11, rbx
    shl r11, 4
    mov rsi, [r10 + r11]                ; name
    movzx edx, byte [r10 + r11 + 8]     ; d_type

    xor ecx, ecx
.pdr_slen:
    cmp byte [rsi + rcx], 0
    je .pdr_slen_done
    inc ecx
    jmp .pdr_slen
.pdr_slen_done:
    lea eax, [rcx + 20]                 ; header 19 + name + NUL
    add eax, 7
    and eax, -8                         ; d_reclen
    mov r11d, r8d
    add r11d, eax
    cmp r11, r13
    ja .pdr_done

    lea rdi, [r12 + r8]
    mov r11, rbx
    inc r11
    mov [rdi + 0], r11                  ; d_ino
    mov [rdi + 8], r11                  ; d_off = next index
    mov [rdi + 16], ax                  ; d_reclen
    mov [rdi + 18], dl                  ; d_type
    add rdi, 19
    push rax
    push rcx
    rep movsb
    pop rcx
    pop rax

    ; zero the padding after the name (including its NUL)
    mov edx, eax
    sub edx, 19
    sub edx, ecx
    lea rdi, [r12 + r8]
    add rdi, 19
    add rdi, rcx
    mov ecx, edx
    push rax
    xor eax, eax
    rep stosb
    pop rax

    add r8d, eax
    inc rbx
    jmp .pdr_loop

.pdr_done:
    mov rax, r8
    mov rdx, rbx
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rcx
    pop rbx
    ret

; ------------------------------------------------------------------------------
align 8
proc_len  dd 0
proc_buf  rb PROC_BUF_SIZE

; path -> id (16 bytes per entry: name pointer, id; a null pointer ends it)
align 8
proc_table:
    dq pn_meminfo, PROC_ID_MEMINFO
    dq pn_mounts, PROC_ID_MOUNTS
    dq pn_cpuinfo, PROC_ID_CPUINFO
    dq pn_version, PROC_ID_VERSION
    dq pn_uptime, PROC_ID_UPTIME
    dq pn_stat, PROC_ID_STAT
    dq pn_loadavg, PROC_ID_LOADAVG
    dq pn_cmdline, PROC_ID_CMDLINE
    dq 0, 0

; directory listing (16 bytes per entry: name pointer, dirent type)
align 8
proc_dir_table:
    dq pn_dot, 4
    dq pn_dotdot, 4
    dq pn_meminfo, 8
    dq pn_mounts, 8
    dq pn_cpuinfo, 8
    dq pn_version, 8
    dq pn_uptime, 8
    dq pn_stat, 8
    dq pn_loadavg, 8
    dq pn_cmdline, 8

pn_dot      db ".", 0
pn_dotdot   db "..", 0
pn_meminfo  db "meminfo", 0
pn_mounts   db "mounts", 0
pn_cpuinfo  db "cpuinfo", 0
pn_version  db "version", 0
pn_uptime   db "uptime", 0
pn_stat     db "stat", 0
pn_loadavg  db "loadavg", 0
pn_cmdline  db "cmdline", 0

pstr_kb        db " kB", 10, 0
pstr_mem_total db "MemTotal:         ", 0
pstr_mem_free  db "MemFree:          ", 0
pstr_mem_avail db "MemAvailable:     ", 0
pstr_mem_rest  db "Buffers:               0 kB", 10, "Cached:                0 kB", 10, "SwapCached:            0 kB", 10, "Active:                0 kB", 10, "Inactive:              0 kB", 10, "Active(anon):          0 kB", 10, "Inactive(anon):        0 kB", 10, "Active(file):          0 kB", 10, "Inactive(file):        0 kB", 10, "Unevictable:           0 kB", 10, "Mlocked:               0 kB", 10, "SwapTotal:             0 kB", 10, "SwapFree:              0 kB", 10, "Dirty:                 0 kB", 10, "Writeback:             0 kB", 10, "AnonPages:             0 kB", 10, "Mapped:                0 kB", 10, "Shmem:                 0 kB", 10, "Slab:                  0 kB", 10, "SReclaimable:          0 kB", 10, "SUnreclaim:            0 kB", 10, "KernelStack:           0 kB", 10, "PageTables:            0 kB", 10, "NFS_Unstable:          0 kB", 10, "Bounce:                0 kB", 10, "WritebackTmp:          0 kB", 10, "CommitLimit:           0 kB", 10, "Committed_AS:          0 kB", 10, "VmallocTotal:          0 kB", 10, "VmallocUsed:           0 kB", 10, "VmallocChunk:          0 kB", 10, 0

pstr_mounts    db "ext4 / ext4 rw,relatime 0 0", 10, "proc /proc proc rw,nosuid,nodev,noexec 0 0", 10, 0

pstr_cpuinfo   db "processor", 9, ": 0", 10, "vendor_id", 9, ": OpenSweet", 10, "cpu family", 9, ": 6", 10, "model", 9, 9, ": 1", 10, "model name", 9, ": OpenSweet Virtual CPU", 10, "stepping", 9, ": 1", 10, "cpu MHz", 9, 9, ": 3000.000", 10, "cache size", 9, ": 4096 KB", 10, "flags", 9, 9, ": fpu de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 syscall nx lm", 10, 0

pstr_version   db "Linux version 6.1.0-opensweet (root@opensweet) #1 SMP PREEMPT", 10, 0

pstr_dot00     db ".00", 10, 0

pstr_stat_cpu  db "cpu  1 0 1 0 0 0 0 0 0 0", 10, "cpu0 1 0 1 0 0 0 0 0 0 0", 10, 0
pstr_stat_btime db "btime ", 0
pstr_stat_tail db "processes 1", 10, "procs_running 1", 10, "procs_blocked 0", 10, 0

pstr_loadavg_a db "0.00 0.00 0.00 1/", 0
pstr_cmdline   db "root=/dev/sdb rw", 10, 0
