; Opensweet OS - Realtek RTL8139 Network Driver
; Included from kernel.asm; runs in long mode (Ring 0), uses R15 (kmain base).

RTL_VENDOR_ID   = 0x10EC
RTL_DEVICE_ID   = 0x8139

; RTL8139 Register Offsets (I/O space from BAR0)
RTL_REG_MAC0    = 0x00          ; Ethernet ID registers 0-5
RTL_REG_MAR0    = 0x08          ; Multicast registers 0-7
RTL_REG_TSD0    = 0x10          ; Transmit status descriptors 0-3
RTL_REG_TSAD0   = 0x20          ; Transmit start address descriptors 0-3
RTL_REG_RBSTART = 0x30          ; Receive buffer start address
RTL_REG_CR      = 0x37          ; Command register
RTL_REG_CAPR    = 0x38          ; Current address of packet read
RTL_REG_CBA     = 0x3A          ; Current buffer address
RTL_REG_IMR     = 0x3C          ; Interrupt mask register
RTL_REG_ISR     = 0x3E          ; Interrupt status register
RTL_REG_TCR     = 0x40          ; Transmit configuration register
RTL_REG_RCR     = 0x44          ; Receive configuration register
RTL_REG_CONFIG1 = 0x52          ; Configuration register 1

; Command Register (CR) bits
RTL_CR_BUFE     = 0x01          ; Buffer empty
RTL_CR_TE       = 0x04          ; Transmit enable
RTL_CR_RE       = 0x08          ; Receive enable
RTL_CR_RST      = 0x10          ; Reset

; Receive Configuration Register (RCR) bits
; 0x000F = AAP (promisc) | APM (match MAC) | AM (multicast) | AB (broadcast)
RTL_RCR_CONFIG  = 0x0000000F

; Buffer sizing
RTL_RX_BUF_SIZE = 8192          ; 8KB ring buffer + 16B header + 1536B wrap
RTL_TX_BUF_SIZE = 2048

; ==============================================================================
; rtl8139_pci_scan: Scan PCI bus for Realtek RTL8139 (10EC:8139)
; Returns: CF=0 if found, CF=1 if not found
; Sets: rtl_io_base, rtl_irq, rtl_pci_bus, rtl_pci_slot, rtl_pci_func
; ==============================================================================
rtl8139_pci_scan:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9

    xor r8d, r8d                ; r8d = bus (0..255)
.bus_loop:
    xor r9d, r9d                ; r9d = slot / dev (0..31)
.dev_loop:
    xor ebx, ebx                ; ebx = func (0..7)
.func_loop:
    ; Form PCI config address for Vendor/Device ID (offset 0)
    mov eax, 0x80000000
    mov ecx, r8d
    shl ecx, 16
    or eax, ecx
    mov ecx, r9d
    shl ecx, 11
    or eax, ecx
    mov ecx, ebx
    shl ecx, 8
    or eax, ecx                 ; offset 0

    mov dx, 0xCF8
    out dx, eax
    mov dx, 0xCFC
    in eax, dx

    cmp ax, 0xFFFF
    je .next_func

    ; Check Vendor ID == 0x10EC and Device ID == 0x8139
    cmp ax, RTL_VENDOR_ID
    jne .next_func
    shr eax, 16
    cmp ax, RTL_DEVICE_ID
    jne .next_func

    ; Found RTL8139!
    mov [r15 + rtl_pci_bus - kmain], r8b
    mov [r15 + rtl_pci_slot - kmain], r9b
    mov [r15 + rtl_pci_func - kmain], bl

    ; Read BAR0 (offset 0x10) to get I/O base port
    mov eax, 0x80000000
    mov ecx, r8d
    shl ecx, 16
    or eax, ecx
    mov ecx, r9d
    shl ecx, 11
    or eax, ecx
    mov ecx, ebx
    shl ecx, 8
    or eax, ecx
    or eax, 0x10                ; offset 0x10 = BAR0

    mov dx, 0xCF8
    out dx, eax
    mov dx, 0xCFC
    in eax, dx

    ; If bit 0 == 1, it's I/O port
    and eax, 0xFFFFFFFC
    mov [r15 + rtl_io_base - kmain], ax

    ; Read Interrupt Line (offset 0x3C, byte 0)
    mov eax, 0x80000000
    mov ecx, r8d
    shl ecx, 16
    or eax, ecx
    mov ecx, r9d
    shl ecx, 11
    or eax, ecx
    mov ecx, ebx
    shl ecx, 8
    or eax, ecx
    or eax, 0x3C                ; offset 0x3C

    mov dx, 0xCF8
    out dx, eax
    mov dx, 0xCFC
    in eax, dx
    mov [r15 + rtl_irq - kmain], al

    ; Enable PCI Bus Master & I/O Space (Command reg at offset 0x04)
    mov eax, 0x80000000
    mov ecx, r8d
    shl ecx, 16
    or eax, ecx
    mov ecx, r9d
    shl ecx, 11
    or eax, ecx
    mov ecx, ebx
    shl ecx, 8
    or eax, ecx
    or eax, 0x04                ; offset 0x04 = Command

    mov dx, 0xCF8
    out dx, eax
    mov dx, 0xCFC
    in eax, dx
    or ax, 0x0005               ; Bit 0: I/O space, Bit 2: Bus Master
    mov dx, 0xCFC
    out dx, eax

    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    clc                         ; Found
    ret

