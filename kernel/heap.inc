; ==============================================================================
; Opensweet OS - Dynamic Kernel Memory Allocator (kernel/heap.inc)
; High-performance Boundary-Tag Arena Heap with Splitting & Coalescing.
; 16-byte aligned allocations, double-free guard, interrupt-safe re-entrancy.
; ==============================================================================

HEAP_MAGIC          = 0x4B484541      ; 'KHEA'
ARENA_MAGIC         = 0x4152454E      ; 'AREN'

HEAP_FLAG_USED      = 0
HEAP_FLAG_FREE      = 1

BLOCK_HDR_SIZE      = 32              ; 32 bytes (16-byte aligned)
ARENA_HDR_SIZE      = 32              ; 32 bytes (16-byte aligned)
ARENA_DEFAULT_PAGES = 16              ; 16 * 4096 = 64 KB per arena
BLOCK_MIN_SIZE      = 16              ; minimum payload size
BLOCK_MIN_SPLIT     = BLOCK_HDR_SIZE + BLOCK_MIN_SIZE ; 48 bytes

; Arena Header Offsets (32 bytes)
ARENA_OFF_MAGIC     = 0               ; uint32: ARENA_MAGIC
ARENA_OFF_PAGES     = 4               ; uint32: total pages in arena
ARENA_OFF_SIZE      = 8               ; uint64: total arena byte size
ARENA_OFF_NEXT      = 16              ; uint64: ptr to next ArenaHeader (or 0)
ARENA_OFF_RES       = 24              ; uint64: reserved/padding

; Block Header Offsets (32 bytes)
BLOCK_OFF_MAGIC     = 0               ; uint32: HEAP_MAGIC
BLOCK_OFF_FLAGS     = 4               ; uint32: bit 0: 1=free, 0=used
BLOCK_OFF_SIZE      = 8               ; uint64: payload size in bytes
BLOCK_OFF_PREV      = 16              ; uint64: ptr to prev BlockHeader (or 0)
BLOCK_OFF_NEXT      = 24              ; uint64: ptr to next BlockHeader (or 0)

align 16
; Heap State
kheap_arena_list    dq 0              ; Head of arena linked list
kheap_arena_count   dd 0              ; Total arenas allocated
kheap_total_bytes   dq 0              ; Total usable heap capacity
kheap_alloc_bytes   dq 0              ; Currently allocated payload bytes
kheap_free_bytes    dq 0              ; Currently free payload bytes
kheap_alloc_count   dd 0              ; Number of active allocations
kheap_free_count    dd 0              ; Number of free blocks

; Strings for heap diagnostics
str_heap_hdr        db "=== Opensweet OS Kernel Heap Manager ===", 10, 0
str_heap_sep        db "----------------------------------------", 10, 0
str_heap_arenas     db "  Active Arenas:     ", 0
str_heap_total      db "  Total Capacity:    ", 0
str_heap_alloc      db "  Allocated Memory:  ", 0
str_heap_free       db "  Free Memory:       ", 0
str_heap_blocks     db "  Allocated Blocks:  ", 0
str_heap_freeblks   db "  Free Blocks:       ", 0
str_heap_bytes      db " bytes", 10, 0
str_heap_in         db " bytes in ", 0
str_heap_blks_s     db " blocks", 10, 0
str_heap_init_ok    db "[kheap] Dynamic Kernel Heap initialized (64 KB initial arena).", 10, 0
str_heap_oom_msg    db "[kheap] ERROR: Out of physical memory for heap expansion!", 10, 0

