; Opensweet OS - x86_64 kernel (stage 2), entered in long mode at 0x10000.
; VGA text output, serial COM1 mirror, polled PS/2 keyboard echo shell.

VGA_BASE = 0xB8000
COLS     = 80
ROWS     = 25
COM1     = 0x3F8

; --- Local APIC ---
LAPIC_BASE = 0xFEE00000
LAPIC_EOI   = LAPIC_BASE + 0x0B0
LAPIC_SPUR  = LAPIC_BASE + 0x0F0
LAPIC_LVTTR = LAPIC_BASE + 0x320
LAPIC_TICR  = LAPIC_BASE + 0x380
LAPIC_TCCR  = LAPIC_BASE + 0x390
LAPIC_TDCR  = LAPIC_BASE + 0x3E0
TIMER_VEC   = 0x30                ; outside PIC range 0x20-0x2F

; --- memory map (from boot) ---
MEMMAP_COUNT = 0x6000
MEMMAP_BASE  = 0x6100

; --- physical memory manager: bitmap of 4KB pages, covers first 256MB ---
PMM_BITMAP   = 0x60000            ; 8KB bitmap, identity-mapped low RAM
BITMAP_BITS  = 65536              ; 256MB / 4KB
BITMAP_DWORDS = BITMAP_BITS / 32
BITMAP_BYTES = BITMAP_BITS / 8
RESERVE_PAGES = 0x100             ; first 1MB (BIOS, kernel image, page tables, stacks)

; --- VMM test target ---
TEST_VIRT    = 0x6000000000

; --- VBE framebuffer (slots filled by boot @0x5010..) ---
VBS_PITCH  = 0x5010
VBS_WIDTH  = 0x5012
VBS_HEIGHT = 0x5014
VBS_BPP    = 0x501A
VBS_LFB    = 0x5028
VBS_OK     = 0x50FE
IN_OFF_MODE = 0
IN_OFF_SIZE = 4

org 0xFFFF800000010000
use64

kmain:
    ; entered at PHYS 0x10000 via identity map (boot jumped low);
    ; all absolute addresses below resolve to HIGHER-HALF via PML4[256].
    mov rsp, 0x90000
    mov r15, 0xFFFF800000010000   ; image base: all data refs go via [r15 + ...]
    mov rax, .high            ; jump to same code via higher-half VA
    jmp rax
.high:
    cli
    cld
    mov rsp, 0x90000          ; low stack for now (identity-mapped)
    xor r14d, r14d            ; r14 = 0: base for low-memory absolute refs

    ; --- init COM1: 38400 8N1 ---
    mov dx, COM1+1
    xor al, al
    out dx, al
    mov dx, COM1+3
    mov al, 0x80
    out dx, al
    mov dx, COM1
    mov al, 3
    out dx, al
    inc dx
    xor al, al
    out dx, al
    mov dx, COM1+3
    mov al, 3
    out dx, al
    mov dx, COM1+2
    mov al, 0xC7
    out dx, al
    mov dx, COM1+4
    mov al, 0x0B
    out dx, al

    ; --- clear VGA screen ---
    mov rdi, VGA_BASE
    mov ecx, COLS*ROWS
    mov ax, 0x0720
    rep stosw
    mov word [r15 + cur - kmain], 0

    call init_idt
    call pic_init
    call pit_init

    sti
    call apic_init
    call pmm_init
    call kheap_init
    call fb_console_init
    call ata_init
    call ext4_mount

    ; --- CPU vendor via CPUID ---
    xor eax, eax
    cpuid
    mov dword [r15 + vendor - kmain], ebx
    mov dword [r15 + vendor - kmain+4], edx
    mov dword [r15 + vendor - kmain+8], ecx
    mov byte [r15 + vendor - kmain+12], 0

    call mouse_init
    call modern_desktop_init

    ; Initialize Preemptive Multitasking Scheduler
    call sched_init

    ; Spawn Background Clock Daemon (Task 1)
    lea rsi, [r15 + str_name_clock - kmain]
    lea rdx, [r15 + task_clock_daemon - kmain]
    call task_create

    ; Spawn Background System Monitor Daemon (Task 2)
    lea rsi, [r15 + str_name_sysmon - kmain]
    lea rdx, [r15 + task_sysmon_daemon - kmain]
    call task_create

    ; Welcome Banner
    mov eax, FB_CLR_HEADER
    call fb_console_set_color
    lea rsi, [r15 + str_os_title - kmain]
    call puts

    mov eax, FB_CLR_LABEL
    call fb_console_set_color
    lea rsi, [r15 + str_os_subtitle - kmain]
    call puts

    mov eax, FB_CLR_SUCCESS
    call fb_console_set_color
    lea rsi, [r15 + str_os_ready - kmain]
    call puts

    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    jmp .prompt

.shell:
    call md_handle_mouse
    cmp byte [r15 + md_term_dirty - kmain], 0
    je .no_render
    mov byte [r15 + md_term_dirty - kmain], 0
    call modern_desktop_render
.no_render:
    call getc
    test al, al
    jnz .key

    ; Fast path: If mouse moved or button changed while blitting, loop immediately without hlt sleep
    mov eax, [r15 + mouse_x - kmain]
    cmp eax, [r15 + md_mouse_x - kmain]
    jne .shell
    mov edx, [r15 + mouse_y - kmain]
    cmp edx, [r15 + md_mouse_y - kmain]
    jne .shell
    mov al, [r15 + mouse_buttons - kmain]
    and al, 1
    cmp al, [r15 + wm_prev_lmb - kmain]
    jne .shell

    hlt                       ; sleep until IRQ
    jmp .shell
.key:
    cmp al, 10
    je .enter
    cmp al, 8
    je .bs
    movzx ecx, byte [r15 + cmd_len - kmain]
    cmp ecx, 127
    jae .echo
    mov byte [r15 + cmd_buf - kmain + rcx], al
    inc byte [r15 + cmd_len - kmain]
.echo:
    call putc
    jmp .shell
.bs:
    cmp byte [r15 + cmd_len - kmain], 0
    je .shell
    dec byte [r15 + cmd_len - kmain]
    mov al, 8
    call putc
    jmp .shell
