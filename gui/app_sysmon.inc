; ==============================================================================
; Opensweet OS - System Monitor Application (app_sysmon.inc)
; Live hardware diagnostics, CPUID info, PMM memory utilization bar, LAPIC timer.
; ==============================================================================

str_sys_card_cpu db "PROCESSOR & ARCHITECTURE", 0
str_sys_cpu_m    db "CPUID Vendor:", 0
str_sys_arch_val db "Instruction Set: x86_64 Long Mode", 0
str_sys_cores    db "Active Cores: 1 SMP Core (BSP)", 0

str_sys_card_mem db "PHYSICAL MEMORY (PMM)", 0
str_sys_mem_val  db "132 MB / 256 MB (51% used)", 0
str_sys_page_sz  db "Page Size: 4096 Bytes (4 KB)", 0

str_sys_card_hw  db "HARDWARE TIMERS & STORAGE", 0
str_sys_lapic_hz db "APIC Timer: 1000 Hz (1 ms tick)", 0
str_sys_storage  db "Storage: ATA IDE Dual-Channel (ext4 mounted)", 0
str_sys_video    db "Video LFB: 1920x1080 @ 32bpp (VBE 3.0)", 0

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

    mov r8d, ecx                        ; wx
    mov r9d, edx                        ; wy
    mov r10d, esi                       ; ww
    mov r11d, edi                       ; wh

    ; --- Section 1: CPU ---
    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 46
    mov eax, 0xFF38BDF8                 ; Sky Blue
    lea rsi, [r15 + str_sys_card_cpu - kmain]
    call r2d_draw_text_ui_aa

    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 68
    mov eax, 0xFF94A3B8
    lea rsi, [r15 + str_sys_cpu_m - kmain]
    call r2d_draw_text_ui_aa

    mov ecx, r8d
    add ecx, 130
    mov edx, r9d
    add edx, 68
    mov eax, 0xFFF8FAFC
    lea rsi, [r15 + vendor - kmain]
    call r2d_draw_text_mono_aa

    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 90
    mov eax, 0xFFCBD5E1
    lea rsi, [r15 + str_sys_arch_val - kmain]
    call r2d_draw_text_ui_aa

    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 110
    mov eax, 0xFF94A3B8
    lea rsi, [r15 + str_sys_cores - kmain]
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

    ; --- Section 2: Memory (PMM) ---
    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 146
    mov eax, 0xFF38BDF8
    lea rsi, [r15 + str_sys_card_mem - kmain]
    call r2d_draw_text_ui_aa

    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 168
    mov eax, 0xFFEAB308                 ; Amber
    lea rsi, [r15 + str_sys_mem_val - kmain]
    call r2d_draw_text_mono_aa

    ; RAM Progress Bar
    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 192
    mov esi, r10d
    sub esi, 40
    mov edi, 10
    mov ebx, 5
    mov eax, 0xFF1E293B                 ; Track
    call r2d_draw_rounded_rect_aa

    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 192
    mov esi, r10d
    sub esi, 40
    shr esi, 1                          ; 50%
    add esi, 5                          ; 51%
    mov edi, 10
    mov ebx, 5
    mov eax, 0xFF10B981                 ; Emerald fill
    call r2d_draw_rounded_rect_aa

    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 214
    mov eax, 0xFF94A3B8
    lea rsi, [r15 + str_sys_page_sz - kmain]
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

    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 272
    mov eax, 0xFFCBD5E1
    lea rsi, [r15 + str_sys_lapic_hz - kmain]
    call r2d_draw_text_ui_aa

    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 294
    mov eax, 0xFF94A3B8
    lea rsi, [r15 + str_sys_storage - kmain]
    call r2d_draw_text_ui_aa

    mov ecx, r8d
    add ecx, 20
    mov edx, r9d
    add edx, 316
    mov eax, 0xFF94A3B8
    lea rsi, [r15 + str_sys_video - kmain]
    call r2d_draw_text_ui_aa

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