; ==============================================================================
; kheap_init: Initialize Kernel Heap with first 64 KB Arena from PMM
; ==============================================================================
kheap_init:
    push rax
    push rbx
    push rcx
    push rdi

    ; Allocate initial 16 contiguous pages (64 KB)
    mov edi, ARENA_DEFAULT_PAGES
    call pmm_alloc_contiguous
    test rax, rax
    jz .init_fail

    ; RDI = Arena base pointer
    mov rdi, rax

    ; Setup Arena Header at offset 0
    mov dword [rdi + ARENA_OFF_MAGIC], ARENA_MAGIC
    mov dword [rdi + ARENA_OFF_PAGES], ARENA_DEFAULT_PAGES
    mov qword [rdi + ARENA_OFF_SIZE], (ARENA_DEFAULT_PAGES * 4096)
    mov qword [rdi + ARENA_OFF_NEXT], 0
    mov qword [rdi + ARENA_OFF_RES], 0

    ; Link arena
    mov [r15 + kheap_arena_list - kmain], rdi
    mov dword [r15 + kheap_arena_count - kmain], 1

    ; First Block Header at rdi + ARENA_HDR_SIZE (offset 32)
    lea rbx, [rdi + ARENA_HDR_SIZE]
    mov dword [rbx + BLOCK_OFF_MAGIC], HEAP_MAGIC
    mov dword [rbx + BLOCK_OFF_FLAGS], HEAP_FLAG_FREE
    ; Usable payload = arena size - arena header - block header
    mov rax, (ARENA_DEFAULT_PAGES * 4096) - ARENA_HDR_SIZE - BLOCK_HDR_SIZE
    mov [rbx + BLOCK_OFF_SIZE], rax
    mov qword [rbx + BLOCK_OFF_PREV], 0
    mov qword [rbx + BLOCK_OFF_NEXT], 0

    ; Initialize global statistics
    mov [r15 + kheap_total_bytes - kmain], rax
    mov [r15 + kheap_free_bytes - kmain], rax
    mov qword [r15 + kheap_alloc_bytes - kmain], 0
    mov dword [r15 + kheap_alloc_count - kmain], 0
    mov dword [r15 + kheap_free_count - kmain], 1

    ; Print boot log to COM1 / console
    lea rsi, [r15 + str_heap_init_ok - kmain]
    call puts

.init_fail:
    pop rdi
    pop rcx
    pop rbx
    pop rax
    ret

; ==============================================================================
; kheap_expand: Append a new Arena to the heap
; RDI = minimum payload size needed
; Returns: RAX = pointer to first free BlockHeader in new arena (or 0 on OOM)
; ==============================================================================
kheap_expand:
    push rbx
    push rcx
    push rdx
    push rdi
    push rsi
    push r8

    ; Calculate pages needed: (needed + ARENA_HDR_SIZE + BLOCK_HDR_SIZE + 4095) / 4096
    lea rax, [rdi + ARENA_HDR_SIZE + BLOCK_HDR_SIZE + 4095]
    shr rax, 12                       ; rax = pages needed
    cmp rax, ARENA_DEFAULT_PAGES
    jae @f
    mov rax, ARENA_DEFAULT_PAGES      ; default to at least 16 pages (64 KB)
@@:
    mov r8d, eax                      ; r8d = pages count

    mov edi, r8d
    call pmm_alloc_contiguous
    test rax, rax
    jz .expand_oom

    ; RDI = new Arena base
    mov rdi, rax

    ; Setup Arena Header
    mov dword [rdi + ARENA_OFF_MAGIC], ARENA_MAGIC
    mov dword [rdi + ARENA_OFF_PAGES], r8d
    mov eax, r8d
    shl rax, 12                       ; rax = total bytes in new arena
    mov [rdi + ARENA_OFF_SIZE], rax
    mov qword [rdi + ARENA_OFF_NEXT], 0
    mov qword [rdi + ARENA_OFF_RES], 0

    ; Link new arena to the end of arena list
    mov rsi, [r15 + kheap_arena_list - kmain]
    test rsi, rsi
    jnz .find_tail
    mov [r15 + kheap_arena_list - kmain], rdi
    jmp .linked
.find_tail:
    mov rdx, [rsi + ARENA_OFF_NEXT]
    test rdx, rdx
    jz .tail_found
    mov rsi, rdx
    jmp .find_tail