.enter:
    mov al, 13                ; CR + LF on newline
    call putc
    mov al, 10
    call putc
    movzx ecx, byte [r15 + cmd_len - kmain]
    mov byte [r15 + cmd_buf - kmain + rcx], 0
    mov byte [r15 + cmd_len - kmain], 0

    ; --- dispatch command ---
    cmp byte [r15 + cmd_buf - kmain], 0
    je .prompt

    ; help
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_help - kmain]
    call streq
    test al, al
    jnz .do_help

    ; clear / cls
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_clear - kmain]
    call streq
    test al, al
    jnz .do_clear
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_cls - kmain]
    call streq
    test al, al
    jnz .do_clear

    ; pwd
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_pwd - kmain]
    call streq
    test al, al
    jnz .do_pwd

    ; mem
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_mem - kmain]
    call streq
    test al, al
    jnz .do_mem

    ; cpu
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_cpu - kmain]
    call streq
    test al, al
    jnz .do_cpu

    ; pci
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_pci - kmain]
    call streq
    test al, al
    jnz .do_pci

    ; uptime / ticks
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_uptime - kmain]
    call streq
    test al, al
    jnz .do_uptime
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_ticks - kmain]
    call streq
    test al, al
    jnz .do_uptime

    ; vbe
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_vbe - kmain]
    call streq
    test al, al
    jnz .do_vbe

    ; fps
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_fps - kmain]
    call streq
    test al, al
    jnz .do_fps

    ; reboot
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_reboot - kmain]
    call streq
    test al, al
    jnz .do_reboot

    ; poweroff / exit
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_poweroff - kmain]
    call streq
    test al, al
    jnz .do_poweroff
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_exit - kmain]
    call streq
    test al, al
    jnz .do_poweroff

    ; ata
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_ata - kmain]
    call streq
    test al, al
    jnz .do_ata

    ; map
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_map - kmain]
    call streq
    test al, al
    jnz .do_map

    ; exc
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_exc - kmain]
    call streq
    test al, al
    jnz .do_exc

    ; div
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_div - kmain]
    call streq
    test al, al
    jnz .do_div

    ; ls: check if "ls" exact or starts with "ls "
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_ls - kmain]
    call streq
    test al, al
    jnz .do_ls
    mov rsi, cmd_buf
    lea rdi, [r15 + str_lssp - kmain]
    mov ecx, 3
    call strpref
    test al, al
    jnz .do_ls

    ; cd: check if "cd" exact or starts with "cd "
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_cd - kmain]
    call streq
    test al, al
    jnz .do_cd
    mov rsi, cmd_buf
    lea rdi, [r15 + str_cdsp - kmain]
    mov ecx, 3
    call strpref
    test al, al
    jnz .do_cd

    ; cat: starts with "cat "
    mov rsi, cmd_buf
    lea rdi, [r15 + str_catsp - kmain]
    mov ecx, 4
    call strpref
    test al, al
    jnz .do_cat

    ; stat: starts with "stat "
    mov rsi, cmd_buf
    lea rdi, [r15 + str_statsp - kmain]
    mov ecx, 5
    call strpref
    test al, al
    jnz .do_stat

    ; wallpaper: "wallpaper" or "wallpaper <path>"
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_wallpaper - kmain]
    call streq
    test al, al
    jnz .do_wallpaper_default
    mov rsi, cmd_buf
    lea rdi, [r15 + str_wallpapersp - kmain]
    mov ecx, 10
    call strpref
    test al, al
    jnz .do_wallpaper_arg

    ; tasks / ps
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_tasks - kmain]
    call streq
    test al, al
    jnz .do_tasks
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_ps - kmain]
    call streq
    test al, al
    jnz .do_tasks

    ; heap
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_heap - kmain]
    call streq
    test al, al
    jnz .do_heap

    ; heaptest
    mov rsi, cmd_buf
    lea rdi, [r15 + cmd_heaptest - kmain]
    call streq
    test al, al
    jnz .do_heaptest

    ; Unknown command
    mov eax, FB_CLR_ERROR
    call fb_console_set_color
    lea rsi, [r15 + str_cmd_notfound1 - kmain]
    call puts
    lea rsi, [r15 + cmd_buf - kmain]
    call puts
    lea rsi, [r15 + str_cmd_notfound2 - kmain]
    call puts
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    jmp .prompt

.do_heaptest:
    call kheap_selftest
    jmp .prompt

.do_heap:
    call kheap_print
    jmp .prompt

.do_tasks:
    call sched_print_tasks
    jmp .prompt

.do_help:
    mov eax, FB_CLR_HEADER
    call fb_console_set_color
    lea rsi, [r15 + str_help_hdr - kmain]
    call puts
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    lea rsi, [r15 + str_help_body - kmain]
    call puts
    jmp .prompt

.do_clear:
    call fb_console_clear
    jmp .prompt

.do_pwd:
    call ext4_pwd
    jmp .prompt

.do_cd:
    cmp byte [r15 + ext4_ok - kmain], 0
    je .fs_err
    lea rsi, [r15 + cmd_buf - kmain + 2]
    call ext4_cd
    jmp .prompt

.do_ls:
    cmp byte [r15 + ext4_ok - kmain], 0
    je .fs_err
    lea rsi, [r15 + cmd_buf - kmain + 2]
.ls_sp:
    cmp byte [rsi], ' '
    jne .ls_sp_done
    inc rsi
    jmp .ls_sp
.ls_sp_done:
    cmp byte [rsi], 0
    je .ls_cwd
    call ext4_lookup
    test eax, eax
    jz .fs_err
    call ext4_inode_load
    jc .fs_err
    call ext4_ls_print
    jmp .prompt
.ls_cwd:
    mov eax, [r15 + ext4_cwd_inode - kmain]
    call ext4_inode_load
    jc .fs_err
    call ext4_ls_print
    jmp .prompt

.do_cat:
    cmp byte [r15 + ext4_ok - kmain], 0
    je .fs_err
    lea rsi, [r15 + cmd_buf - kmain + 3]
.cat_sp:
    cmp byte [rsi], ' '
    jne .cat_sp_done
    inc rsi
    jmp .cat_sp
.cat_sp_done:
    cmp byte [rsi], 0
    je .cat_usage

    call ext4_lookup
    test eax, eax
    jz .fs_err
    call ext4_inode_load
    jc .fs_err
    movzx edx, word [r15 + ext4_inode - kmain + IN_OFF_MODE]
    and edx, 0xF000
    cmp edx, 0x4000
    je .cat_isdir

    call ext4_cat_print
    mov al, 10
    call putc
    jmp .prompt

.cat_usage:
    mov eax, FB_CLR_LABEL
    call fb_console_set_color
    lea rsi, [r15 + str_cat_usage - kmain]
    call puts
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    jmp .prompt

.cat_isdir:
    mov eax, FB_CLR_ERROR
    call fb_console_set_color
    lea rsi, [r15 + str_cat_isdir - kmain]
    call puts
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    jmp .prompt

.do_stat:
    cmp byte [r15 + ext4_ok - kmain], 0
    je .fs_err
    lea rsi, [r15 + cmd_buf - kmain + 4]
    call ext4_stat_print
    jmp .prompt

.do_wallpaper_default:
    cmp byte [r15 + ext4_ok - kmain], 0
    je .fs_err
    lea rsi, [r15 + str_default_wallpaper - kmain]
    jmp .do_load_wall

.do_wallpaper_arg:
    cmp byte [r15 + ext4_ok - kmain], 0
    je .fs_err
    lea rsi, [r15 + cmd_buf - kmain + 10]
.skip_wall_sp:
    cmp byte [rsi], ' '
    jne .do_load_wall
    inc rsi
    jmp .skip_wall_sp

.do_load_wall:
    call png_load_wallpaper_from_ext4
    jnc .wall_cmd_ok
    mov eax, FB_CLR_ERROR
    call fb_console_set_color
    lea rsi, [r15 + str_wall_err - kmain]
    call puts
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    jmp .prompt

.wall_cmd_ok:
    mov eax, FB_CLR_SUCCESS
    call fb_console_set_color
    lea rsi, [r15 + str_wall_ok - kmain]
    call puts
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    call modern_desktop_render
    jmp .prompt

