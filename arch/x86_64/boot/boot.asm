; Opensweet OS - stage 1 (MBR): load stage2 (32 sectors from LBA 1) to 0x0600, jump.
; IDE geometry: 63 spt x 16 heads (QEMU default).

STAGE_SECTORS equ 32

org 0x7C00
use16

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0xFF00
    sti
    mov [boot_drive], dl            ; BIOS passes boot drive in DL

    ; Check if LBA extensions are supported (INT 13h AH=41h)
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]
    int 0x13
    jc .chs_fallback
    cmp bx, 0xAA55
    jne .chs_fallback
    test cl, 1                   ; Packet calls supported?
    jz .chs_fallback

    ; Fast LBA DAP read (loads all 32 sectors of stage2 in 1 call!)
    mov dl, [boot_drive]
    mov si, dap
    mov ah, 0x42
    int 0x13
    jnc .stage2_loaded

.chs_fallback:
    mov ax, 0                       ; buffer segment
    mov es, ax
    mov bx, 0x0600                  ; buffer offset -> stage2 home
    mov si, STAGE_SECTORS
    mov word [cur_lba], 1

.read_loop:
    mov ax, [cur_lba]
    xor dx, dx
    mov cx, 63
    div cx                          ; ax = t, dx = sector-1
    inc dx
    push dx
    xor dx, dx
    mov cx, 16
    div cx                          ; ax = cylinder, dx = head
    mov [cur_head], dl
    pop dx
    mov [cur_cyl], ax
    mov cl, dl                      ; sector
    mov ch, al                      ; cylinder low
    mov dh, [cur_head]              ; head
    mov dl, [boot_drive]
    mov ah, 0x02                    ; BIOS: read sectors
    mov al, 1
    int 0x13
    jc  die
    add bx, 512                     ; next destination paragraph
    inc word [cur_lba]
    dec si
    jnz .read_loop

.stage2_loaded:
    mov al, '1'                     ; DEBUG: stage2 loaded
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10

    mov dl, [boot_drive]
    jmp 0:0x0600                    ; hand off to stage2

die:
    mov al, 'D'
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    hlt
    jmp die

align 4
dap:
    db 0x10, 0
    dw STAGE_SECTORS
    dw 0x0600
    dw 0x0000
    dd 1
    dd 0

cur_lba    dw 0
cur_cyl    dw 0
cur_head   db 0
boot_drive db 0

rb 510-($-$$)
dw 0xAA55
