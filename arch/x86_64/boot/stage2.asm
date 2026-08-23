; Opensweet OS - stage 2: kernel load (LBA->CHS), e820, VBE graphics, enter long mode.
; Loaded by MBR at 0x0600. IDE geometry: 63 spt x 16 heads.

KERNEL_LBA     = 33                  ; LBA0=MBR, LBA1..32=stage2(16KB)
KERNEL_ADDR    = 0x10000
KERNEL_SECTORS = 512                 ; 256KB kernel window

org 0x0600
use16

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0xFF00
    sti

    ; --- init COM1 38400 8N1 ---
    mov dx, 0x3F9
    mov al, 0
    out dx, al
    mov dx, 0x3FB
    mov al, 0x80
    out dx, al
    mov dx, 0x3F8
    mov al, 3
    out dx, al
    inc dx
    xor al, al
    out dx, al
    mov dx, 0x3FB
    mov al, 3
    out dx, al
    mov dx, 0x3FA
    mov al, 0xC7
    out dx, al
    mov dx, 0x3FC
    mov al, 0x0B
    out dx, al

    mov al, 'S'
    call rs_putc

    mov [boot_drive], dl

    ; --- load kernel: LBA->CHS per IDE geometry ---
    mov ax, KERNEL_LOAD_SEG
    mov es, ax
    xor bx, bx
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
    mov al, 'K'
    call rs_putc

    ; --- e820 memory map -> count dword @0x6000, entries @0x6100 ---
    xor ax, ax
    mov es, ax
    mov dword [0x6000], 0
    mov di, 0x6100
    xor ebx, ebx
    xor bp, bp
    mov edx, 0x534D4150
.e820_loop:
    mov eax, 0xE820
    mov ecx, 20
    int 0x15
    jc .e820_done
    cmp eax, 0x534D4150
    jne .e820_done
    inc bp
    add di, 20
    cmp di, 0x7E00
    jae .e820_done
    test bx, bx
    jnz .e820_loop
.e820_done:
    mov [0x6000], bp

    ; --- VBE: pick 32bpp LFB mode (>=640x400) ---
    mov byte [0x50FE], 0
    xor ax, ax
    mov es, ax
    mov di, 0x5000
    mov ax, 0x4F00
    int 0x10
    cmp ax, 0x004F
    jne .vbe_none
    cmp dword [di], 'ASEV'
    jne .vbe_none

    mov si, [di + 0x0E]
    mov dx, [di + 0x10]
    mov gs, dx
    mov cx, 256
.vbe_mode_loop:
    mov bx, [gs:si]
    cmp bx, 0xFFFF
    je .vbe_none
    push es
    xor ax, ax
    mov es, ax
    mov di, 0x5200
    mov cx, bx
    mov ax, 0x4F01
    int 0x10
    pop es
    cmp ax, 0x004F
    jne .vbe_next
    mov ax, [es:0x5200]
    and ax, 0x0091
    cmp ax, 0x0091
    jne .vbe_next
    movzx eax, word [es:0x5212]
    cmp eax, 640
    jb .vbe_next
    movzx eax, word [es:0x5214]
    cmp eax, 400
    jb .vbe_next
    cmp byte [es:0x521A], 32
    jne .vbe_next

    mov ax, [es:0x5210]
    mov [0x5010], ax
    mov ax, [es:0x5212]
    mov [0x5012], ax
    mov ax, [es:0x5214]
    mov [0x5014], ax
    mov al, [es:0x521A]
    mov [0x501A], al
    mov eax, [es:0x5228]
    mov [0x5028], eax
    mov [0x5030], bx
    jmp .vbe_found
.vbe_next:
    add si, 2
    dec cx
    jnz .vbe_mode_loop
    jmp .vbe_none
.vbe_found:
    mov bx, [0x5030]
    or bx, 0x4000
    mov ax, 0x4F02
    int 0x10
    cmp ax, 0x004F
    jne .vbe_none
    mov byte [0x50FE], 1
    mov al, 'V'
    call rs_putc
.vbe_none:
.vbe_done:

    ; --- enable A20 ---
    in al, 0x92
    or al, 2
    out 0x92, al

    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE32_SEL:pm_entry

disk_error:
    mov al, 'D'
    call rs_putc
.halt:
    hlt
    jmp .halt

use32
pm_entry:
    mov ax, DATA_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0xFF00

    ; page tables: PML4@0x1000, PDPT@0x2000, PD@0x3000, PDPT_K@0x4000? (see below)
    mov edi, 0x1000
    xor eax, eax
    mov ecx, 0xC00
    rep stosd

    mov dword [0x1000], 0x2001          ; PML4[0] -> PDPT
    mov dword [0x2000], 0x3001          ; PDPT[0] -> PD
    mov dword [0x2018], 0xC0000087      ; PDPT[3]: 1GB @3GB (LAPIC/VBE LFB)

    ; higher-half: PML4[256] -> PDPT_B@0x4000 -> PD_B@0x5000 -> 2MB @ phys 0
    mov dword [0x1800], 0x4001
    mov dword [0x4000], 0x5001
    mov dword [0x5000], 0x83

    mov edi, 0x3000
    mov eax, 0x83
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
    or eax, 1 shl 5                     ; PAE
    or eax, 1 shl 10                    ; OSFXSR (SSE for C code)
    mov cr4, eax

    mov ecx, 0xC0000080                 ; EFER
    rdmsr
    or eax, 1 shl 8                     ; LME
    wrmsr

    mov eax, cr0
    or eax, 0x80000000                  ; PG
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
    mov rsp, 0xFF000
    mov rax, KERNEL_ADDR
    jmp rax

align 16
gdt_start:
    dq 0
CODE32_SEL = $ - gdt_start
    dw 0xFFFF, 0x0000
    db 0x00, 0x9A, 0xCF, 0x00
DATA_SEL = $ - gdt_start
    dw 0xFFFF, 0x0000
    db 0x00, 0x92, 0xCF, 0x00
CODE64_SEL = $ - gdt_start
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

; --- serial out (AL = char) ---
rs_putc:
    push dx
    push ax
.wait:
    mov dx, 0x3FD
    in al, dx
    test al, 0x20
    jz .wait
    pop ax
    mov dx, 0x3F8
    out dx, al
    pop dx
    ret