.do_mem:
    mov eax, FB_CLR_HEADER
    call fb_console_set_color
    lea rsi, [r15 + str_mem_hdr - kmain]
    call puts

    call count_free                   ; r8d = free pages count
    mov r9d, 65536
    sub r9d, r8d                      ; r9d = used pages count

    mov eax, FB_CLR_LABEL
    call fb_console_set_color
    lea rsi, [r15 + str_mem_total - kmain]
    call puts

    mov eax, FB_CLR_LABEL
    call fb_console_set_color
    lea rsi, [r15 + str_mem_used - kmain]
    call puts
    mov eax, FB_CLR_NUMBER
    call fb_console_set_color
    mov eax, r9d
    call putdec64
    lea rsi, [r15 + str_mem_pages - kmain]
    call puts

    mov eax, FB_CLR_LABEL
    call fb_console_set_color
    lea rsi, [r15 + str_mem_free - kmain]
    call puts
    mov eax, FB_CLR_SUCCESS
    call fb_console_set_color
    mov eax, r8d
    call putdec64
    lea rsi, [r15 + str_mem_pages - kmain]
    call puts

    mov eax, FB_CLR_LABEL
    call fb_console_set_color
    lea rsi, [r15 + str_mem_pgsz - kmain]
    call puts

    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    jmp .prompt

.do_cpu:
    mov eax, FB_CLR_HEADER
    call fb_console_set_color
    lea rsi, [r15 + str_cpu_hdr - kmain]
    call puts

    mov eax, FB_CLR_LABEL
    call fb_console_set_color
    lea rsi, [r15 + str_cpu_lbl_vendor - kmain]
    call puts
    mov eax, FB_CLR_FILE
    call fb_console_set_color
    lea rsi, [r15 + vendor - kmain]
    call puts
    mov al, 10
    call putc

    ; Processor Brand String (CPUID 0x80000002..0x80000004)
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000004
    jb .no_brand

    lea rdi, [r15 + cpu_brand_str - kmain]
    mov eax, 0x80000002
    cpuid
    mov [rdi + 0], eax
    mov [rdi + 4], ebx
    mov [rdi + 8], ecx
    mov [rdi + 12], edx
    mov eax, 0x80000003
    cpuid
    mov [rdi + 16], eax
    mov [rdi + 20], ebx
    mov [rdi + 24], ecx
    mov [rdi + 28], edx
    mov eax, 0x80000004
    cpuid
    mov [rdi + 32], eax
    mov [rdi + 36], ebx
    mov [rdi + 40], ecx
    mov [rdi + 44], edx
    mov byte [rdi + 48], 0

    mov eax, FB_CLR_LABEL
    call fb_console_set_color
    lea rsi, [r15 + str_cpu_lbl_model - kmain]
    call puts
    mov eax, FB_CLR_FILE
    call fb_console_set_color
    lea rsi, [r15 + cpu_brand_str - kmain]
    call puts
    mov al, 10
    call putc

.no_brand:
    mov eax, 1
    cpuid
    mov r8d, edx
    mov r9d, ecx

    mov eax, FB_CLR_LABEL
    call fb_console_set_color
    lea rsi, [r15 + str_cpu_lbl_feat - kmain]
    call puts
    mov eax, FB_CLR_SUCCESS
    call fb_console_set_color

    test r8d, 1 shl 9
    jz @f
    lea rsi, [r15 + str_feat_apic - kmain]
    call puts
@@: test r8d, 1 shl 4
    jz @f
    lea rsi, [r15 + str_feat_tsc - kmain]
    call puts
@@: test r8d, 1 shl 5
    jz @f
    lea rsi, [r15 + str_feat_msr - kmain]
    call puts
@@: test r8d, 1 shl 25
    jz @f
    lea rsi, [r15 + str_feat_sse - kmain]
    call puts
@@: test r8d, 1 shl 26
    jz @f
    lea rsi, [r15 + str_feat_sse2 - kmain]
    call puts
@@: test r9d, 1 shl 0
    jz @f
    lea rsi, [r15 + str_feat_sse3 - kmain]
    call puts
@@: test r9d, 1 shl 28
    jz @f
    lea rsi, [r15 + str_feat_avx - kmain]
    call puts
@@: mov al, 10
    call putc

    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    jmp .prompt

.do_pci:
    mov eax, FB_CLR_HEADER
    call fb_console_set_color
    lea rsi, [r15 + str_pci_hdr - kmain]
    call puts

    xor r8d, r8d                      ; dev 0..31
.pci_dev_loop:
    xor r9d, r9d                      ; func 0..7
.pci_func_loop:
    mov eax, 0x80000000
    mov ecx, r8d
    shl ecx, 11
    or eax, ecx
    mov ecx, r9d
    shl ecx, 8
    or eax, ecx

    mov dx, 0xCF8
    out dx, eax
    mov dx, 0xCFC
    in eax, dx

    cmp ax, 0xFFFF
    je .pci_next_func

    mov r10d, eax

    ; Read Class Code (reg 2, offset 8)
    mov eax, 0x80000000
    mov ecx, r8d
    shl ecx, 11
    or eax, ecx
    mov ecx, r9d
    shl ecx, 8
    or eax, ecx
    or eax, 0x08
    mov dx, 0xCF8
    out dx, eax
    mov dx, 0xCFC
    in eax, dx
    shr eax, 16
    mov r11d, eax

    mov eax, FB_CLR_DIR
    call fb_console_set_color
    lea rsi, [r15 + str_pci_prefix - kmain]
    call puts

    mov eax, FB_CLR_FILE
    call fb_console_set_color
    mov al, r8b
    call puthex8
    mov al, '.'
    call putc
    mov al, r9b
    add al, '0'
    call putc

    lea rsi, [r15 + str_pci_vend - kmain]
    call puts
    mov eax, r10d
    call puthex16

    lea rsi, [r15 + str_pci_dev - kmain]
    call puts
    mov eax, r10d
    shr eax, 16
    call puthex16

    lea rsi, [r15 + str_pci_cls - kmain]
    call puts
    mov eax, r11d
    shr eax, 8
    and eax, 0xFF
    call puthex8

    cmp al, 0x06
    je .cls_bridge
    cmp al, 0x03
    je .cls_vga
    cmp al, 0x01
    je .cls_storage
    cmp al, 0x02
    je .cls_net
    jmp .cls_done

.cls_bridge:
    mov eax, FB_CLR_MUTED
    call fb_console_set_color
    lea rsi, [r15 + str_cls_bridge - kmain]
    call puts
    jmp .cls_done
.cls_vga:
    mov eax, FB_CLR_SUCCESS
    call fb_console_set_color
    lea rsi, [r15 + str_cls_vga - kmain]
    call puts
    jmp .cls_done
.cls_storage:
    mov eax, FB_CLR_SIZE
    call fb_console_set_color
    lea rsi, [r15 + str_cls_storage - kmain]
    call puts
    jmp .cls_done
.cls_net:
    mov eax, FB_CLR_DIR
    call fb_console_set_color
    lea rsi, [r15 + str_cls_net - kmain]
    call puts

.cls_done:
    mov al, 10
    call putc

.pci_next_func:
    inc r9d
    cmp r9d, 8
    jb .pci_func_loop

    inc r8d
    cmp r8d, 32
    jb .pci_dev_loop

    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    jmp .prompt

.do_uptime:
    mov eax, FB_CLR_LABEL
    call fb_console_set_color
    lea rsi, [r15 + str_uptime_lbl - kmain]
    call puts
    mov eax, FB_CLR_NUMBER
    call fb_console_set_color
    mov eax, [r15 + timer_ticks - kmain]
    call putdec64
    lea rsi, [r15 + str_uptime_ticks - kmain]
    call puts
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    jmp .prompt