.next_func:
    inc ebx
    cmp ebx, 8
    jb .func_loop

    inc r9d
    cmp r9d, 32
    jb .dev_loop

    inc r8d
    cmp r8d, 256
    jb .bus_loop

    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    stc                         ; Not found
    ret

; ==============================================================================
; rtl8139_init: Initialize the RTL8139 network card
; Returns: CF=0 on success, CF=1 on failure
; ==============================================================================
rtl8139_init:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    call rtl8139_pci_scan
    jnc @f
    ; RTL8139 not detected
    mov byte [r15 + rtl_present - kmain], 0
    stc
    jmp .init_done
@@:
    mov byte [r15 + rtl_present - kmain], 1
    movzx edx, word [r15 + rtl_io_base - kmain]

    ; 1. Power on: Write 0x00 to CONFIG1 (offset 0x52)
    push rdx
    add edx, RTL_REG_CONFIG1
    xor al, al
    out dx, al
    pop rdx

    ; 2. Software Reset: Write 0x10 to CR (offset 0x37)
    push rdx
    add edx, RTL_REG_CR
    mov al, RTL_CR_RST
    out dx, al
.wait_reset:
    in al, dx
    test al, RTL_CR_RST
    jnz .wait_reset
    pop rdx

    ; 3. Read Hardware MAC Address (offset 0x00..0x05)
    push rdx
    in eax, dx                  ; Bytes 0..3
    mov dword [r15 + rtl_mac - kmain], eax
    add edx, 4
    in ax, dx                   ; Bytes 4..5
    mov word [r15 + rtl_mac + 4 - kmain], ax
    pop rdx

    ; 4. Allocate RX Ring Buffer (4 pages = 16KB contiguous physical memory)
    mov edi, 4
    call pmm_alloc_contiguous
    test rax, rax
    jz .init_fail
    mov [r15 + rtl_rx_buf_phys - kmain], rax

    ; Zero RX buffer
    mov rdi, rax
    mov ecx, 2048               ; 2048 qwords = 16KB
    xor eax, eax
    rep stosq

    ; Set RBSTART register (offset 0x30)
    mov rax, [r15 + rtl_rx_buf_phys - kmain]
    push rdx
    add edx, RTL_REG_RBSTART
    out dx, eax
    pop rdx

    ; 5. Allocate TX Buffers (4 buffers x 2KB = 2 contiguous pages = 8KB)
    mov edi, 2
    call pmm_alloc_contiguous
    test rax, rax
    jz .init_fail
    mov [r15 + rtl_tx_buf_phys - kmain], rax

    ; Setup TX buffer pointers
    mov [r15 + rtl_tx_bufs + 0*8 - kmain], rax
    add rax, RTL_TX_BUF_SIZE
    mov [r15 + rtl_tx_bufs + 1*8 - kmain], rax
    add rax, RTL_TX_BUF_SIZE
    mov [r15 + rtl_tx_bufs + 2*8 - kmain], rax
    add rax, RTL_TX_BUF_SIZE
    mov [r15 + rtl_tx_bufs + 3*8 - kmain], rax

    ; Initialize TX descriptor index and RX offset
    mov byte [r15 + rtl_tx_cur - kmain], 0
    mov word [r15 + rtl_rx_offset - kmain], 0

    ; 6. Set CAPR to 0 (offset 0x38)
    push rdx
    add edx, RTL_REG_CAPR
    xor ax, ax
    out dx, ax
    pop rdx

    ; 7. Configure Receive Register RCR (offset 0x44)
    ; Set AAP | APM | AM | AB (0x0F)
    push rdx
    add edx, RTL_REG_RCR
    mov eax, RTL_RCR_CONFIG
    out dx, eax
    pop rdx

    ; 8. Enable Receive and Transmit: Write 0x0C to CR (offset 0x37)
    push rdx
    add edx, RTL_REG_CR
    mov al, RTL_CR_RE or RTL_CR_TE
    out dx, al
    pop rdx

    ; 9. Enable interrupts in IMR: ROK (0x0001) | TOK (0x0004) = 0x0005
    push rdx
    add edx, RTL_REG_IMR
    mov ax, 0x0005
    out dx, ax
    pop rdx

    ; Log initialization
    lea rsi, [r15 + str_rtl_init_ok - kmain]
    call puts
    movzx eax, word [r15 + rtl_io_base - kmain]
    call puthex16
    lea rsi, [r15 + str_rtl_mac - kmain]
    call puts
    ; Print MAC address
    mov ecx, 6
    xor ebx, ebx
