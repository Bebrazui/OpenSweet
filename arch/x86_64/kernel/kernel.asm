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
PMM_BITMAP   = 0x20000            ; 8KB bitmap, identity-mapped low RAM
BITMAP_BITS  = 65536              ; 256MB / 4KB
BITMAP_DWORDS = BITMAP_BITS / 32
BITMAP_BYTES = BITMAP_BITS / 8
RESERVE_PAGES = 0x23               ; low boot structures + kernel + bitmap

; --- VMM test target ---
TEST_VIRT    = 0x6000000000

; --- VBE framebuffer (slots filled by boot @0x5010..) ---
VBS_PITCH  = 0x5010
VBS_WIDTH  = 0x5012
VBS_HEIGHT = 0x5014
VBS_BPP    = 0x501A
VBS_LFB    = 0x5028
VBS_OK     = 0x50FE

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
    call ata_init
    call ext4_mount
    call fb_test_pattern
    call ext4_inode_load_auto
    call ext4_ls_print

    mov rsi, banner
    call puts

    ; --- CPU vendor via CPUID ---
    xor eax, eax
    cpuid
    mov dword [r15 + vendor - kmain], ebx
    mov dword [r15 + vendor - kmain+4], edx
    mov dword [r15 + vendor - kmain+8], ecx
    mov byte [r15 + vendor - kmain+12], 0
    mov rsi, cpu_msg
    call puts
    mov rsi, vendor
    call puts
    mov al, 10
    call putc

    mov rsi, prompt
    call puts

.shell:
    call getc
    test al, al
    jnz .key
    hlt                       ; sleep until IRQ
    jmp .shell
.key:
    cmp al, 10
    je .enter
    cmp al, 8
    je .bs
    movzx ecx, byte [r15 + cmd_len - kmain]
    cmp ecx, 63
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
    mov rsi, cmd_buf
    mov rdi, cmd_exc
    call streq
    test al, al
    jnz .do_exc
    mov rsi, cmd_buf
    mov rdi, cmd_div
    call streq
    test al, al
    jnz .do_div
    mov rsi, cmd_buf
    mov rdi, cmd_ticks
    call streq
    test al, al
    jnz .do_ticks
    mov rsi, cmd_buf
    mov rdi, cmd_mem
    call streq
    test al, al
    jnz .do_mem
    mov rsi, cmd_buf
    mov rdi, cmd_map
    call streq
    test al, al
    jnz .do_map
    mov rsi, cmd_buf
    mov rdi, cmd_ata
    call streq
    test al, al
    jnz .do_ata
    mov rdi, cmd_ls
    call streq
    test al, al
    jnz .do_ls
    ; prefix command "cat <path>"
    mov rsi, cmd_buf
    lea rdi, [r15 + str_catsp - kmain]
    mov ecx, 4
    call strpref
    test al, al
    jnz .do_cat
    jmp .prompt
.do_exc:
    db 0xCC                   ; int3 -> #BP (vector 3)
.do_div:
    xor ecx, ecx
    xor edx, edx
    div ecx                   ; -> #DE (vector 0)
.do_ticks:
    mov rsi, ticks_msg
    call puts
    mov eax, [r15 + timer_ticks - kmain]
    call puthex64
    mov al, 10
    call putc
    jmp .prompt
.do_mem:
    call count_free           ; r8d = free pages
    mov rsi, msg_freepages
    call puts
    mov eax, r8d
    call puthex64
    mov al, 10
    call putc
    ; alloc -> free -> alloc must return same page
    call pmm_alloc
    mov r8, rax
    mov rdi, rax
    call pmm_free
    call pmm_alloc
    mov r9, rax
    mov rsi, msg_a
    call puts
    mov rax, r8
    call puthex64
    mov rsi, msg_b
    call puts
    mov rax, r9
    call puthex64
    cmp r8, r9
    je .mem_eq
    mov rsi, msg_ne
    jmp .mem_prn
.mem_eq:
    mov rsi, msg_eq
.mem_prn:
    call puts
    jmp .prompt
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
    ; sectors count of data drive
    mov rsi, msg_atasec
    call puts
    mov eax, dword [r15 + ata_sectors - kmain]
    call puthex64
    mov al, 10
    call putc
    ; read sector 0 into ata_buf and dump first 16 bytes
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
.do_ls:
    cmp byte [r15 + ext4_ok - kmain], 0
    je .fs_err
    mov eax, 2                    ; root inode
    call ext4_inode_load
    jc .fs_err
    call ext4_ls_print
    mov al, 10
    call putc
    jmp .prompt