.do_fps:
    mov eax, FB_CLR_HEADER
    call fb_console_set_color
    lea rsi, [r15 + str_fps_hdr - kmain]
    call puts

    mov eax, FB_CLR_LABEL
    call fb_console_set_color
    lea rsi, [r15 + str_fps_lbl - kmain]
    call puts

    mov eax, FB_CLR_SUCCESS
    call fb_console_set_color
    mov eax, [r15 + gui_fps - kmain]
    call putdec64
    lea rsi, [r15 + str_fps_unit - kmain]
    call puts

    mov eax, FB_CLR_LABEL
    call fb_console_set_color
    lea rsi, [r15 + str_fps_frames - kmain]
    call puts

    mov eax, FB_CLR_NUMBER
    call fb_console_set_color
    mov eax, [r15 + gui_frame_count - kmain]
    call putdec64

    mov eax, FB_CLR_LABEL
    call fb_console_set_color
    lea rsi, [r15 + str_fps_engine - kmain]
    call puts

    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    jmp .prompt


.do_reboot:
    mov eax, FB_CLR_SIZE
    call fb_console_set_color
    lea rsi, [r15 + str_reboot_msg - kmain]
    call puts
    mov al, 0xFE
    out 0x64, al
    hlt
    jmp .prompt

.do_poweroff:
    mov eax, FB_CLR_SIZE
    call fb_console_set_color
    lea rsi, [r15 + str_poweroff_msg - kmain]
    call puts
    mov ax, 0x2000
    mov dx, 0x604
    out dx, ax
    mov dx, 0xB004
    out dx, ax
    hlt
    jmp .prompt

.do_exc:
    db 0xCC                   ; int3 -> #BP (vector 3)
.do_div:
    xor ecx, ecx
    xor edx, edx
    div ecx                   ; -> #DE (vector 0)
.do_ticks:
    jmp .do_uptime

.do_map:
    call pmm_alloc_zero       ; phys page (zeroed)
    test rax, rax
    jz .map_oom
    push rax
    mov rdi, TEST_VIRT
    mov r11d, 3               ; P|RW
    call vmm_map
    mov rcx, 0x1122334455667788
    mov rax, TEST_VIRT
    mov [rax], rcx            ; write via new mapping
    mov rdx, [rax]            ; read back
    mov rsi, msg_mapval
    call puts
    mov rax, rdx
    call puthex64
    mov al, 10
    call putc
    mov rdi, TEST_VIRT
    call vmm_unmap
    pop rax
    call pmm_free
    jmp .prompt
.map_oom:
    mov rsi, msg_oom
    call puts
    jmp .prompt

.do_ata:
    mov rsi, msg_atasec
    call puts
    mov eax, dword [r15 + ata_sectors - kmain]
    call puthex64
    mov al, 10
    call putc
    lea rdi, [r15 + ata_buf - kmain]
    xor eax, eax             ; LBA 0
    mov ecx, 1
    call disk_read_blocks
    jc .ata_err
    mov rsi, msg_atadump
    call puts
    lea rbx, [r15 + ata_buf - kmain]
    xor ecx, ecx             ; byte index
.ata_byte:
    movzx eax, byte [rbx + rcx]
    shr al, 4                ; high nibble
    call hexdigit
    call putc
    movzx eax, byte [rbx + rcx]
    and al, 0xF              ; low nibble
    call hexdigit
    call putc
    mov al, ' '
    call putc
    inc ecx
    cmp ecx, 16
    jb .ata_byte
    mov al, 10
    call putc
    jmp .prompt
.ata_err:
    mov rsi, msg_ataerr
    call puts
    jmp .prompt

.fs_err:
    mov eax, FB_CLR_ERROR
    call fb_console_set_color
    lea rsi, [r15 + msg_fserr - kmain]
    call puts
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    jmp .prompt

.do_vbe:
    mov eax, FB_CLR_LABEL
    call fb_console_set_color
    lea rsi, [r15 + str_vbe_hdr - kmain]
    call puts
    mov eax, FB_CLR_NUMBER
    call fb_console_set_color
    movzx eax, word [r14 + VBS_WIDTH]
    call putdec64
    mov al, 'x'
    call putc
    movzx eax, word [r14 + VBS_HEIGHT]
    call putdec64
    lea rsi, [r15 + str_vbe_at - kmain]
    call puts
    movzx eax, byte [r14 + VBS_BPP]
    call putdec64
    lea rsi, [r15 + str_vbe_bpp - kmain]
    call puts
    movzx eax, word [r14 + VBS_PITCH]
    call putdec64
    lea rsi, [r15 + str_vbe_lfb - kmain]
    call puts
    mov eax, dword [r14 + VBS_LFB]
    call puthex32
    mov al, 10
    call putc
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    jmp .prompt

.prompt:
    mov byte [r15 + cmd_len - kmain], 0
    mov eax, FB_CLR_PROMPT
    call fb_console_set_color
    lea rsi, [r15 + prompt_user - kmain]
    call puts
    lea rsi, [r15 + ext4_cwd_path - kmain]
    call puts
    lea rsi, [r15 + prompt_sym - kmain]
    call puts
    mov eax, FB_CLR_DEFAULT
    call fb_console_set_color
    call modern_desktop_render
    jmp .shell

; --- read next ASCII from keyboard ring buffer; AL=0 if empty ---
getc:
    movzx ecx, byte [r15 + kb_tail - kmain]
    movzx edx, byte [r15 + kb_head - kmain]
    cmp ecx, edx
    je .empty
    mov al, [r15 + kb_buf - kmain + rcx]
    inc ecx
    and ecx, 31
    mov [r15 + kb_tail - kmain], cl
    ret
.empty:
    xor eax, eax
    ret

; --- IRQ1: translate scancode -> ASCII, push into ring buffer ---
kb_irq:
    in  al, 0x60
    cmp al, 0x2A              ; LShift make
    je .sh_on
    cmp al, 0x36              ; RShift make
    je .sh_on
    cmp al, 0xAA
    je .sh_off
    cmp al, 0xB6
    je .sh_off
    test al, 0x80             ; ignore breaks
    jnz .skip
    cmp al, KEYMAP_SIZE
    jae .skip
    lea rbx, [r15 + keymap_norm - kmain]
    cmp byte [r15 + shift - kmain], 0
    je .sel
    lea rbx, [r15 + keymap_shift - kmain]
.sel:
    movzx r8d, al
    mov al, [rbx + r8]
    test al, al
    jz .skip
    movzx ecx, byte [r15 + kb_head - kmain]
    lea edx, [ecx+1]
    and edx, 31
    movzx r8d, byte [r15 + kb_tail - kmain]
    cmp edx, r8d
    je .skip                  ; full - drop char
    mov [r15 + kb_buf - kmain + rcx], al
    mov [r15 + kb_head - kmain], dl
.skip:
    ret
.sh_on:
    mov byte [r15 + shift - kmain], 1
    ret
.sh_off:
    mov byte [r15 + shift - kmain], 0
    ret

; --- print ASCIIZ string at RSI ---
puts:
    lodsb
    test al, al
    jz  .done
    call putc
    jmp puts
.done:
    ret

; --- print char AL to serial + Framebuffer Console ---
putc:
    push rax
    push rbx
    push rdx

    mov bl, al                        ; bl holds character safely

    ; 1. Serial COM1
.wait_com1:
    mov dx, COM1+5
    in al, dx
    test al, 0x20
    jz .wait_com1
    mov dx, COM1
    mov al, bl
    out dx, al

    ; 2. Modern Terminal buffer
    mov al, bl
    call md_term_putc

    pop rdx
    pop rbx
    pop rax
    ret

