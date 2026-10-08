; Opensweet OS - Lightweight Network Stack (Ethernet, ARP, IPv4, ICMP Ping)
; Included from kernel.asm; runs in long mode (Ring 0), uses R15 (kmain base).

NET_OUR_IP0     = 10
NET_OUR_IP1     = 0
NET_OUR_IP2     = 2
NET_OUR_IP3     = 15            ; 10.0.2.15 (Default QEMU user net client IP)

; ==============================================================================
; net_init: Initialize network driver and protocol stack
; ==============================================================================
net_init:
    call rtl8139_init
    jc .no_net

    ; Set our default IP (10.0.2.15)
    mov byte [r15 + net_ip + 0 - kmain], NET_OUR_IP0
    mov byte [r15 + net_ip + 1 - kmain], NET_OUR_IP1
    mov byte [r15 + net_ip + 2 - kmain], NET_OUR_IP2
    mov byte [r15 + net_ip + 3 - kmain], NET_OUR_IP3

    lea rsi, [r15 + str_net_stack_ok - kmain]
    call puts
    ret

.no_net:
    lea rsi, [r15 + str_net_none - kmain]
    call puts
    ret

; ==============================================================================
; net_poll: Poll for incoming packet and process protocol handlers
; Returns: CF=0 if packet processed, CF=1 if idle
; ==============================================================================
net_poll:
    cmp byte [r15 + rtl_present - kmain], 0
    je .poll_none

    push rdi
    push rsi
    push rdx

    lea rdi, [r15 + net_rx_packet - kmain]
    mov edx, 1514
    call rtl8139_poll
    jc .poll_done_none

    ; Packet received! RAX = length, data in net_rx_packet
    test eax, eax
    jz .poll_done_none

    lea rsi, [r15 + net_rx_packet - kmain]
    mov edx, eax
    call net_handle_packet

    pop rdx
    pop rsi
    pop rdi
    clc
    ret

.poll_done_none:
    pop rdx
    pop rsi
    pop rdi
.poll_none:
    stc
    ret

; ==============================================================================
; net_handle_packet: Parse Ethernet frame and dispatch
; Input: RSI = packet pointer, EDX = length
; ==============================================================================
net_handle_packet:
    cmp edx, 14                 ; Minimum Ethernet header size
    jb .drop

    ; Check EtherType at offset 12 (Big Endian)
    ; 0x0806 (ARP)  -> in memory: 0x08, 0x06 -> x86 word: 0x0608
    ; 0x0800 (IPv4) -> in memory: 0x08, 0x00 -> x86 word: 0x0008
    movzx eax, word [rsi + 12]
    cmp ax, 0x0608              ; ARP
    je .handle_arp

    cmp ax, 0x0008              ; IPv4
    je .handle_ipv4

.drop:
    ret

; ------------------------------------------------------------------------------
; ARP Handler
; ------------------------------------------------------------------------------
.handle_arp:
    cmp edx, 42                 ; 14 (Eth) + 28 (ARP)
    jb .drop

    ; Check ARP Operation: offset 20-21 (Opcode 1 = Request, Big Endian -> 0x0100)
    movzx eax, word [rsi + 20]
    cmp ax, 0x0100              ; ARP Request
    jne .drop

    ; Check Target IP at offset 38-41 == Our IP (10.0.2.15)
    mov eax, [rsi + 38]
    cmp eax, dword [r15 + net_ip - kmain]
    jne .drop

    ; Match! Construct ARP Reply in net_tx_packet
    push rdi
    push rsi
    push rcx

    lea rdi, [r15 + net_tx_packet - kmain]

    ; 1. Ethernet Header (14 bytes):
    ; Dest MAC = packet[22..27]
    mov rax, [rsi + 22]
    mov [rdi + 0], eax
    shr rax, 32
    mov [rdi + 4], ax

    ; Src MAC = our rtl_mac
    mov eax, dword [r15 + rtl_mac - kmain]
    mov [rdi + 6], eax
    mov ax, word [r15 + rtl_mac + 4 - kmain]
    mov [rdi + 10], ax

    ; EtherType = 0x0806 (ARP) -> Big Endian: 0x06, 0x08
    mov word [rdi + 12], 0x0608

    ; 2. ARP Packet (28 bytes):
    ; Hardware Type = 1 (Ethernet) -> 0x0100
    mov word [rdi + 14], 0x0100
    ; Protocol Type = 0x0800 (IPv4) -> 0x0008
    mov word [rdi + 16], 0x0008
    ; HW size = 6, Proto size = 4
    mov byte [rdi + 18], 6
    mov byte [rdi + 19], 4
    ; Opcode = 2 (Reply) -> Big Endian: 0x0200
    mov word [rdi + 20], 0x0200

    ; Sender MAC = our rtl_mac
    mov eax, dword [r15 + rtl_mac - kmain]
    mov [rdi + 22], eax
    mov ax, word [r15 + rtl_mac + 4 - kmain]
    mov [rdi + 26], ax

    ; Sender IP = our net_ip
    mov eax, dword [r15 + net_ip - kmain]
    mov [rdi + 28], eax

    ; Target MAC = packet[22..27]
    mov rax, [rsi + 22]
    mov [rdi + 32], eax
    shr rax, 32
    mov [rdi + 36], ax

    ; Target IP = packet[28..31] (Sender IP from request)
    mov eax, [rsi + 28]
    mov [rdi + 38], eax

    ; Send ARP Reply (42 bytes)
    lea rsi, [r15 + net_tx_packet - kmain]
    mov edx, 42
    call rtl8139_send

    lea rsi, [r15 + str_arp_reply - kmain]
    call puts

    pop rcx
    pop rsi
    pop rdi
    ret