.print_mac_loop:
    movzx eax, byte [r15 + rtl_mac - kmain + rbx]
    call puthex8
    inc ebx
    cmp ebx, 6
    jae @f
    mov al, ':'
    call putc
@@:
    loop .print_mac_loop
    mov al, 10
    call putc

    clc
    jmp .init_done

.init_fail:
    mov byte [r15 + rtl_present - kmain], 0
    stc

.init_done:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

; ==============================================================================
; rtl8139_send: Send an Ethernet packet
; Input:  RSI = packet data pointer
;         EDX = packet length in bytes (max 1514)
; Returns: CF=0 on success, CF=1 on failure
; ==============================================================================
rtl8139_send:
    cmp byte [r15 + rtl_present - kmain], 0
    je .send_fail

    test edx, edx
    jz .send_fail
    cmp edx, 1514
    ja .send_fail

    push rbx
    push rcx
    push rsi
    push rdi
    push rdx

    ; Current TX descriptor index (0..3)
    movzx ebx, byte [r15 + rtl_tx_cur - kmain]

    ; Target TX buffer address
    mov rdi, [r15 + rtl_tx_bufs - kmain + rbx*8]

    ; Minimum Ethernet packet size is 60 bytes (excluding 4-byte CRC)
    mov ecx, edx
    rep movsb

    ; Pad to 60 bytes if needed
    cmp edx, 60
    jae .no_pad
    mov ecx, 60
    sub ecx, edx
    xor al, al
    rep stosb
    mov edx, 60
.no_pad:

    ; Base I/O port
    movzx eax, word [r15 + rtl_io_base - kmain]

    ; 1. Write physical address to TSAD[ebx] (offset 0x20 + ebx*4)
    lea edx, [eax + RTL_REG_TSAD0 + ebx*4]
    mov rcx, [r15 + rtl_tx_bufs - kmain + rbx*8]
    push rdx
    mov edx, edx
    mov eax, ecx
    pop rdx
    out dx, eax

    ; 2. Write packet length & clear OWN bit to TSD[ebx] (offset 0x10 + ebx*4)
    pop rdx                     ; restore length in edx
    push rdx
    movzx eax, word [r15 + rtl_io_base - kmain]
    lea ecx, [eax + RTL_REG_TSD0 + ebx*4]
    mov eax, edx
    and eax, 0x1FFF             ; length bits 0..12
    mov dx, cx
    out dx, eax                 ; start transmit

    ; Advance tx_cur = (tx_cur + 1) & 3
    inc ebx
    and ebx, 3
    mov [r15 + rtl_tx_cur - kmain], bl

    pop rdx
    pop rdi
    pop rsi
    pop rcx
    pop rbx
    clc
    ret

