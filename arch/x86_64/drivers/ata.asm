; Opensweet OS - ATA PIO driver (primary channel, LBA28, polling)
; Included from kernel.asm; runs in long mode, uses R15 (image base).
; Conventions: all routines may clobber rax,rcx,rdx; callers preserve what they need.

ATA_DATA   = 0x1F0
ATA_ERRFL  = 0x1F1
ATA_SCNT   = 0x1F2
ATA_LBAL   = 0x1F3
ATA_LBAM   = 0x1F4
ATA_LBAH   = 0x1F5
ATA_DRV    = 0x1F6
ATA_STATC  = 0x1F7              ; read = status, write = command

ATA_CMD_IDENTIFY = 0xEC
ATA_CMD_READ     = 0x20

STA_BSY = 0x80
STA_DRQ = 0x08
STA_ERR = 0x01

SEL_MASTER = 0xE0              ; |LBA bit
SEL_SLAVE  = 0xF0              ; |LBA bit

ATA_TIMEOUT = 0x100000

; --- wait BSY=0; CF=1 timeout ---
ata_wait_ready:
    push rcx
    push rdx
    mov ecx, ATA_TIMEOUT
    mov dx, ATA_STATC
.awr:
    in al, dx
    test al, STA_BSY
    jz .done
    dec ecx
    jnz .awr
    stc
    pop rdx
    pop rcx
    ret
.done:
    clc
    pop rdx
    pop rcx
    ret

; --- wait DRQ=1 (BSY must be 0); CF=1 timeout/error ---
ata_wait_drq:
    push rcx
    push rdx
    mov ecx, ATA_TIMEOUT
    mov dx, ATA_STATC
.awd:
    in al, dx
    test al, STA_ERR
    jnz .bad
    test al, STA_BSY
    jnz .next
    test al, STA_DRQ
    jnz .ok
.next:
    dec ecx
    jnz .awd
.bad:
    stc
    pop rdx
    pop rcx
    ret
.ok:
    clc
    pop rdx
    pop rcx
    ret

; --- select drive: AL = SEL_MASTER/SEL_SLAVE ---
ata_select:
    push rcx
    push rdx
    mov dx, ATA_DRV
    out dx, al
    mov ecx, 4               ; 400ns settle
    mov dx, ATA_STATC
.asd:
    in al, dx
    dec ecx
    jnz .asd
    call ata_wait_ready
    pop rdx
    pop rcx
    ret

; --- identify: AL = select byte; CF=0 ok -> sectors in EAX ---
ata_identify:
    push rbx
    mov bl, al               ; save select
    call ata_select
    jc .faild
    mov al, ATA_CMD_IDENTIFY
    mov dx, ATA_STATC
    out dx, al
    call ata_wait_drq
    jc .faild
    lea rdi, [r15 + ata_id_buf - kmain]
    mov dx, ATA_DATA
    mov ecx, 256
    rep insw
    ; LBA28 user sectors: words 60/61 -> dword @120
    mov eax, dword [r15 + ata_id_buf - kmain + 120]
    clc
    pop rbx
    ret
.faild:
    stc
    pop rbx
    ret

; --- init: probe both drives; picks data drive (slave preferred) ---
ata_init:
    mov byte [r15 + ata_drv - kmain], SEL_MASTER
    mov qword [r15 + ata_sectors - kmain], 0

    mov al, SEL_SLAVE
    call ata_identify
    test eax, eax
    jnz .slave_ok
    mov al, SEL_MASTER
    call ata_identify
    test eax, eax
    jz .done
.master_ok:
    mov [r15 + ata_sectors - kmain], rax
.done:
    ret
.slave_ok:
    mov byte [r15 + ata_drv - kmain], SEL_SLAVE
    mov [r15 + ata_sectors - kmain], rax
    jmp .done

; --- read one sector: RAX=LBA, RDI=buffer(512); CF=1 error ---
ata_read_one:
    push rbx
    push rcx
    push rdx
    push rdi
    mov rbx, rax             ; lba

    ; select drive | LBA bits 24-27
    mov eax, ebx
    shr eax, 24
    and al, 0x0F
    or al, [r15 + ata_drv - kmain]
    mov dx, ATA_DRV
    out dx, al
    mov al, 'z'
    push rdx
    mov dx, 0xE9
    out dx, al
    pop rdx
    call ata_wait_ready
    mov al, 'a'
    mov dx, 0xE9
    out dx, al
    jnc .sel_ok
    stc
    jmp .pop
.sel_ok:

    mov dx, ATA_SCNT
    mov al, 1
    out dx, al

    mov eax, ebx             ; LBA bits 0-7
    mov dx, ATA_LBAL
    out dx, al

    mov eax, ebx
    shr eax, 8               ; LBA bits 8-15
    mov dx, ATA_LBAM
    out dx, al

    mov eax, ebx
    shr eax, 16              ; LBA bits 16-23
    mov dx, ATA_LBAH
    out dx, al

    mov dx, ATA_STATC
    mov al, ATA_CMD_READ
    out dx, al

    call ata_wait_drq
    mov al, 'b'
    mov dx, 0xE9
    out dx, al
    jnc .got_data
    stc
    jmp .pop
.got_data:

    mov dx, ATA_DATA
    mov ecx, 256
    rep insw
    clc
.pop:
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    ret
.err:
    stc
    jmp .pop

; --- public: RAX=LBA, RCX=count, RDI=buffer; CF=1 error ---
disk_read_blocks:
    test rcx, rcx
    jz .ok
.dr_loop:
    push rcx
    call ata_read_one
    pop rcx
    jc .fail
    add rdi, 512
    inc rax
    dec rcx
    jnz .dr_loop
.ok:
    clc
    ret
.fail:
    stc
    ret

align 16
ata_id_buf   rb 512
ata_buf      rb 512
ata_drv      db SEL_MASTER
ata_sectors  dq 0