.tail_found:
    mov [rsi + ARENA_OFF_NEXT], rdi

.linked:
    inc dword [r15 + kheap_arena_count - kmain]

    ; Setup first BlockHeader in this new arena
    lea rbx, [rdi + ARENA_HDR_SIZE]
    mov dword [rbx + BLOCK_OFF_MAGIC], HEAP_MAGIC
    mov dword [rbx + BLOCK_OFF_FLAGS], HEAP_FLAG_FREE
    mov rax, [rdi + ARENA_OFF_SIZE]
    sub rax, (ARENA_HDR_SIZE + BLOCK_HDR_SIZE)
    mov [rbx + BLOCK_OFF_SIZE], rax
    mov qword [rbx + BLOCK_OFF_PREV], 0
    mov qword [rbx + BLOCK_OFF_NEXT], 0

    ; Update global stats
    add [r15 + kheap_total_bytes - kmain], rax
    add [r15 + kheap_free_bytes - kmain], rax
    inc dword [r15 + kheap_free_count - kmain]

    mov rax, rbx                      ; return pointer to BlockHeader
    jmp .expand_done

.expand_oom:
    lea rsi, [r15 + str_heap_oom_msg - kmain]
    call puts
    xor eax, eax

.expand_done:
    pop r8
    pop rsi
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    ret

; ==============================================================================
; kmalloc: Allocate dynamic memory block from Kernel Heap
; RCX = size in bytes
; Returns: RAX = pointer to 16-byte aligned payload (or 0 on OOM)
; ==============================================================================
kmalloc:
    test rcx, rcx
    jnz @f
    xor eax, eax
    ret
@@:
    push rbx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10

    ; Align requested size up to multiple of 16 (minimum 16 bytes)
    add rcx, 15
    and rcx, -16
    cmp rcx, BLOCK_MIN_SIZE
    jae @f
    mov rcx, BLOCK_MIN_SIZE
@@:
    mov r8, rcx                       ; r8 = aligned payload size needed

    ; Re-entrancy & interrupt guard
    pushfq
    cli

    ; Walk all arenas
    mov rsi, [r15 + kheap_arena_list - kmain]
.arena_loop:
    test rsi, rsi
    jz .need_expand

    ; First block in arena
    lea rbx, [rsi + ARENA_HDR_SIZE]
.block_loop:
    test rbx, rbx
    jz .next_arena

    ; Sanity check block magic
    cmp dword [rbx + BLOCK_OFF_MAGIC], HEAP_MAGIC
    jne .next_arena                   ; block corrupted, skip arena

    ; Check if block is free
    cmp dword [rbx + BLOCK_OFF_FLAGS], HEAP_FLAG_FREE
    jne .next_block

    ; Check if block is large enough
    mov rax, [rbx + BLOCK_OFF_SIZE]
    cmp rax, r8
    jae .block_found

.next_block:
    mov rbx, [rbx + BLOCK_OFF_NEXT]
    jmp .block_loop

.next_arena:
    mov rsi, [rsi + ARENA_OFF_NEXT]
    jmp .arena_loop

.need_expand:
    ; No suitable free block found -> expand heap
    mov rdi, r8
    call kheap_expand
    test rax, rax
    jz .kmalloc_oom
    mov rbx, rax                      ; rbx = new free block

.block_found:
    ; RBX points to suitable free BlockHeader
    ; Check if we should split the block
    mov rax, [rbx + BLOCK_OFF_SIZE]
    sub rax, r8                       ; rax = excess bytes
    cmp rax, BLOCK_MIN_SPLIT
    jb .no_split

    ; Split: create new free block after this block
    ; new_block_addr = rbx + BLOCK_HDR_SIZE + r8
    lea rdi, [rbx + BLOCK_HDR_SIZE + r8]
    mov dword [rdi + BLOCK_OFF_MAGIC], HEAP_MAGIC
    mov dword [rdi + BLOCK_OFF_FLAGS], HEAP_FLAG_FREE
    sub rax, BLOCK_HDR_SIZE           ; usable size of split block
    mov [rdi + BLOCK_OFF_SIZE], rax
    mov [rdi + BLOCK_OFF_PREV], rbx
    mov rdx, [rbx + BLOCK_OFF_NEXT]
    mov [rdi + BLOCK_OFF_NEXT], rdx
    test rdx, rdx
    jz @f
    mov [rdx + BLOCK_OFF_PREV], rdi