.send_fail:
    stc
    ret

; ==============================================================================
; rtl8139_poll: Poll for an incoming Ethernet packet
; Input:  RDI = destination buffer pointer
;         EDX = max destination buffer size
; Returns: RAX = packet length (0 if no packet), CF=0 on packet, CF=1 if none
; ==============================================================================
rtl8139_poll:
    cmp byte [r15 + rtl_present - kmain], 0
    je .no_packet

    push rbx
    push rcx
    push rsi
    push rdi
    push r8
    push r9

    movzx edx, word [r15 + rtl_io_base - kmain]
    add edx, RTL_REG_CR
    in al, dx
    test al, RTL_CR_BUFE        ; Buffer Empty?
    jnz .no_packet_pop

    ; Packet is present! Read packet at rtl_rx_offset
    movzx ebx, word [r15 + rtl_rx_offset - kmain]
    mov rsi, [r15 + rtl_rx_buf_phys - kmain]
    add rsi, rbx

    ; Packet header in RX ring:
    ; word [rsi + 0] = status (bit 0 = ROK)
    ; word [rsi + 2] = length (includes 4-byte CRC)
    movzx eax, word [rsi]
    test al, 1                  ; ROK bit
    jz .bad_packet

    movzx ecx, word [rsi + 2]   ; packet length + 4 CRC bytes
    cmp ecx, 4
    jbe .bad_packet
    sub ecx, 4                  ; strip CRC

    ; Copy Ethernet frame to destination buffer
    mov r8, rsi
    add r8, 4                   ; skip 4-byte header
    mov r9d, ecx                ; save actual length

    mov rsi, r8
    ; RDI is destination buffer from caller
    rep movsb

    ; Update rx_offset: (rx_offset + length + 4 + 3) & ~3
    add ebx, r9d
    add ebx, 4 + 4 + 3          ; length + CRC(4) + header(4) + align(3)
    and ebx, not 3
    cmp ebx, RTL_RX_BUF_SIZE
    jb @f
    sub ebx, RTL_RX_BUF_SIZE
@@:
    mov [r15 + rtl_rx_offset - kmain], bx

    ; Update CAPR register: outw(io + 0x38, rx_offset - 0x10)
    movzx edx, word [r15 + rtl_io_base - kmain]
    add edx, RTL_REG_CAPR
    lea eax, [ebx - 0x10]
    out dx, ax

    ; Acknowledge interrupt in ISR (clear ROK bit)
    movzx edx, word [r15 + rtl_io_base - kmain]
    add edx, RTL_REG_ISR
    mov ax, 0x0001
    out dx, ax

    mov eax, r9d                ; return packet length
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rcx
    pop rbx
    clc
    ret

.bad_packet:
.no_packet_pop:
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rcx
    pop rbx
.no_packet:
    xor eax, eax
    stc
    ret

; ==============================================================================
; RTL8139 Data Section
; ==============================================================================
rtl_present         db 0
rtl_pci_bus         db 0
rtl_pci_slot        db 0
rtl_pci_func        db 0
rtl_irq             db 0
rtl_tx_cur          db 0
rtl_io_base         dw 0
rtl_rx_offset       dw 0
rtl_rx_buf_phys     dq 0
rtl_tx_buf_phys     dq 0
rtl_tx_bufs         dq 0, 0, 0, 0
rtl_mac             db 0, 0, 0, 0, 0, 0, 0, 0

str_rtl_init_ok     db "[NET] RTL8139 initialized at I/O 0x", 0
str_rtl_mac         db ", MAC: ", 0
