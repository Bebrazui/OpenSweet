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
ATA_CMD_WRITE    = 0x30
ATA_CMD_FLUSH    = 0xE7

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
    xor eax, eax
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
    call ata_wait_ready
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
    jnc .got_data
    stc
    jmp .pop
.got_data:

    mov dx, ATA_DATA
    mov ecx, 256
    cld
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
; Reads RCX sectors starting from LBA RAX into [RDI].
; Batches reads up to 128 sectors per ATA READ command for fast throughput.
disk_read_blocks:
    test rcx, rcx
    jz .ok
    push rbx
    push rsi
    push r12
    push r13

    mov r12, rax             ; r12 = current LBA
    mov r13, rcx             ; r13 = remaining sectors

.drb_chunk_loop:
    test r13, r13
    jz .drb_done

    ; batch size = min(r13, 128)
    mov rbx, r13
    cmp rbx, 128
    jbe @f
    mov rbx, 128
@@:
    ; 1. Wait drive ready
    call ata_wait_ready
    jc .drb_err

    ; 2. Select drive | LBA bits 24-27
    mov eax, r12d
    shr eax, 24
    and al, 0x0F
    or al, [r15 + ata_drv - kmain]
    mov dx, ATA_DRV
    out dx, al
    call ata_wait_ready
    jc .drb_err

    ; 3. Sector count
    mov dx, ATA_SCNT
    mov al, bl               ; 1..128
    out dx, al

    ; 4. LBA bits 0-7, 8-15, 16-23
    mov eax, r12d
    mov dx, ATA_LBAL
    out dx, al

    mov eax, r12d
    shr eax, 8
    mov dx, ATA_LBAM
    out dx, al

    mov eax, r12d
    shr eax, 16
    mov dx, ATA_LBAH
    out dx, al

    ; 5. Issue ATA_CMD_READ
    mov dx, ATA_STATC
    mov al, ATA_CMD_READ
    out dx, al

    ; 6. Transfer each sector in batch
    mov esi, ebx
.drb_sec_loop:
    call ata_wait_drq
    jc .drb_err

    mov dx, ATA_DATA
    mov ecx, 256
    cld
    rep insw                 ; transfers 512 bytes, advances rdi by 512

    dec esi
    jnz .drb_sec_loop

    ; Advance LBA and decrease count
    add r12, rbx
    sub r13, rbx
    jmp .drb_chunk_loop

.drb_done:
    pop r13
    pop r12
    pop rsi
    pop rbx
.ok:
    clc
    ret

.drb_err:
    pop r13
    pop r12
    pop rsi
    pop rbx
    stc
    ret

; --- write one sector: RAX=LBA, RSI=buffer(512); CF=1 error ---
ata_write_one:
    push rbx
    push rcx
    push rdx
    push rsi
    mov rbx, rax             ; lba

    ; select drive | LBA bits 24-27
    mov eax, ebx
    shr eax, 24
    and al, 0x0F
    or al, [r15 + ata_drv - kmain]
    mov dx, ATA_DRV
    out dx, al
    call ata_wait_ready
    jc .err

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
    mov al, ATA_CMD_WRITE
    out dx, al

    call ata_wait_drq
    jc .err

    mov dx, ATA_DATA
    mov ecx, 256
    cld
    rep outsw

    ; 400ns delay then wait until ready
    mov ecx, 4
    mov dx, ATA_STATC
.w_delay:
    in al, dx
    dec ecx
    jnz .w_delay
    call ata_wait_ready
    jc .err

    clc
.pop:
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret
.err:
    stc
    jmp .pop

; --- public: RAX=LBA, RCX=count, RSI=buffer; CF=1 error ---
disk_write_blocks:
    test rcx, rcx
    jz .ok
.dw_loop:
    push rcx
    push rax
    call ata_write_one
    pop rax
    pop rcx
    jc .err
    add rsi, 512
    inc rax
    dec rcx
    jnz .dw_loop
.ok:
    clc
    ret
.err:
    stc
    ret

; --- flush drive write cache: CF=1 error ---
disk_flush_cache:
    push rdx
    mov al, [r15 + ata_drv - kmain]
    call ata_select
    jc .err
    mov dx, ATA_STATC
    mov al, ATA_CMD_FLUSH
    out dx, al
    call ata_wait_ready
    jc .err
    clc
    pop rdx
    ret
.err:
    stc
    pop rdx
    ret

align 16
ata_id_buf   rb 512
ata_buf      rb 512
ata_drv      db SEL_MASTER
ata_sectors  dq 0