.do_cat:
    cmp byte [r15 + ext4_ok - kmain], 0
    je .fs_err
    lea rsi, [r15 + cmd_buf - kmain]
    add rsi, 4                    ; skip "cat "
    lea rdi, [r15 + e4t_path - kmain]
    mov byte [rdi], '/'
    inc rdi
.cat_cp:
    lodsb
    test al, al
    jz .cat_cpd
    stosb
    jmp .cat_cp
.cat_cpd:
    mov byte [rdi], 0
    lea rsi, [r15 + e4t_path - kmain]
    call ext4_lookup
    test eax, eax
    jz .fs_err
    call ext4_inode_load
    jc .fs_err
    call ext4_cat_print
    mov al, 10
    call putc
    jmp .prompt
.fs_err:
    mov rsi, msg_fserr
    call puts
    jmp .prompt

.prompt:
    mov rsi, prompt
    call puts
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

; --- print char AL to serial + VGA ---
putc:
    push rax
    push rcx
    push rdx
    push rdi
    push rsi

    ; serial
    mov rsi, rax
.wait:
    mov dx, COM1+5
    in  al, dx
    test al, 0x20
    jz  .wait
    mov dx, COM1
    mov al, sil
    out dx, al
    mov rax, rsi

    ; VGA
    cmp al, 10
    je  .nl
    cmp al, 13
    je  .out
    cmp al, 8
    je  .bs
    movzx edx, word [r15 + cur - kmain]
    mov rdi, VGA_BASE
    lea rdi, [rdi + rdx*2]
    mov [rdi], al
    mov byte [rdi+1], 0x07
    inc word [r15 + cur - kmain]
    cmp word [r15 + cur - kmain], COLS*ROWS
    jb  .out
    call scroll
    jmp .out
.nl:
    movzx eax, word [r15 + cur - kmain]
    xor edx, edx
    mov ecx, COLS
    div rcx                   ; rax=row, rdx=col
    sub rax, rdx
    add rax, COLS             ; next row start
    cmp rax, COLS*ROWS
    jb  .set_cur
    call scroll
    mov rax, COLS*(ROWS-1)
.set_cur:
    mov [r15 + cur - kmain], ax
    jmp .out
.bs:
    cmp word [r15 + cur - kmain], 0
    je  .out
    dec word [r15 + cur - kmain]
    movzx edx, word [r15 + cur - kmain]
    mov rdi, VGA_BASE
    lea rdi, [rdi + rdx*2]
    mov word [rdi], 0x0720
.out:
    pop rsi
    pop rdi
    pop rdx
    pop rcx
    pop rax
    ret

scroll:
    push rsi
    push rdi
    push rcx
    push rax
    mov rsi, VGA_BASE + COLS*2
    mov rdi, VGA_BASE
    mov ecx, (ROWS-1)*COLS
    rep movsw
    mov ecx, COLS
    mov ax, 0x0720
    rep stosw
    pop rax
    pop rcx
    pop rdi
    pop rsi
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
    mov al, 0xFC              ; mask all but timer+kbd
    out 0x21, al
    mov al, 0xFF
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
    ; full context save: we run on an arbitrary interrupted thread
    push rax
    push rcx
    push rdx
    push rbx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11                  ; vector now at [rsp + 10*8]

    ; always ack BOTH controllers - covers PIC, LAPIC-EXTINT and LAPIC paths
    mov al, 0x20
    out 0x20, al
    mov ecx, LAPIC_EOI
    xor eax, eax
    mov dword [ecx], eax

    mov rax, [rsp+80]
    cmp rax, TIMER_VEC
    je .timer
    cmp rax, 32               ; legacy PIT IRQ0 (pre-switch)
    je .timer
    cmp rax, 33               ; IRQ1 keyboard
    je .kbd
    jmp .ret
.timer:
    inc dword [r15 + timer_ticks - kmain]
    jmp .ret
.kbd:
    call kb_irq
.ret:
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbx
    pop rdx
    pop rcx
    pop rax
    add rsp, 8                ; drop pushed vector -> rsp points at RIP
    iretq

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
    mov rax, [rsp+8]          ; RIP from trap frame
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
cmd_ticks db "ticks", 0
cmd_mem   db "mem", 0
cmd_map   db "map", 0
cmd_ata   db "ata", 0
cmd_ls    db "ls", 0
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
cmd_buf  rb 64

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
include 'D:\Opensweet\fs\ext4\ext4.inc'

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