@@:
    mov [rbx + BLOCK_OFF_NEXT], rdi
    mov [rbx + BLOCK_OFF_SIZE], r8    ; truncate original block to r8
    inc dword [r15 + kheap_free_count - kmain]

.no_split:
    ; Mark block USED
    mov dword [rbx + BLOCK_OFF_FLAGS], HEAP_FLAG_USED

    ; Update global stats
    mov rax, [rbx + BLOCK_OFF_SIZE]
    add [r15 + kheap_alloc_bytes - kmain], rax
    sub [r15 + kheap_free_bytes - kmain], rax
    inc dword [r15 + kheap_alloc_count - kmain]
    dec dword [r15 + kheap_free_count - kmain]

    popfq

    ; Payload starts immediately after BlockHeader
    lea rax, [rbx + BLOCK_HDR_SIZE]
    jmp .kmalloc_ret

.kmalloc_oom:
    popfq
    xor eax, eax

.kmalloc_ret:
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rbx
    ret

; ==============================================================================
; kfree: Release dynamic memory block back to Kernel Heap
; RDI = pointer to payload previously returned by kmalloc/kzalloc
; ==============================================================================
kfree:
    test rdi, rdi
    jnz @f
    ret
@@:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    ; Locate BlockHeader: payload - BLOCK_HDR_SIZE
    lea rbx, [rdi - BLOCK_HDR_SIZE]

    ; Sanity check: must match HEAP_MAGIC
    cmp dword [rbx + BLOCK_OFF_MAGIC], HEAP_MAGIC
    jne .kfree_bad

    ; Guard against double free
    cmp dword [rbx + BLOCK_OFF_FLAGS], HEAP_FLAG_FREE
    je .kfree_bad

    pushfq
    cli

    ; Mark block FREE
    mov dword [rbx + BLOCK_OFF_FLAGS], HEAP_FLAG_FREE

    ; Update global stats
    mov rax, [rbx + BLOCK_OFF_SIZE]
    sub [r15 + kheap_alloc_bytes - kmain], rax
    add [r15 + kheap_free_bytes - kmain], rax
    dec dword [r15 + kheap_alloc_count - kmain]
    inc dword [r15 + kheap_free_count - kmain]

    ; 1. Coalesce Right: merge with next block if it is free
    mov rdx, [rbx + BLOCK_OFF_NEXT]
    test rdx, rdx
    jz .coalesce_left

    cmp dword [rdx + BLOCK_OFF_MAGIC], HEAP_MAGIC
    jne .coalesce_left
    cmp dword [rdx + BLOCK_OFF_FLAGS], HEAP_FLAG_FREE
    jne .coalesce_left

    ; Merge RDX into RBX
    mov rax, [rdx + BLOCK_OFF_SIZE]
    add rax, BLOCK_HDR_SIZE
    add [rbx + BLOCK_OFF_SIZE], rax

    ; rbx.next = rdx.next
    mov rsi, [rdx + BLOCK_OFF_NEXT]
    mov [rbx + BLOCK_OFF_NEXT], rsi
    test rsi, rsi
    jz @f
    mov [rsi + BLOCK_OFF_PREV], rbx
@@:
    ; Invalidate merged header
    mov dword [rdx + BLOCK_OFF_MAGIC], 0
    dec dword [r15 + kheap_free_count - kmain]

.coalesce_left:
    ; 2. Coalesce Left: merge with prev block if it is free
    mov rdx, [rbx + BLOCK_OFF_PREV]
    test rdx, rdx
    jz .kfree_done

    cmp dword [rdx + BLOCK_OFF_MAGIC], HEAP_MAGIC
    jne .kfree_done
    cmp dword [rdx + BLOCK_OFF_FLAGS], HEAP_FLAG_FREE
    jne .kfree_done

    ; Merge RBX into RDX
    mov rax, [rbx + BLOCK_OFF_SIZE]
    add rax, BLOCK_HDR_SIZE
    add [rdx + BLOCK_OFF_SIZE], rax

    ; rdx.next = rbx.next
    mov rsi, [rbx + BLOCK_OFF_NEXT]
    mov [rdx + BLOCK_OFF_NEXT], rsi
    test rsi, rsi
    jz @f
    mov [rsi + BLOCK_OFF_PREV], rdx
@@:
    ; Invalidate merged header
    mov dword [rbx + BLOCK_OFF_MAGIC], 0
    dec dword [r15 + kheap_free_count - kmain]

.kfree_done:
    popfq

.kfree_bad:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; ==============================================================================
; kzalloc: Allocate zero-initialized memory block
; RCX = size in bytes
; Returns: RAX = pointer to zeroed payload (or 0 on OOM)
; ==============================================================================
kzalloc:
    push rcx
    call kmalloc
    pop rcx
    test rax, rax
    jz .kz_ret

    push rax
    push rdi
    push rcx

    mov rdi, rax
    ; Round count to qwords
    add rcx, 7
    shr rcx, 3
    xor eax, eax
    cld
    rep stosq

    pop rcx
    pop rdi
    pop rax

.kz_ret:
    ret

; ==============================================================================
; krealloc: Resize previously allocated block
; RDI = old pointer (or 0 for fresh kmalloc)
; RSI = new size in bytes (or 0 for kfree)
; Returns: RAX = new pointer (or 0 on OOM)
; ==============================================================================
krealloc:
    test rdi, rdi
    jnz .have_ptr
    mov rcx, rsi
    jmp kmalloc

.have_ptr:
    test rsi, rsi
    jnz .have_size
    call kfree
    xor eax, eax
    ret

.have_size:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8

    ; Read current block size
    lea rbx, [rdi - BLOCK_HDR_SIZE]
    cmp dword [rbx + BLOCK_OFF_MAGIC], HEAP_MAGIC
    jne .realloc_fail

    mov r8, [rbx + BLOCK_OFF_SIZE]    ; r8 = old_size

    ; If requested size fits in existing block, reuse it!
    cmp rsi, r8
    jbe .realloc_reuse

    ; Allocate larger block
    mov rcx, rsi
    call kmalloc
    test rax, rax
    jz .realloc_fail

    ; Copy min(old_size, new_size) bytes
    push rax
    mov rdi, rax
    mov rsi, [rsp + 16]               ; restore original old pointer
    mov rcx, r8
    cld
    rep movsb
    pop rax

    ; Free old block
    push rax
    mov rdi, [rsp + 16]
    call kfree
    pop rax
    jmp .realloc_done

.realloc_reuse:
    mov rax, rdi
    jmp .realloc_done

.realloc_fail:
    xor eax, eax

.realloc_done:
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

; ==============================================================================
; kheap_print: Display kernel heap statistics in terminal
; ==============================================================================
kheap_print:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    mov eax, FB_CLR_HEADER
    call fb_console_set_color
    lea rsi, [r15 + str_heap_hdr - kmain]
    call puts
    lea rsi, [r15 + str_heap_sep - kmain]
    call puts
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color

    ; Active Arenas
    lea rsi, [r15 + str_heap_arenas - kmain]
    call puts
    mov eax, FB_CLR_SUCCESS
    call fb_console_set_color
    mov eax, [r15 + kheap_arena_count - kmain]
    call putdec64
    mov al, 10
    call putc

    ; Total Capacity
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    lea rsi, [r15 + str_heap_total - kmain]
    call puts
    mov eax, FB_CLR_SUCCESS
    call fb_console_set_color
    mov rax, [r15 + kheap_total_bytes - kmain]
    call putdec64
    lea rsi, [r15 + str_heap_bytes - kmain]
    call puts

    ; Allocated Memory
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    lea rsi, [r15 + str_heap_alloc - kmain]
    call puts
    mov eax, 0xFFF59E0B               ; Amber
    call fb_console_set_color
    mov rax, [r15 + kheap_alloc_bytes - kmain]
    call putdec64
    lea rsi, [r15 + str_heap_in - kmain]
    call puts
    mov eax, [r15 + kheap_alloc_count - kmain]
    call putdec64
    lea rsi, [r15 + str_heap_blks_s - kmain]
    call puts

    ; Free Memory
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    lea rsi, [r15 + str_heap_free - kmain]
    call puts
    mov eax, 0xFF10B981               ; Emerald Green
    call fb_console_set_color
    mov rax, [r15 + kheap_free_bytes - kmain]
    call putdec64
    lea rsi, [r15 + str_heap_in - kmain]
    call puts
    mov eax, [r15 + kheap_free_count - kmain]
    call putdec64
    lea rsi, [r15 + str_heap_blks_s - kmain]
    call puts

    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color

    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; ==============================================================================
; kheap_selftest: Comprehensive runtime verification of heap allocator
; Tests 16-byte alignment, splitting, coalescing, dynamic expansion, and kzalloc
; ==============================================================================
str_ht_start        db "[heaptest] Starting Kernel Heap runtime verification...", 10, 0
str_ht_step1_ok     db "  [PASS] 16-byte alignment and multi-block allocation OK", 10, 0
str_ht_step2_ok     db "  [PASS] Bidirectional coalescing and fragmentation merge OK", 10, 0
str_ht_step3_ok     db "  [PASS] Dynamic PMM expansion (>64KB) and large block OK", 10, 0
str_ht_step4_ok     db "  [PASS] kzalloc zero-initialization verified OK", 10, 0
str_ht_pass         db "[heaptest] ALL HEAP TESTS PASSED SUCCESSFULLY!", 10, 0

str_ht_err_alloc    db "  [FAIL] kmalloc returned NULL pointer!", 10, 0
str_ht_err_align    db "  [FAIL] Allocation pointer not 16-byte aligned!", 10, 0
str_ht_err_corrupt  db "  [FAIL] Payload data integrity check failed!", 10, 0
str_ht_err_coalesce db "  [FAIL] Coalescing failed to merge adjacent free blocks!", 10, 0
str_ht_err_stats    db "  [FAIL] Heap stats inconsistent after free!", 10, 0
str_ht_err_expand   db "  [FAIL] Failed to dynamically expand heap via PMM!", 10, 0
str_ht_err_arenacnt db "  [FAIL] Arena count did not increment on expansion!", 10, 0
str_ht_err_notzero  db "  [FAIL] kzalloc memory was not zero-initialized!", 10, 0

