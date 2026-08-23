; Opensweet OS - x86_64 stage 1: MBR bootloader
; Loads kernel to 0x10000, A20 on, protected mode -> long mode -> jump to kernel.

KERNEL_ADDR    = 0x10000
KERNEL_SECTORS = 32

org 0x7C00
use16

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    mov [boot_drive], dl

    ; --- load kernel: multi-track CHS read (1 sector per int13 call) ---
    mov ax, KERNEL_LOAD_SEG
    mov es, ax
    xor bx, bx               ; es:bx = 0x1000:0000
    mov si, KERNEL_SECTORS   ; remaining sectors
    mov byte [cur_cyl], 0
    mov byte [cur_head], 0
    mov byte [cur_sec], 2
.read_loop:
    mov ah, 0x02
    mov al, 1
    mov ch, [cur_cyl]
    mov cl, [cur_sec]
    mov dh, [cur_head]
    mov dl, [boot_drive]
    int 0x13
    jc  disk_error
    mov ax, es
    add ax, 0x20             ; next 512-byte paragraph
    mov es, ax

    ; advance CHS: 18 sectors/head, 2 heads
    inc byte [cur_sec]
    cmp byte [cur_sec], 18
    jbe .no_track
    mov byte [cur_sec], 1
    inc byte [cur_head]
    cmp byte [cur_head], 2
    jb .no_track
    mov byte [cur_head], 0
    inc byte [cur_cyl]
.no_track:
    dec si
    jnz .read_loop

    ; --- e820 memory map -> entries at 0x6000 (20 bytes each), count dword @0x5FFC ---
    xor ax, ax
    mov es, ax                ; ES got advanced by the disk reads!
    mov word [0x5FFC], 0
    mov di, 0x6000
    xor ebx, ebx
    xor bp, bp
    mov edx, 0x534D4150      ; 'PAMS'
.e820:
    mov eax, 0xE820
    mov ecx, 20
    int 0x15
    jc .e820_done
    cmp eax, 0x534D4150
    jne .e820_done
    inc bp
    add di, 20
    cmp di, 0x6E00
    jae .e820_done
    test bx, bx
    jnz .e820
.e820_done:
    mov [0x5FFC], bp
    mov word [0x5FFE], 0      ; full dword count

    ; --- enable A20 (fast gate) ---
    in  al, 0x92
    or  al, 2
    out 0x92, al

    lgdt [gdt_descriptor]

    ; --- protected mode ---
    mov eax, cr0
    or  eax, 1
    mov cr0, eax
    jmp CODE32_SEL:pm_entry

disk_error:
    mov si, err_msg
.err:
    lodsb
    test al, al
    jz .h
    mov ah, 0x0E
    int 0x10
    jmp .err
.h:
    hlt
    jmp .h

use32
pm_entry:
    mov ax, DATA_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x7C00

    ; --- page tables: PML4@0x1000, PDPT@0x2000, PD@0x3000 (identity 1GB, 2MB pages),
    ;     PDPT[3] -> 1GB page for [3GB,4GB) so LAPIC MMIO @0xFEE00000 is mapped ---
    mov edi, 0x1000
    xor eax, eax
    mov ecx, 0xC00          ; clear 12KB
    rep stosd

    mov dword [0x1000], 0x2001   ; PML4[0] -> PDPT
    mov dword [0x2000], 0x3001   ; PDPT[0] -> PD
    mov dword [0x2018], 0xC0000087  ; PDPT[3]: 1GB page @ 3GB, P|RW|PS

    mov edi, 0x3000
    mov eax, 0x83                ; P|RW|PS, phys 0
    mov ecx, 512
.fill_pd:
    mov [edi], eax
    mov dword [edi+4], 0
    add eax, 0x200000
    add edi, 8
    loop .fill_pd

    mov eax, 0x1000
    mov cr3, eax
    mov eax, cr4
    or  eax, 1 shl 5             ; PAE
    mov cr4, eax

    mov ecx, 0xC0000080          ; EFER
    rdmsr
    or  eax, 1 shl 8             ; LME
    wrmsr

    mov eax, cr0
    or  eax, 0x80000000          ; PG
    mov cr0, eax

    jmp CODE64_SEL:lm_entry

use64
lm_entry:
    mov ax, DATA_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, 0x90000
    mov rax, KERNEL_ADDR
    jmp rax

align 8
gdt_start:
    dq 0                          ; null
CODE32_SEL = $ - gdt_start         ; 0x08
    dw 0xFFFF, 0x0000
    db 0x00, 0x9A, 0xCF, 0x00
DATA_SEL = $ - gdt_start           ; 0x10
    dw 0xFFFF, 0x0000
    db 0x00, 0x92, 0xCF, 0x00
CODE64_SEL = $ - gdt_start         ; 0x18
    dw 0xFFFF, 0x0000
    db 0x00, 0x9A, 0xAF, 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

boot_drive db 0
cur_cyl    db 0
cur_head   db 0
cur_sec    db 0
KERNEL_LOAD_SEG = 0x1000
err_msg db "DISK ERR", 0
loading_msg db "OK", 0

rb 510-($-$$)
dw 0xAA55