; --- IDT: vectors 0..31 -> exception stubs, halt-loop on fault ---
init_idt:
    lea rdi, [r15 + idt - kmain]
    mov ecx, 256*16/4
    xor eax, eax
    rep stosd
    lea r11, [r15 + idt - kmain]   ; r11 = idt base (rdi is now END of table!)

    mov rsi, ex_table
    xor ebx, ebx
.next:
    mov rax, [rsi]
    mov rdi, rbx
    shl rdi, 4                ; entry size = 16 bytes
    add rdi, r11
    mov [rdi], ax             ; offset 15:0
    shr rax, 16
    mov word [rdi+6], ax      ; offset 31:16
    shr rax, 16
    mov [rdi+8], eax          ; offset 63:32
    mov word [rdi+2], 0x18    ; CODE64_SEL
    mov byte [rdi+5], 0x8E    ; present | ring0 | interrupt gate
    add rsi, 8
    inc rbx
    cmp rbx, 32
    jb .next

    ; vectors 32..47 <- irq_table
    mov rsi, irq_table
    xor ebx, ebx
.irq_next:
    mov rax, [rsi]
    mov rdi, rbx
    shl rdi, 4
    add rdi, r11
    add rdi, 32*16
    mov [rdi], ax
    shr rax, 16
    mov word [rdi+6], ax
    shr rax, 16
    mov [rdi+8], eax
    mov word [rdi+2], 0x18    ; CODE64_SEL
    mov byte [rdi+5], 0x8E    ; present | ring0 | interrupt gate
    add rsi, 8
    inc rbx
    cmp rbx, 16
    jb .irq_next

    ; vectors 48..255 <- generic ignore stub (spurious-safe)
    mov ebx, 48
.fill_rest:
    lea rax, [r15 + irq_spur - kmain]
    mov rdi, rbx
    shl rdi, 4
    add rdi, r11
    mov [rdi], ax
    shr rax, 16
    mov word [rdi+6], ax
    shr rax, 16
    mov [rdi+8], eax
    mov word [rdi+2], 0x18
    mov byte [rdi+5], 0x8E
    inc ebx
    cmp ebx, 256
    jb .fill_rest

    ; entry TIMER_VEC(48) = real LAPIC timer handler (overrides generic stub)
    lea rax, [r15 + irq_timer - kmain]
    mov rdi, (TIMER_VEC)*16
    add rdi, r11
    mov [rdi], ax
    shr rax, 16
    mov word [rdi+6], ax
    shr rax, 16
    mov [rdi+8], eax
    mov word [rdi+2], 0x18
    mov byte [rdi+5], 0x8E

    ; entry 49 (0x31) = voluntary yield software interrupt (int 0x31)
    lea rax, [r15 + isr_yield - kmain]
    mov rdi, 49*16
    add rdi, r11
    mov [rdi], ax
    shr rax, 16
    mov word [rdi+6], ax
    shr rax, 16
    mov [rdi+8], eax
    mov word [rdi+2], 0x18
    mov byte [rdi+5], 0x8E

    lidt tword [r15 + idtr - kmain]
    ret

; --- remap 8259 PIC: IRQ0-7 -> 0x20, IRQ8-15 -> 0x28; unmask IRQ0+IRQ1 ---
pic_init:
    mov al, 0x11              ; ICW1: init + cascade
    out 0x20, al
    out 0xA0, al
    mov al, 0x20              ; ICW2: offsets
    out 0x21, al
    mov al, 0x28
    out 0xA1, al
    mov al, 0x04              ; ICW3: slave on IRQ2
    out 0x21, al
    mov al, 0x02
    out 0xA1, al
    mov al, 0x01              ; ICW4: 8086 mode
    out 0x21, al
    out 0xA1, al
    mov al, 0xF8              ; unmask IRQ0 (timer), IRQ1 (kbd), IRQ2 (cascade)
    out 0x21, al
    mov al, 0xEF              ; unmask IRQ12 (mouse on slave PIC line 4)
    out 0xA1, al
    ret

; --- PIT channel 0: ~1000 Hz square wave ---
pit_init:
    mov al, 0x36
    out 0x43, al
    mov ax, 1193              ; 1193182 Hz / 1193 ~= 1000 Hz
    out 0x40, al
    mov al, ah
    out 0x40, al
    ret

; --- hardware IRQ common: EOI by source + dispatch ---
common_irq:
    ; Unified 15-GPR context save matching isr_yield and task_create
    xchg [rsp], rax           ; swap vector with rax: [rsp] = rax, rax = vector
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15                  ; all 15 GPRs now on stack
    mov r12, rax              ; save vector in r12

    ; always ack BOTH controllers - covers PIC, LAPIC-EXTINT and LAPIC paths
    mov al, 0x20
    out 0x20, al
    out 0xA0, al
    mov ecx, LAPIC_EOI
    xor eax, eax
    mov dword [ecx], eax

    cmp r12, TIMER_VEC
    je .timer
    cmp r12, 32               ; legacy PIT IRQ0 (pre-switch)
    je .timer
    cmp r12, 33               ; IRQ1 keyboard
    je .kbd
    cmp r12, 44               ; IRQ12 mouse (32 + 12 = 44)
    je .mouse
    jmp sched_restore

.timer:
    inc dword [r15 + timer_ticks - kmain]
    jmp sched_tick

.kbd:
    call kb_irq
    jmp sched_restore

.mouse:
    call mouse_irq
    jmp sched_restore

irq_spur:
    push 47                   ; dummy vector -> unknown path, just EOI
    jmp common_irq

irq_timer:
    push TIMER_VEC            ; LAPIC timer vector
    jmp common_irq

rept 16 n
{
    irq#n:
    push n+31                 ; vector = 32 + (n-1)
    jmp common_irq
}