; ------------------------------------------------------------------------------
; IPv4 Handler
; ------------------------------------------------------------------------------
.handle_ipv4:
    cmp edx, 34                 ; 14 (Eth) + 20 (min IP)
    jb .drop

    ; Check IPv4 version (byte at offset 14 high nibble == 4)
    mov al, [rsi + 14]
    and al, 0xF0
    cmp al, 0x40
    jne .drop

    ; Check Destination IP at offset 30 == Our IP
    mov eax, [rsi + 30]
    cmp eax, dword [r15 + net_ip - kmain]
    jne .drop

    ; Check IP Protocol at offset 23
    mov al, [rsi + 23]
    cmp al, 1                   ; ICMP
    je .handle_icmp

    ret

; ------------------------------------------------------------------------------
; ICMP Ping Handler (Echo Request -> Echo Reply)
; ------------------------------------------------------------------------------
.handle_icmp:
    ; IP header length in bytes: (packet[14] & 0x0F) * 4
    movzx eax, byte [rsi + 14]
    and eax, 0x0F
    shl eax, 2
    lea r8d, [eax + 14]         ; r8d = offset of ICMP header (usually 34)

    ; Check ICMP Type at offset r8d: 8 = Echo Request
    mov al, [rsi + r8]
    cmp al, 8
    jne .drop

    ; ICMP Echo Request received! Convert packet in-place to Echo Reply and send.
    push rdi
    push rsi
    push rcx
    push rdx

    ; 1. Swap MAC addresses in Ethernet header (offset 0..11)
    ; Dest MAC = incoming Src MAC [rsi + 6]
    ; Src MAC = our rtl_mac
    mov rax, [rsi + 6]
    mov [rsi + 0], eax
    shr rax, 32
    mov [rsi + 4], ax

    mov eax, dword [r15 + rtl_mac - kmain]
    mov [rsi + 6], eax
    mov ax, word [r15 + rtl_mac + 4 - kmain]
    mov [rsi + 10], ax

    ; 2. Swap IP addresses in IP header
    ; Dest IP (offset 30) = incoming Src IP (offset 26)
    ; Src IP (offset 26) = our net_ip
    mov eax, [rsi + 26]         ; sender IP
    mov [rsi + 30], eax         ; becomes dest IP
    mov eax, dword [r15 + net_ip - kmain]
    mov [rsi + 26], eax         ; our IP becomes src IP

    ; Recalculate IP Checksum (offset 24)
    mov word [rsi + 24], 0
    push rsi
    push rdx
    lea rsi, [rsi + 14]         ; start of IP header
    movzx ecx, byte [rsi]       ; header length in 32-bit words
    and ecx, 0x0F
    shl ecx, 1                  ; in 16-bit words
    call net_checksum
    pop rdx
    pop rsi
    mov [rsi + 24], ax

    ; 3. ICMP Echo Reply:
    ; Type = 0 (offset r8)
    mov byte [rsi + r8], 0

    ; Recalculate ICMP Checksum (offset r8 + 2)
    ; ICMP length = total_len - 14 - ip_hdr_len
    mov word [rsi + r8 + 2], 0
    push rsi
    push rdx
    movzx ecx, word [rsi + 16]  ; IP Total Length (Big Endian)
    xchg cl, ch                 ; Convert to host endian
    sub ecx, 20                 ; minus 20-byte IP header
    shr ecx, 1                  ; 16-bit words
    lea rsi, [rsi + r8]         ; start of ICMP header
    call net_checksum
    pop rdx
    pop rsi
    mov [rsi + r8 + 2], ax

    ; 4. Send the Echo Reply frame!
    ; Length EDX remains the same as incoming packet
    call rtl8139_send

    lea rsi, [r15 + str_icmp_reply - kmain]
    call puts

    pop rdx
    pop rcx
    pop rsi
    pop rdi
    ret

; ==============================================================================
; net_checksum: Compute Internet 16-bit one's complement checksum
; Input:  RSI = data pointer, ECX = word count (16-bit)
; Returns: AX = checksum (network byte order)
; ==============================================================================
net_checksum:
    push rsi
    push rcx
    push rdx
    xor edx, edx
.csum_loop:
    test ecx, ecx
    jz .csum_done
    movzx eax, word [rsi]
    add edx, eax
    add rsi, 2
    dec ecx
    jmp .csum_loop
.csum_done:
    ; Fold 32-bit sum into 16-bit
    mov eax, edx
    shr eax, 16
    and edx, 0xFFFF
    add eax, edx
    mov edx, eax
    shr edx, 16
    add eax, edx
    not ax
    pop rdx
    pop rcx
    pop rsi
    ret

; ==============================================================================
; task_net_daemon: Background task that polls network and handles packets
; ==============================================================================
task_net_daemon:
.loop:
    call net_poll
    mov ecx, 10                  ; 10 ms sleep
    call task_sleep
    jmp .loop

; ==============================================================================
; Network Data Section
; ==============================================================================
net_ip              db 10, 0, 2, 15
str_name_net        db "netd", 0
str_net_stack_ok    db "[NET] Network stack initialized (IP: 10.0.2.15)", 10, 0
str_net_none        db "[NET] No network adapter found", 10, 0
str_arp_reply       db "[NET] Sent ARP Reply to host", 10, 0
str_icmp_reply      db "[NET] Sent ICMP Echo Reply (Ping OK!)", 10, 0

align 16
net_rx_packet       rb 1536
net_tx_packet       rb 1536

