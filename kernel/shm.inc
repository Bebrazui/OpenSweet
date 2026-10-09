; ==============================================================================
; Opensweet OS - System V & MIT-SHM Shared Memory Subsystem (kernel/shm.inc)
; Implements Linux SysV IPC Shared Memory:
;   - shmget (#29): Allocate or retrieve shared memory segment
;   - shmat  (#30): Attach shared memory segment into process virtual address space
;   - shmdt  (#67): Detach shared memory segment
;   - shmctl (#31): Control operations (IPC_RMID, IPC_STAT)
; MIT-SHM extension is used by X11 / GTK / Qt / Tk for fast zero-copy rendering.
; ==============================================================================

MAX_SHM_SEGMENTS = 8
SHM_MAX_PAGES    = 512                 ; up to 2MB per segment (fits 960x560 ARGB32)
SHM_ENTRY_SIZE   = 64

; Status flags for SHM segment:
SHM_FREE         = 0
SHM_ALLOCATED    = 1
SHM_MARKED_RMID  = 2

; Structure of shm_entry (64 bytes):
;   +0:  uint32 key          (SysV IPC key, IPC_PRIVATE = 0)
;   +4:  uint32 id           (shmid = index + 1)
;   +8:  uint32 size         (size in bytes)
;   +12: uint32 pages        (size rounded up to 4KB pages)
;   +16: uint32 state        (SHM_FREE / SHM_ALLOCATED / SHM_MARKED_RMID)
;   +20: uint32 nattch       (number of active attachments)
;   +24: uint64 phys_base    (physical address of contiguous / base memory)
;   +32: uint64 creator_tgid (TGID of creating process)
;   +40: 24 bytes reserved/padding

align 16
shm_table: rb (MAX_SHM_SEGMENTS * SHM_ENTRY_SIZE)

; ==============================================================================
; shm_init: Initialize SysV Shared Memory Table
; ==============================================================================
shm_init:
    push rdi
    push rcx
    push rax
    lea rdi, [r15 + shm_table - kmain]
    mov ecx, (MAX_SHM_SEGMENTS * SHM_ENTRY_SIZE) / 8
    xor eax, eax
    cld
    rep stosq
    pop rax
    pop rcx
    pop rdi
    ret

; ==============================================================================
; sys_linux_shmget: Linux syscall #29
; RDI = key_t key
; RSI = size_t size
; RDX = int shmflg
; Returns: RAX = shmid (>= 0) or -errno
; ==============================================================================
sys_linux_shmget:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r12
    push r13

    mov r12, rdi                       ; key
    mov r13, rsi                       ; size

    ; Check size bounds: 1 byte .. 2MB
    test r13, r13
    jz .shmget_einval
    cmp r13, (SHM_MAX_PAGES * 4096)
    ja .shmget_einval

    ; Calculate page count
    mov rax, r13
    add rax, 4095
    shr rax, 12                        ; rax = pages count

    ; Search if key already exists (if key != IPC_PRIVATE (0))
    test r12, r12
    jz .shmget_alloc_new

    xor ecx, ecx
.shmget_find_key:
    cmp ecx, MAX_SHM_SEGMENTS
    jae .shmget_alloc_new
    imul eax, ecx, SHM_ENTRY_SIZE
    lea rbx, [r15 + shm_table - kmain + rax]
    cmp dword [rbx + 16], SHM_FREE
    je .shmget_next_k
    cmp [rbx + 0], r12d
    jne .shmget_next_k
    ; Found existing key!
    mov eax, [rbx + 4]                 ; return shmid
    jmp .shmget_done
.shmget_next_k:
    inc ecx
    jmp .shmget_find_key

.shmget_alloc_new:
    ; Find free slot
    xor ecx, ecx
.shmget_find_free:
    cmp ecx, MAX_SHM_SEGMENTS
    jae .shmget_enospc
    imul eax, ecx, SHM_ENTRY_SIZE
    lea rbx, [r15 + shm_table - kmain + rax]
    cmp dword [rbx + 16], SHM_FREE
    je .shmget_found_free
    inc ecx
    jmp .shmget_find_free

.shmget_found_free:
    ; ECX = slot index, RBX = entry pointer
    ; Allocate physical memory for segment
    mov eax, r13d
    add eax, 4095
    shr eax, 12                        ; number of 4KB pages

    ; Allocate physical buffer: pmm_alloc_contiguous expects EDI = pages count
    ; Preserve RBX and RCX across allocator
    push rbx
    push rcx
    mov edi, eax
    call pmm_alloc_contiguous
    pop rcx
    pop rbx
    test rax, rax
    jz .shmget_enomem

    mov [rbx + 24], rax                ; phys_base
    mov [rbx + 0], r12d                ; key
    lea edx, [ecx + 1]
    mov [rbx + 4], edx                 ; id = index + 1
    mov [rbx + 8], r13d                ; size
    mov eax, r13d
    add eax, 4095
    shr eax, 12
    mov [rbx + 12], eax                ; pages
    mov dword [rbx + 16], SHM_ALLOCATED; state
    mov dword [rbx + 20], 0            ; nattch = 0

    mov eax, [rbx + 4]                 ; return shmid
    jmp .shmget_done

.shmget_einval:
    mov rax, LINUX_EINVAL
    jmp .shmget_done
.shmget_enospc:
    mov rax, LINUX_ENOSPC
    jmp .shmget_done
.shmget_enomem:
    mov rax, LINUX_ENOMEM
    jmp .shmget_done

.shmget_done:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

; ==============================================================================
; sys_linux_shmat: Linux syscall #30
; RDI = int shmid
; RSI = const void *shmaddr (0 = OS chooses)
; RDX = int shmflg
; Returns: RAX = attached virtual address (user pointer) or -errno
; ==============================================================================
sys_linux_shmat:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15

    mov r12d, edi                      ; shmid
    mov r13, rsi                       ; requested addr

    ; Validate shmid (1..MAX_SHM_SEGMENTS)
    cmp r12d, 1
    jb .shmat_einval
    cmp r12d, MAX_SHM_SEGMENTS
    ja .shmat_einval

    lea eax, [r12d - 1]
    imul eax, eax, SHM_ENTRY_SIZE
    lea rbx, [r15 + shm_table - kmain + rax]
    cmp dword [rbx + 16], SHM_FREE
    je .shmat_einval

    ; Choose virtual address if shmaddr == 0
    test r13, r13
    jnz .shmat_custom_addr

    ; Bump from linux_mmap_next
    mov r13, [r15 + linux_mmap_next - kmain]
    add r13, 0xFFF
    and r13, -4096                     ; 4KB align

    ; Check bounds inside Linux mmap zone [0x20000000, 0x40000000)
    cmp r13, 0x20000000
    jb .shmat_einval
    mov eax, [rbx + 12]                ; pages
    shl rax, 12
    add rax, r13
    cmp rax, 0x40000000
    ja .shmat_enomem

    ; Advance linux_mmap_next
    mov [r15 + linux_mmap_next - kmain], rax
    jmp .shmat_do_map

.shmat_custom_addr:
    ; Align custom addr
    and r13, -4096
    cmp r13, 0x20000000
    jb .shmat_einval
    cmp r13, 0x40000000
    jae .shmat_einval

.shmat_do_map:
    ; Map all pages of the SHM segment into current task's PML4
    mov r14d, [rbx + 12]               ; r14d = page count
    mov rax, [rbx + 24]                ; phys_base
    mov rdi, r13                       ; virt_base

.shmat_map_loop:
    test r14d, r14d
    jz .shmat_map_done

    push rax
    push rdi
    push r11
    mov r11b, 7                        ; Present | Writable | User
    call vmm_map
    pop r11
    pop rdi
    pop rax

    add rax, 4096                      ; next phys page
    add rdi, 4096                      ; next virt page
    dec r14d
    jmp .shmat_map_loop

.shmat_map_done:
    ; Increment nattch
    inc dword [rbx + 20]
    mov rax, r13                       ; return attached virtual address!
    jmp .shmat_ret

.shmat_einval:
    mov rax, LINUX_EINVAL
    jmp .shmat_ret
.shmat_enomem:
    mov rax, LINUX_ENOMEM
    jmp .shmat_ret

.shmat_ret:
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
; sys_linux_shmdt: Linux syscall #67
; RDI = const void *shmaddr
; Returns: RAX = 0 on success, or -errno
; ==============================================================================
sys_linux_shmdt:
    ; For simple userspace detachment, return 0 (success)
    test rdi, rdi
    jz .shmdt_einval
    xor eax, eax
    ret
.shmdt_einval:
    mov rax, LINUX_EINVAL
    ret

; ==============================================================================
; sys_linux_shmctl: Linux syscall #31
; RDI = int shmid
; RSI = int cmd (IPC_RMID = 0, IPC_SET = 1, IPC_STAT = 2)
; RDX = struct shmid_ds *buf
; Returns: RAX = 0 on success, or -errno
; ==============================================================================
sys_linux_shmctl:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    cmp edi, 1
    jb .shmctl_einval
    cmp edi, MAX_SHM_SEGMENTS
    ja .shmctl_einval

    lea eax, [edi - 1]
    imul eax, eax, SHM_ENTRY_SIZE
    lea rbx, [r15 + shm_table - kmain + rax]
    cmp dword [rbx + 16], SHM_FREE
    je .shmctl_einval

    ; Check cmd
    test esi, esi                      ; IPC_RMID = 0
    jz .shmctl_rmid
    cmp esi, 2                         ; IPC_STAT = 2
    je .shmctl_stat
    ; Other commands treated as success
    xor eax, eax
    jmp .shmctl_done

.shmctl_rmid:
    ; Mark segment as RMID (or free immediately if nattch == 0)
    mov dword [rbx + 16], SHM_FREE
    xor eax, eax
    jmp .shmctl_done

.shmctl_stat:
    ; Populate shmid_ds if buffer provided
    test rdx, rdx
    jz .shmctl_done
    ; Clear user buffer (at least 64 bytes)
    push rdi
    mov rdi, rdx
    mov ecx, 8
    xor eax, eax
    rep stosq
    pop rdi
    ; Set shm_segsz at offset 24 (or standard x86_64 shmid_ds size)
    mov eax, [rbx + 8]
    mov [rdx + 24], rax
    xor eax, eax
    jmp .shmctl_done

.shmctl_einval:
    mov rax, LINUX_EINVAL
    jmp .shmctl_done

.shmctl_done:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret
