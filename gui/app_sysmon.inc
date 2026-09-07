; ==============================================================================
; Opensweet OS - System Monitor Application (app_sysmon.inc)
; Live hardware diagnostics, CPUID info, real-time PMM memory utilization bar,
; dynamic Kernel Heap stats, scheduler active tasks, and LAPIC uptime counter.
; ==============================================================================

align 16
str_sys_card_cpu   db "PROCESSOR & ARCHITECTURE", 0
str_sys_cpu_m      db "CPUID Vendor:", 0
str_sys_arch_val   db "Instruction Set: x86_64 Long Mode", 0

str_sys_card_mem   db "PHYSICAL MEMORY (PMM)", 0
str_sys_page_sz    db "Page Size: 4096 Bytes (4 KB) | Bitmap: 65,536 pages", 0

str_sys_card_hw    db "HARDWARE TIMERS & STORAGE", 0
str_sys_video      db "Video LFB: 1920x1080 @ 32bpp (VBE 3.0 Linear)", 0

str_sys_storage_ok db "Storage: ATA Drive 1 (ext4 mounted @ /)", 0
str_sys_storage_no db "Storage: ATA Drive 1 (no ext4 mounted)", 0

sys_mem_buf        rb 64                       ; "132 MB / 256 MB (51% used, 31744 pages free)"
sys_heap_buf       rb 64                       ; "Kernel Heap: 12 KB / 64 KB allocated"
sys_tasks_buf      rb 64                       ; "Active Tasks: 2 (Kernel Thread, Shell Worker)"
sys_uptime_buf     rb 64                       ; "Timer Ticks: 12400 (12s uptime @ 1000 Hz)"

; ==============================================================================
; sys_u32_to_dec: Format integer in EAX into buffer RDI (null terminated)
; Advances RDI to null terminator. Preserves RBX, RCX, RDX.
; ==============================================================================
sys_u32_to_dec:
    push rbx
    push rcx
    push rdx

    test eax, eax
    jnz .non_zero
    mov byte [rdi], '0'
    inc rdi
    mov byte [rdi], 0
    jmp .done

.non_zero:
    mov ebx, 10
    xor ecx, ecx
.div_loop:
    xor edx, edx
    div ebx
    push rdx
    inc ecx
    test eax, eax
    jnz .div_loop

.pop_loop:
    pop rax
    add al, '0'
    mov [rdi], al
    inc rdi
    loop .pop_loop
    mov byte [rdi], 0

.done:
    pop rdx
    pop rcx
    pop rbx
    ret

; ==============================================================================
; app_sys_render: Render System Monitor contents
; ECX = wx, EDX = wy, ESI = ww, EDI = wh, EAX = is_focused
; ==============================================================================
app_sys_render:
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

    mov r8d, ecx                        ; wx
    mov r9d, edx                        ; wy
    mov r10d, esi                       ; ww
    mov r11d, edi                       ; wh

    ; --- Section 1: CPU & Scheduler ---
    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 46
    mov eax, 0xFF38BDF8                 ; Sky Blue
    lea rsi, [r15 + str_sys_card_cpu - kmain]
    call r2d_draw_text_ui_aa

    ; CPUID Vendor Label
    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 68
    mov eax, 0xFF94A3B8
    lea rsi, [r15 + str_sys_cpu_m - kmain]
    call r2d_draw_text_ui_aa

    ; Real CPUID Vendor Value
    mov ecx, r8d
    add ecx, 130
    mov edx, r9d
    add edx, 68
    mov eax, 0xFFF8FAFC
    lea rsi, [r15 + vendor - kmain]
    call r2d_draw_text_mono_aa

    ; Architecture string
    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 90
    mov eax, 0xFFCBD5E1
    lea rsi, [r15 + str_sys_arch_val - kmain]
    call r2d_draw_text_ui_aa

    ; Dynamic Active Tasks String
    lea rdi, [r15 + sys_tasks_buf - kmain]
    mov dword [rdi], 'Acti'
    mov dword [rdi+4], 've T'
    mov dword [rdi+8], 'asks'
    mov word [rdi+12], ': '
    add rdi, 14
    mov eax, [r15 + sched_task_cnt - kmain]
    call sys_u32_to_dec
    mov dword [rdi], ' (Ke'
    mov dword [rdi+4], 'rnel'
    mov dword [rdi+8], ', Sh'
    mov dword [rdi+12], 'ell)'
    mov byte [rdi+16], 0

    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 110
    mov eax, 0xFF94A3B8
    lea rsi, [r15 + sys_tasks_buf - kmain]
    call r2d_draw_text_ui_aa

    ; Divider 1
    mov ecx, r8d
    add ecx, 16
    mov edx, r9d
    add edx, 134
    mov esi, r10d
    sub esi, 32
    mov edi, 1
    mov eax, 0x30FFFFFF
    call r2d_fill_rect_alpha

    ; --- Section 2: Memory (Real PMM & Kernel Heap) ---
    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 146
    mov eax, 0xFF38BDF8
    lea rsi, [r15 + str_sys_card_mem - kmain]
    call r2d_draw_text_ui_aa

    ; Query REAL free physical pages from PMM
    push r8
    push r9
    call count_free                     ; r8d = free pages count (each 4KB)
    mov r12d, r8d                       ; r12d = free pages
    pop r9
    pop r8
    mov r13d, 65536
    sub r13d, r12d                      ; r13d = used pages count

    ; used_mb = used_pages / 256
    mov eax, r13d
    shr eax, 8                          ; eax = used MB
    ; used_pct = (used_pages * 100) / 65536
    imul ecx, r13d, 100
    shr ecx, 16                         ; ecx = used %

    ; Format sys_mem_buf: "[used_mb] MB / 256 MB ([used_pct]% used, [free_pages] pages free)"
    lea rdi, [r15 + sys_mem_buf - kmain]
    push rcx
    call sys_u32_to_dec
    mov dword [rdi], ' MB '
    mov dword [rdi+4], '/ 25'
    mov word [rdi+8], '6 '
    mov word [rdi+10], 'MB'
    mov word [rdi+12], ' ('
    add rdi, 14
    pop rax
    push rax
    call sys_u32_to_dec
    mov dword [rdi], '% us'
    mov dword [rdi+4], 'ed, '
    add rdi, 8
    mov eax, r12d
    call sys_u32_to_dec
    mov dword [rdi], ' pag'
    mov dword [rdi+4], 'es f'
    mov dword [rdi+8], 'ree)'
    mov byte [rdi+12], 0
    pop r12                             ; r12 = used_pct

    ; Draw RAM metrics text
    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 168
    mov eax, 0xFFEAB308                 ; Amber
    lea rsi, [r15 + sys_mem_buf - kmain]
    call r2d_draw_text_mono_aa

    ; RAM Progress Bar Track (Slate 800)
    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 192
    mov esi, r10d
    sub esi, 40                         ; track width
    mov edi, 10
    mov ebx, 5
    mov eax, 0xFF1E293B                 ; Track background
    call r2d_draw_rounded_rect_aa

    ; Dynamic RAM Progress Bar Fill:
    ; fill_width = (track_width * used_pages) / 65536
    mov eax, r10d
    sub eax, 40
    imul eax, r13d
    shr eax, 16                         ; eax = fill_width
    cmp eax, 6
    jae @f
    mov eax, 6
