; Opensweet OS - x86_64 stage 1: MBR bootloader
; Loads kernel to 0x10000, A20 on, protected mode -> long mode -> jump to kernel.

KERNEL_LBA     = 33                  ; LBA0=MBR, LBA1..32=stage2(16KB)
KERNEL_ADDR    = 0x10000
KERNEL_SECTORS = 512                 ; 256KB window

org 0x0600
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

    ; --- load kernel: LBA->CHS per IDE geometry (63 spt, 16 heads) ---
    mov ax, KERNEL_LOAD_SEG
    mov es, ax
    mov word [cur_lba], KERNEL_LBA
    mov si, KERNEL_SECTORS
.read_loop:
    mov ax, [cur_lba]
    xor dx, dx
    mov cx, 63
    div cx                   ; ax = t, dx = sector-1
    inc dx
    push dx
    xor dx, dx
    mov cx, 16
    div cx                   ; ax = cylinder, dx = head
    mov [cur_head], dl
    pop dx
    mov [cur_cyl], ax
    mov cl, dl               ; sector
    mov ch, al               ; cylinder low
    mov dh, [cur_head]
    mov dl, [boot_drive]
    mov ah, 0x02             ; BIOS: read sectors
    mov al, 1
    int 0x13
    jc  disk_error
    mov ax, es
    add ax, 0x20
    mov es, ax
    inc word [cur_lba]
    dec si
    jnz .read_loop

    ; --- e820 memory map -> count dword @0x6000, entries @0x6100 (20 bytes each) ---
    xor ax, ax
    mov es, ax                ; ES got advanced by the disk reads!
    mov dword [0x6000], 0
    mov di, 0x6100
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
    cmp di, 0x6F00
    jae .e820_done
    test bx, bx
    jnz .e820
.e820_done:
    mov [0x6000], bp

    ; --- VBE: pick 32bpp LFB graphics mode (>=640x400), set it ---
    ; slots: 0x5010 pitch, 0x5012 width, 0x5014 height, 0x501A bpp,
    ;        0x5028 LFB phys, 0x5030 mode num, 0x50FE vbe_ok
    xor ax, ax
    mov es, ax
    mov di, 0x5000
    mov byte [0x50FE], 0
    mov ax, 0x4F00                   ; Get VBE controller info
    int 0x10
    cmp ax, 0x004F
    jne .vbe_none
    cmp dword [di], 'ASEV'           ; 'VESA'
    jne .vbe_none

    mov si, [di + 0x0E]              ; video mode list offset
    mov dx, [di + 0x10]              ; video mode list segment
    mov gs, dx                       ; GS:SI -> mode list
    mov cx, 256                      ; safety cap
.vbe_mode_loop:
    mov bx, [gs:si]                  ; next mode number
    cmp bx, 0xFFFF                   ; terminator
    je .vbe_none
    push es
    xor ax, ax
    mov es, ax
    mov di, 0x5200                   ; ModeInfoBlock buffer
    mov cx, bx                       ; requested mode
    mov ax, 0x4F01
    int 0x10
    pop es
    cmp ax, 0x004F
    jne .vbe_next_mode
    mov ax, [es:0x5200]              ; mode attributes
    and ax, 0x0091                   ; bit0 supported | bit4 graphics | bit7 LFB
    cmp ax, 0x0091
    jne .vbe_next_mode
    movzx eax, word [es:0x5212]      ; X resolution
    cmp eax, 640
    jb .vbe_next_mode
    movzx eax, word [es:0x5214]      ; Y resolution
    cmp eax, 400
    jb .vbe_next_mode
    cmp byte [es:0x521A], 32         ; bits per pixel
    jne .vbe_next_mode

    ; chosen: copy key fields to fixed slots
    mov ax, [es:0x5210]              ; bytes per scanline
    mov [0x5010], ax
    mov ax, [es:0x5212]
    mov [0x5012], ax                 ; width
    mov ax, [es:0x5214]
    mov [0x5014], ax                 ; height
    mov al, [es:0x521A]
    mov [0x501A], al                 ; bpp
    mov eax, [es:0x5228]             ; LFB physical address
    mov [0x5028], eax
    mov [0x5030], bx                 ; mode number
    jmp .vbe_mode_found
.vbe_next_mode:
    add si, 2
    dec cx
    jnz .vbe_mode_loop
    jmp .vbe_none
.vbe_mode_found:
    mov bx, [0x5030]
    or  bx, 0x4000                   ; linear framebuffer flag
    mov ax, 0x4F02                   ; Set VBE mode
    int 0x10
    cmp ax, 0x004F
    jne .vbe_none
    mov byte [0x50FE], 1             ; vbe_ok
.vbe_none:
.vbe_done:
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
    ;     PDPT[3] -> 1GB page for [3GB,4GB) so LAPIC MMIO @0xFEE00000 is mapped,
    ;     PML4[256] -> kernel at 0xFFFF800000000000+ (higher half, 2MB page @ phys 0) ---
    mov edi, 0x1000
    xor eax, eax
    mov ecx, 0x1400          ; clear 20KB: 0x1000..0x5FFF (memmap @0x6000 untouched!)
    rep stosd

    mov dword [0x1000], 0x2001   ; PML4[0] -> PDPT
    mov dword [0x2000], 0x3001   ; PDPT[0] -> PD
    mov dword [0x2018], 0xC0000087  ; PDPT[3]: 1GB page @ 3GB, P|RW|PS

    mov dword [0x1000+256*8], 0x4001   ; PML4[256] -> PDPT_K @0x4000
    mov dword [0x4000], 0x5001         ; PDPT_K[0] -> PD_K @0x5000
    mov dword [0x5000], 0x83           ; PD_K[0]: 2MB @ phys 0

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
cur_lba    dw 0
cur_cyl    dw 0
cur_head   db 0
cur_sec    db 0
KERNEL_LOAD_SEG = 0x1000
err_msg db "DISK ERR", 0
loading_msg db "OK", 0