kheap_selftest:
    push rax
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
    push r13

    mov eax, FB_CLR_HEADER
    call fb_console_set_color
    lea rsi, [r15 + str_ht_start - kmain]
    call puts
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color

    ; Test 1: 16-byte alignment and basic allocation
    mov rcx, 48
    call kmalloc
    test rax, rax
    jz .test_fail_alloc
    test al, 0x0F                     ; must be 16-byte aligned
    jnz .test_fail_align
    mov r12, rax                      ; r12 = p1 (48 bytes)

    ; Write signature
    mov rax, 0x1122334455667788
    mov [r12], rax

    mov rcx, 100
    call kmalloc
    test rax, rax
    jz .test_fail_alloc
    test al, 0x0F
    jnz .test_fail_align
    mov r13, rax                      ; r13 = p2 (100 -> 112 bytes)

    mov rcx, 200
    call kmalloc
    test rax, rax
    jz .test_fail_alloc
    test al, 0x0F
    jnz .test_fail_align
    mov r11, rax                      ; r11 = p3 (200 -> 208 bytes)

    lea rsi, [r15 + str_ht_step1_ok - kmain]
    call puts

    ; Test 2: Verify data integrity
    mov rax, 0x1122334455667788
    cmp [r12], rax
    jne .test_fail_corrupt

    ; Test 3: Coalescing test
    ; Free p2 (middle block)
    mov rdi, r13
    call kfree

    ; Free p1 (left block) -> should coalesce with p2!
    mov rdi, r12
    call kfree

    ; Free p3 (right block) -> should coalesce with p1+p2 back to 1 free block!
    mov rdi, r11
    call kfree

    ; Check if free count is 1 and alloc count is 0
    cmp dword [r15 + kheap_alloc_count - kmain], 0
    jne .test_fail_stats
    cmp dword [r15 + kheap_free_count - kmain], 1
    jne .test_fail_coalesce

    lea rsi, [r15 + str_ht_step2_ok - kmain]
    call puts

    ; Test 4: Dynamic expansion (request 70000 bytes > 64KB initial arena)
    mov rcx, 70000
    call kmalloc
    test rax, rax
    jz .test_fail_expand
    mov r12, rax

    ; Verify arena count expanded to 2
    cmp dword [r15 + kheap_arena_count - kmain], 2
    jb .test_fail_arena_cnt

    ; Write test pattern across the expanded memory
    mov dword [r12], 0xCAFEBABE
    mov dword [r12 + 69984], 0xDEADBEEF
    cmp dword [r12], 0xCAFEBABE
    jne .test_fail_corrupt
    cmp dword [r12 + 69984], 0xDEADBEEF
    jne .test_fail_corrupt

    ; Free the large allocation
    mov rdi, r12
    call kfree

    lea rsi, [r15 + str_ht_step3_ok - kmain]
    call puts

    ; Test 5: kzalloc (zero verification)
    mov rcx, 128
    call kzalloc
    test rax, rax
    jz .test_fail_alloc
    mov r12, rax
    ; Check all 16 qwords are 0
    xor ecx, ecx
.check_zero:
    cmp qword [r12 + rcx*8], 0
    jne .test_fail_not_zero
    inc ecx
    cmp ecx, 16
    jb .check_zero
    mov rdi, r12
    call kfree

    lea rsi, [r15 + str_ht_step4_ok - kmain]
    call puts

    ; All Passed!
    mov eax, FB_CLR_SUCCESS
    call fb_console_set_color
    lea rsi, [r15 + str_ht_pass - kmain]
    call puts
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    jmp .test_exit

.test_fail_alloc:
    lea rsi, [r15 + str_ht_err_alloc - kmain]
    jmp .test_error
.test_fail_align:
    lea rsi, [r15 + str_ht_err_align - kmain]
    jmp .test_error
.test_fail_corrupt:
    lea rsi, [r15 + str_ht_err_corrupt - kmain]
    jmp .test_error
.test_fail_coalesce:
    lea rsi, [r15 + str_ht_err_coalesce - kmain]
    jmp .test_error
.test_fail_stats:
    lea rsi, [r15 + str_ht_err_stats - kmain]
    jmp .test_error
.test_fail_expand:
    lea rsi, [r15 + str_ht_err_expand - kmain]
    jmp .test_error
.test_fail_arena_cnt:
    lea rsi, [r15 + str_ht_err_arenacnt - kmain]
    jmp .test_error
.test_fail_not_zero:
    lea rsi, [r15 + str_ht_err_notzero - kmain]
    jmp .test_error

.test_error:
    push rsi
    mov eax, FB_CLR_ERROR
    call fb_console_set_color
    pop rsi
    call puts
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color

.test_exit:
    pop r13
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
    pop rax
    ret