@@:
    mov esi, r10d
    sub esi, 40
    cmp eax, esi
    jbe @f
    mov eax, esi
@@:
    mov esi, eax                        ; fill width

    ; Dynamic Color based on load percentage
    mov eax, 0xFF10B981                 ; Emerald if < 75%
    cmp r12d, 75
    jb .bar_color_ok
    mov eax, 0xFFF59E0B                 ; Amber if 75%..89%
    cmp r12d, 90
    jb .bar_color_ok
    mov eax, 0xFFF43F5E                 ; Rose Red if >= 90%
.bar_color_ok:

    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 192
    mov edi, 10
    mov ebx, 5
    call r2d_draw_rounded_rect_aa

    ; Dynamic Kernel Heap Metrics:
    ; sys_heap_buf = "Kernel Heap: [used/1024] KB / [total/1024] KB allocated"
    lea rdi, [r15 + sys_heap_buf - kmain]
    mov dword [rdi], 'Kern'
    mov dword [rdi+4], 'el H'
    mov dword [rdi+8], 'eap:'
    mov word [rdi+12], ' '
    add rdi, 13

    mov rax, [r15 + kheap_alloc_bytes - kmain]
    shr rax, 10                         ; used KB
    call sys_u32_to_dec
    mov dword [rdi], ' KB '
    mov word [rdi+4], '/ '
    add rdi, 6
    mov rax, [r15 + kheap_total_bytes - kmain]
    shr rax, 10                         ; total KB
    call sys_u32_to_dec
    mov dword [rdi], ' KB '
    mov dword [rdi+4], 'allo'
    mov dword [rdi+8], 'cate'
    mov word [rdi+12], 'd'
    mov byte [rdi+13], 0

    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 214
    mov eax, 0xFF94A3B8
    lea rsi, [r15 + sys_heap_buf - kmain]
    call r2d_draw_text_ui_aa

    ; Divider 2
    mov ecx, r8d
    add ecx, 16
    mov edx, r9d
    add edx, 238
    mov esi, r10d
    sub esi, 32
    mov edi, 1
    mov eax, 0x30FFFFFF
    call r2d_fill_rect_alpha

    ; --- Section 3: Hardware Timers & Storage ---
    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 250
    mov eax, 0xFF38BDF8
    lea rsi, [r15 + str_sys_card_hw - kmain]
    call r2d_draw_text_ui_aa

    ; Real Timer Ticks & Uptime Counter
    ; sys_uptime_buf = "Timer Ticks: [ticks] ([ticks/1000]s uptime @ 1000 Hz)"
    lea rdi, [r15 + sys_uptime_buf - kmain]
    mov dword [rdi], 'Time'
    mov dword [rdi+4], 'r Ti'
    mov dword [rdi+8], 'cks:'
    mov byte [rdi+12], ' '
    add rdi, 13

    mov eax, [r15 + timer_ticks - kmain]
    push rax
    call sys_u32_to_dec
    mov word [rdi], ' ('
    add rdi, 2
    pop rax
    xor edx, edx
    mov ecx, 1000
    div ecx
    call sys_u32_to_dec
    mov dword [rdi], 's up'
    mov dword [rdi+4], 'time'
    mov dword [rdi+8], ' @ 1'
    mov dword [rdi+12], '000 '
    mov word [rdi+16], 'Hz'
    mov byte [rdi+18], ')'
    mov byte [rdi+19], 0

    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 272
    mov eax, 0xFFCBD5E1
    lea rsi, [r15 + sys_uptime_buf - kmain]
    call r2d_draw_text_ui_aa

    ; Real Storage Status
    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 294
    mov eax, 0xFF94A3B8
    cmp byte [r15 + ext4_ok - kmain], 1
    jne .stor_none
    lea rsi, [r15 + str_sys_storage_ok - kmain]
    jmp .stor_draw
.stor_none:
    lea rsi, [r15 + str_sys_storage_no - kmain]
.stor_draw:
    call r2d_draw_text_ui_aa

    ; Video Mode Information
    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 316
    mov eax, 0xFF94A3B8
    lea rsi, [r15 + str_sys_video - kmain]
    call r2d_draw_text_ui_aa

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