align 8
irq_table:
rept 16 n { dq irq#n }

; --- Local APIC timer: periodic ~1kHz (ICR=6250, bus 100MHz, /16). ---
; NOTE: after LAPIC enable + LINT0 masked, the PIC cannot deliver anymore,
; so there is NO PIT-based calibration possible here. Fixed QEMU assumption;
; recalibrate via PIT one-shot + port polling on real HW later.
apic_init:
    mov eax, 1
    cpuid
    test edx, 1 shl 9         ; APIC present?
    jz .no_apic

    ; spurious register: vector 0xFF + enable bit8
    mov ecx, LAPIC_SPUR
    mov dword [ecx], 0x1FF

    ; mask ALL LVTs: silence EXTINT/LINT/error paths
    mov ecx, LAPIC_BASE + 0x330     ; thermal
    mov dword [ecx], 0x10000
    mov ecx, LAPIC_BASE + 0x340     ; performance
    mov dword [ecx], 0x10000
    mov ecx, LAPIC_BASE + 0x350     ; LINT0 = ExtINT mode: PIC passes through LAPIC
    mov dword [ecx], 0x00700
    mov ecx, LAPIC_BASE + 0x360     ; LINT1
    mov dword [ecx], 0x10000
    mov ecx, LAPIC_BASE + 0x370     ; error
    mov dword [ecx], 0x10000

    ; timer: /16, periodic, 6250 counts = 1ms @ 100MHz
    mov ecx, LAPIC_TDCR
    mov dword [ecx], 3                        ; divide by 16
    mov ecx, LAPIC_LVTTR
    mov dword [ecx], TIMER_VEC or 0x20000     ; vector | periodic(bit17)
    mov ecx, LAPIC_TICR
    mov dword [ecx], 6250

    ; mask PIT IRQ0: otherwise ExtINT reflood-storms on every EOI
    ; (LAPIC timer is the only tick source now)
    in  al, 0x21
    or  al, 1
    out 0x21, al

.skip:
.no_apic:
    ret

; --- physical memory manager ---
; bitmap: 1 bit per 4KB page, first 256MB; 1 = used, 0 = free
pmm_init:
    mov edi, PMM_BITMAP
    mov ecx, BITMAP_BYTES/4
    mov eax, -1
    rep stosd                 ; everything used by default

    ; walk e820: clear bits for usable (type=1) ranges
    mov ebx, [r14 + MEMMAP_COUNT]
    test ebx, ebx
    jz .head
    mov rsi, MEMMAP_BASE
.range:
    cmp dword [rsi+16], 1
    jne .next_e
    mov rax, [rsi]            ; base
    mov rcx, [rsi+8]          ; length
    add rcx, rax              ; end
    mov rdx, 0x10000000       ; clamp to 256MB
    cmp rcx, rdx
    jbe .clamped
    mov rcx, rdx
.clamped:
    add rax, 0xFFF            ; round base up to page
    and rax, -4096
    cmp rax, rcx
    jae .next_e
.page_loop:
    mov r8, rax
    shr r8, 12                ; page number
    cmp r8, BITMAP_BITS
    jae .next_e
    mov r9, r8
    shr r9, 5                 ; dword index
    and r8d, 31               ; bit index
    btr dword [r14 + PMM_BITMAP + r9*4], r8d
    add rax, 4096
    cmp rax, rcx
    jb .page_loop
.next_e:
    add rsi, 20
    dec ebx
    jnz .range

.head:
    ; re-reserve low pages [0, RESERVE_PAGES)
    mov ecx, RESERVE_PAGES
.setr:
    lea eax, [ecx-1]
    mov r8, rax
    shr r8, 5
    and eax, 31
    bts dword [r14 + PMM_BITMAP + r8*4], eax
    loop .setr

    ; re-reserve GUI/wallpaper pages [0x2000, 0x4600) (38MB @ 0x02000000)
    mov ecx, 9728
.set_bb:
    lea eax, [ecx + 0x1FFF]
    mov r8, rax
    shr r8, 5
    and eax, 31
    bts dword [r14 + PMM_BITMAP + r8*4], eax
    loop .set_bb
    ret

; RAX = phys addr of free page (0 = OOM)
pmm_alloc:
    xor ebx, ebx
.dw:
    mov eax, [r14 + PMM_BITMAP + rbx*4]
    not eax                   ; free bits set
    test eax, eax
    jz .next
    bsf ecx, eax              ; first free bit
    bts dword [r14 + PMM_BITMAP + rbx*4], ecx
    shl ebx, 5
    add ebx, ecx
    shl rbx, 12
    mov rax, rbx
    ret
.next:
    inc ebx
    cmp ebx, BITMAP_DWORDS
    jb .dw
    xor eax, eax
    ret

; RAX = zeroed free page phys (0 = OOM)
pmm_alloc_zero:
    call pmm_alloc
    test rax, rax
    jz .bad
    mov rdi, rax
    push rdi
    mov ecx, 4096/8
    xor eax, eax
    rep stosq
    pop rax
.bad:
    ret

; RDI = phys addr to release
pmm_free:
    shr rdi, 12
    mov ecx, edi
    shr ecx, 5
    and edi, 31
    btr dword [r14 + PMM_BITMAP + rcx*4], edi
    ret

; RDI = number of contiguous pages needed
; Returns: RAX = phys addr of first page (or 0 on OOM)
pmm_alloc_contiguous:
    push rbx
    push rcx
    push rdx
    push r8
    push r9
    push r10

    mov r8d, edi                      ; r8d = pages needed
    test r8d, r8d
    jz .fail

    mov ebx, RESERVE_PAGES            ; start after reserved pages
.search_start:
    cmp ebx, BITMAP_BITS
    jae .fail

    ; Check bit ebx
    mov eax, ebx
    shr eax, 5                        ; dword idx
    mov edx, ebx
    and edx, 31                       ; bit idx
    bt dword [r14 + PMM_BITMAP + rax*4], edx
    jc .next_start                    ; 1 = used

    ; Bit ebx is free! Check if next r8d-1 bits are also free
    mov ecx, 1
.check_span:
    cmp ecx, r8d
    jae .found_span

    lea eax, [ebx + ecx]
    cmp eax, BITMAP_BITS
    jae .fail

    mov r9, rax
    shr r9, 5
    and eax, 31
    bt dword [r14 + PMM_BITMAP + r9*4], eax
    jc .span_broken
    inc ecx
    jmp .check_span

.span_broken:
    lea ebx, [ebx + ecx + 1]
    jmp .search_start

.next_start:
    inc ebx
    jmp .search_start

.found_span:
    ; Mark all r8d bits as used
    xor ecx, ecx
.mark_span:
    lea eax, [ebx + ecx]
    mov r9, rax
    shr r9, 5
    and eax, 31
    bts dword [r14 + PMM_BITMAP + r9*4], eax
    inc ecx
    cmp ecx, r8d
    jb .mark_span

    ; Zero all allocated pages
    mov rax, rbx
    shl rax, 12                       ; phys address
    push rax
    mov rdi, rax
    mov ecx, r8d
    shl ecx, 12 - 3                   ; r8d * 4096 / 8 = r8d * 512 qwords
    xor eax, eax
    rep stosq
    pop rax
    jmp .done

.fail:
    xor eax, eax

.done:
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rbx
    ret

; RDI = phys addr, RSI = count
pmm_free_contiguous:
    push rax
    push rbx
    push rcx
    push rdi
    push rsi

    shr rdi, 12                       ; page index
    xor ecx, ecx
.free_loop:
    cmp ecx, esi
    jae .free_done
    lea eax, [edi + ecx]
    mov rbx, rax
    shr rbx, 5
    and eax, 31
    btr dword [r14 + PMM_BITMAP + rbx*4], eax
    inc ecx
    jmp .free_loop

.free_done:
    pop rsi
    pop rdi
    pop rcx
    pop rbx
    pop rax
    ret

; count free pages -> R8D
count_free:
    xor ebx, ebx
    xor r8d, r8d
.dw:
    mov eax, [r14 + PMM_BITMAP + rbx*4]
    not eax
    mov ecx, 32
.bits:
    shr eax, 1
    adc r8d, 0
    dec ecx
    jnz .bits
    inc ebx
    cmp ebx, BITMAP_DWORDS
    jb .dw
    ret

; --- VMM: map/unmap single 4KB pages, tables allocated on demand ---
; vmm_map: RDI=virt, RAX=phys, R11B=pte flags; preserves params
vmm_map:
    push rdi
    push rax
    push r11
    mov rdx, rdi
    shr rdx, 39
    and edx, 511              ; PML4 idx
    mov r8, rdi
    shr r8, 30
    and r8d, 511              ; PDPT idx
    mov r9, rdi
    shr r9, 21
    and r9d, 511              ; PD idx
    mov r10, rdi
    shr r10, 12
    and r10d, 511             ; PT idx
    mov rsi, 0x1000           ; PML4 (linear == phys)

.lvl_pdpt:
    mov rbx, [rsi + rdx*8]
    test rbx, rbx
    jnz .have_pdpt
    call pmm_alloc_zero
    or al, 3                  ; P|RW
    mov [rsi + rdx*8], rax
.have_pdpt:
    mov rsi, [rsi + rdx*8]
    and esi, 0xFFFFF000

.lvl_pd:
    mov rbx, [rsi + r8*8]
    test rbx, rbx
    jnz .have_pd
    call pmm_alloc_zero
    or al, 3
    mov [rsi + r8*8], rax
.have_pd:
    mov rsi, [rsi + r8*8]
    and esi, 0xFFFFF000

.lvl_pt:
    mov rbx, [rsi + r9*8]
    test rbx, rbx
    jnz .have_pt
    call pmm_alloc_zero
    or al, 3
    mov [rsi + r9*8], rax
.have_pt:
    mov rsi, [rsi + r9*8]
    and esi, 0xFFFFF000

    pop r11
    pop rax
    pop rdi
    and rax, -4096
    or rax, r11               ; final PTE
    mov [rsi + r10*8], rax
    ret

; vmm_unmap: RDI=virt (assumes fully populated path)
vmm_unmap:
    mov rdx, rdi
    shr rdx, 39
    and edx, 511
    mov r8, rdi
    shr r8, 30
    and r8d, 511
    mov r9, rdi
    shr r9, 21
    and r9d, 511
    mov r10, rdi
    shr r10, 12
    and r10d, 511
    mov rsi, 0x1000
    mov rsi, [rsi + rdx*8]
    and esi, 0xFFFFF000
    mov rsi, [rsi + r8*8]
    and esi, 0xFFFFF000
    mov rsi, [rsi + r9*8]
    and esi, 0xFFFFF000
    and qword [rsi + r10*8], 0
    mov rax, cr3
    mov cr3, rax              ; TLB flush
    ret

common_ex:
    mov rsi, exc_msg
    call puts
    mov al, [rsp]             ; vector number pushed by stub
    push rax
    shr al, 4
    call hexdigit
    call putc                 ; high digit
    pop rax
    and al, 0xF
    call hexdigit
    call putc                 ; low digit
    mov rsi, at_msg
    call puts
    mov rax, [rsp+8]          ; RIP from trap frame (was [rsp+16])
    call puthex64
    mov al, ' '
    call putc
    mov al, 'C'
    call putc
    mov al, 'R'
    call putc
    mov al, '2'
    call putc
    mov al, '='
    call putc
    mov rax, cr2
    call puthex64
    mov al, 10
    call putc
.halt:
    cli
    hlt
    jmp .halt

rept 32 n
{
    ex#n:
    push n-1
    jmp common_ex
}

align 8
ex_table:
rept 32 n { dq ex#n }

; --- compare ASCIIZ RSI vs RDI -> AL=1 equal ---
streq:
.loop:
    lodsb
    mov bl, [rdi]
    inc rdi
    cmp al, bl
    jne .neq
    test al, al
    jz .eq
    jmp .loop
.eq:
    mov al, 1
    ret
.neq:
    xor eax, eax
    ret

; --- AL low nibble -> ASCII in AL ---
hexdigit:
    and al, 0xF
    cmp al, 10
    jb .num
    add al, 'A'-10
    ret
.num:
    add al, '0'
    ret

; --- print RAX as 16 hex digits ---
puthex64:
    push rax
    push rcx
    mov ecx, 16
.l:
    rol rax, 4
    push rax
    call hexdigit
    call putc
    pop rax
    dec ecx
    jnz .l
    pop rcx
    pop rax
    ret

; --- print RAX in decimal ---
putdec64:
    push rax
    push rbx
    push rcx
    push rdx

    test rax, rax
    jnz .non_zero
    mov al, '0'
    call putc
    jmp .dec_done

.non_zero:
    xor ecx, ecx                      ; digit count
    mov rbx, 10
.div_loop:
    xor edx, edx
    div rbx                           ; rax = rax / 10, rdx = remainder
    push rdx                          ; push remainder
    inc ecx
    test rax, rax
    jnz .div_loop

.print_loop:
    pop rax
    add al, '0'
    call putc
    dec ecx
    jnz .print_loop

.dec_done:
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; --- print EAX as 8 hex digits ---
puthex32:
    push rax
    push rcx
    mov ecx, 8
.l32:
    rol eax, 4
    push rax
    call hexdigit
    call putc
    pop rax
    dec ecx
    jnz .l32
    pop rcx
    pop rax
    ret

; --- print AX as 4 hex digits ---
puthex16:
    push rax
    push rcx
    mov ecx, 4
.l16:
    rol ax, 4
    push rax
    call hexdigit
    call putc
    pop rax
    dec ecx
    jnz .l16
    pop rcx
    pop rax
    ret

; --- print AL as 2 hex digits ---
puthex8:
    push rax
    push rax
    shr al, 4
    call hexdigit
    call putc
    pop rax
    call hexdigit
    call putc
    pop rax
    ret

align 16
banner   db "Opensweet OS 0.0.2 [x86_64] higher-half - built with FASM", 10, 0
cpu_msg  db "CPU: ", 0
prompt   db "opensweet> ", 0
vendor   rb 16
cur      dw 0
shift    db 0
exc_msg  db "EXCEPTION ", 0
at_msg   db " @ ", 0
cmd_exc  db "exc", 0
cmd_div  db "div", 0
cmd_map  db "map", 0
cmd_help     db "help", 0
cmd_ls       db "ls", 0
cmd_cd       db "cd", 0
cmd_pwd      db "pwd", 0
cmd_cat      db "cat", 0
cmd_stat     db "stat", 0
cmd_wallpaper db "wallpaper", 0
cmd_mem      db "mem", 0
cmd_cpu      db "cpu", 0
cmd_pci      db "pci", 0
cmd_ticks    db "ticks", 0
cmd_uptime   db "uptime", 0
cmd_vbe      db "vbe", 0
cmd_fps      db "fps", 0
cmd_clear    db "clear", 0
cmd_cls      db "cls", 0
cmd_tasks    db "tasks", 0
cmd_ps       db "ps", 0
cmd_heap     db "heap", 0
cmd_heaptest db "heaptest", 0
cmd_reboot   db "reboot", 0
cmd_poweroff db "poweroff", 0
cmd_exit     db "exit", 0

str_lssp     db "ls ", 0
str_cdsp     db "cd ", 0
str_statsp   db "stat ", 0
str_wallpapersp db "wallpaper ", 0

str_wall_ok  db "Wallpaper decoded and applied successfully.", 10, 0
str_wall_err db "wallpaper: failed to load or decode PNG file.", 10, 0

str_fps_hdr    db "--- GUI Compositor Metrics ---", 10, 0
str_fps_lbl    db "  Framerate:    ", 0
str_fps_unit   db " FPS", 10, 0
str_fps_frames db "  Total Frames: ", 0
str_fps_engine db 10, "  Engine Mode:  1080p Sub-Region Dirty Compositor (Zero-Lag)", 10, 0

prompt_user  db "opensweet:", 0
prompt_sym   db "# ", 0

str_cmd_notfound1 db "opensweet: command not found: '", 0
str_cmd_notfound2 db "' (type 'help' for available commands)", 10, 0

str_help_hdr db "=== Opensweet OS Available Commands ===", 10, 0
str_help_body:
db "  heap            - Display kernel dynamic heap allocator stats", 10
db "  heaptest        - Run kernel heap allocator verification test", 10
db "  tasks / ps      - List active threads, state, ticks and stacks", 10
db "  fps             - Display GUI frame rate and compositor metrics", 10
db "  ls [path]       - List directory contents on ext4", 10
db "  cd [path]       - Change current working directory", 10
db "  pwd             - Print current working directory", 10
db "  cat <file>      - Print file contents from ext4", 10
db "  stat <file>     - Display inode and extent metadata", 10
db "  wallpaper [path]- Load and apply PNG wallpaper from ext4", 10
db "  mem             - Display physical memory (PMM) stats", 10
db "  cpu             - Display CPU vendor, brand and features", 10
db "  pci             - Scan and enumerate PCI bus devices", 10
db "  uptime / ticks  - Show timer ticks and system uptime", 10
db "  vbe             - Show VBE framebuffer mode details", 10
db "  clear           - Clear terminal screen", 10
db "  reboot          - Reboot system via 8042 reset", 10
db "  poweroff        - Power off virtual machine", 10
db 0

str_os_title    db "Opensweet OS v0.0.2 [x86_64 Long Mode]", 10, 0
str_os_subtitle db "SMP Kernel | ext4 Read-Only VFS | 1024x768 Framebuffer Console", 10, 0
str_os_ready    db "System initialized successfully. Type 'help' for available commands.", 10, 10, 0

str_cat_usage   db "Usage: cat <file>", 10, 0
str_cat_isdir   db "cat: is a directory", 10, 0

str_mem_hdr     db "--- Physical Memory Manager (PMM) ---", 10, 0
str_mem_total   db "  Total RAM:    256 MB (65536 physical pages)", 10, 0
str_mem_used    db "  Used Pages:   ", 0
str_mem_free    db "  Free Pages:   ", 0
str_mem_pages   db " pages", 10, 0
str_mem_pgsz    db "  Page Size:    4096 bytes (4 KB)", 10, 0

str_cpu_hdr     db "--- CPU Diagnostics (CPUID) ---", 10, 0
str_cpu_lbl_vendor db "  Vendor:       ", 0
str_cpu_lbl_model  db "  Model:        ", 0
str_cpu_lbl_feat   db "  Features:     ", 0
str_feat_apic   db "APIC ", 0
str_feat_tsc    db "TSC ", 0
str_feat_msr    db "MSR ", 0
str_feat_sse    db "SSE ", 0
str_feat_sse2   db "SSE2 ", 0
str_feat_sse3   db "SSE3 ", 0
str_feat_avx    db "AVX ", 0

str_pci_hdr     db "--- PCI Bus 0 Device Scan ---", 10, 0
str_pci_prefix  db "  [PCI] 00:", 0
str_pci_vend    db "  Vendor: 0x", 0
str_pci_dev     db "  Device: 0x", 0
str_pci_cls     db "  Class: 0x", 0
str_cls_bridge  db " (Host / PCI Bridge)", 0
str_cls_vga     db " (VGA Display Controller)", 0
str_cls_storage db " (Mass Storage / IDE Controller)", 0
str_cls_net     db " (Network Controller)", 0

str_uptime_lbl  db "System Uptime:  ", 0
str_uptime_ticks db " timer ticks", 10, 0

str_reboot_msg  db "Rebooting system...", 10, 0
str_poweroff_msg db "Powering off virtual machine...", 10, 0

cpu_brand_str   rb 64

cmd_ata   db "ata", 0
str_vbe_hdr   db "VBE Framebuffer: ", 0
str_vbe_at    db " @ ", 0
str_vbe_bpp   db " bpp, Pitch: ", 0
str_vbe_lfb   db " bytes, LFB: 0x", 0
cmd_gui   db "gui", 0
msg_gui_ok db "GUI rendered to VBE framebuffer (1024x768x32bpp WM desktop)", 10, 0
cmd_mouse db "mouse", 0
msg_mouse_x db "mouse: x=", 0
msg_mouse_y db " y=", 0
msg_mouse_btn db " btn=", 0
str_catsp db "cat ", 0
msg_freepages db "free_pages=", 0
msg_a     db " a=", 0
msg_b     db " b=", 0
msg_eq    db "EQ", 10, 0
msg_ne    db "NE", 10, 0
msg_mapval db "mapped_value=", 0
msg_oom   db "OOM", 10, 0
msg_atasec db "ata_sectors=", 0
msg_atadump db "sec0: ", 0
msg_ataerr db "ATA ERR", 10, 0
msg_fserr db "FS ERR", 10, 0

; --- prefix compare: RSI vs RDI, RCX bytes -> AL=1 equal ---
strpref:
    push rsi
    push rdi
    push rcx
    repe cmpsb
    setz al
    pop rcx
    pop rdi
    pop rsi
    ret
kb_buf   rb 32
kb_head  db 0
kb_tail  db 0
timer_ticks dd 0
ticks_msg db "ticks=", 0
cmd_len  db 0
cmd_buf  rb 128

align 16
idt      rb 256*16
idtr     dw 4095
         dq idt

KEYMAP_SIZE = 0x54
keymap_norm:
db 0,27,"1234567890-=",8,9
db "qwertyuiop[]",10,0
db "asdfghjkl;'",96,0,92
db "zxcvbnm,./",0,0,0," "
rb 0x53-$+keymap_norm
keymap_shift:
db 0,27,"!@#$%^&*()_+",8,9
db "QWERTYUIOP{}",10,0
db 'ASDFGHJKL:"',126,0,"|"
db "ZXCVBNM<>?",0,0,0," "
rb 0x53-$+keymap_shift

include '..\drivers\ata.asm'
include '..\drivers\mouse.asm'
include 'D:\Opensweet\fs\ext4\ext4.inc'
include '..\drivers\console.asm'
include 'D:\Opensweet\gui\modern_desktop.inc'
include 'D:\Opensweet\kernel\sched.inc'
include 'D:\Opensweet\kernel\heap.inc'

; ================= framebuffer test pattern (proves VBE LFB works) =================
; fills screen with per-pixel gradient: R=x, G=y, B=(x+y) & 255
fb_test_pattern:
    cmp byte [r14 + VBS_OK], 1
    jne .ret
    ; load params
    movzx eax, word [r14 + VBS_PITCH]
    mov [r15 + vbe_pitch - kmain], eax
    movzx eax, word [r14 + VBS_WIDTH]
    mov [r15 + vbe_width - kmain], eax
    movzx eax, word [r14 + VBS_HEIGHT]
    mov [r15 + vbe_height - kmain], eax
    mov eax, dword [r14 + VBS_LFB]
    mov [r15 + vbe_lfb - kmain], eax

    xor r8d, r8d                     ; y = 0
.yloop:
    mov r9d, 0                       ; x = 0
.xloop:
    mov eax, r9d
    and eax, 0xFF                    ; R = x
    shl eax, 16
    mov ecx, r8d
    and ecx, 0xFF
    mov edx, ecx
    shl edx, 8                       ; G = y
    or eax, edx
    add ecx, r9d                     ; B = (x+y)
    and ecx, 0xFF
    or eax, ecx
    mov edi, [r15 + vbe_lfb - kmain]
    mov ecx, [r15 + vbe_pitch - kmain]
    imul ecx, r8d
    add edi, ecx
    lea rdi, [rdi + r9*4]
    mov [rdi], eax
    inc r9d
    cmp r9d, [r15 + vbe_width - kmain]
    jb .xloop
    inc r8d
    cmp r8d, [r15 + vbe_height - kmain]
    jb .yloop
.ret:
    ret

align 16
vbe_lfb    dd 0
vbe_pitch  dd 0
vbe_width  dd 0
vbe_height dd 0
