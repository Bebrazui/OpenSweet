; Opensweet OS - x86_64 kernel (stage 2), entered in long mode at 0x10000.
; VGA text output, serial COM1 mirror, polled PS/2 keyboard echo shell.

VGA_BASE = 0xB8000
COLS     = 80
ROWS     = 25
COM1     = 0x3F8

org 0x10000
use64

kmain:
    cli
    cld
    mov rsp, 0x90000

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
    mov word [cur], 0

    call init_idt
    call pic_init
    call pit_init

    sti

    mov rsi, banner
    call puts

    ; --- CPU vendor via CPUID ---
    xor eax, eax
    cpuid
    mov dword [vendor], ebx
    mov dword [vendor+4], edx
    mov dword [vendor+8], ecx
    mov byte [vendor+12], 0
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
    movzx ecx, byte [cmd_len]
    cmp ecx, 63
    jae .echo
    mov byte [cmd_buf + rcx], al
    inc byte [cmd_len]
.echo:
    call putc
    jmp .shell
.bs:
    cmp byte [cmd_len], 0
    je .shell
    dec byte [cmd_len]
    mov al, 8
    call putc
    jmp .shell
.enter:
    mov al, 13                ; CR + LF on newline
    call putc
    mov al, 10
    call putc
    movzx ecx, byte [cmd_len]
    mov byte [cmd_buf + rcx], 0
    mov byte [cmd_len], 0

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
    mov eax, [timer_ticks]
    call puthex64
    mov al, 10
    call putc
    jmp .prompt
.prompt:
    mov rsi, prompt
    call puts
    jmp .shell

; --- read next ASCII from keyboard ring buffer; AL=0 if empty ---
getc:
    movzx ecx, byte [kb_tail]
    movzx edx, byte [kb_head]
    cmp ecx, edx
    je .empty
    mov al, [kb_buf + rcx]
    inc ecx
    and ecx, 31
    mov [kb_tail], cl
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
    lea rbx, [keymap_norm]
    cmp byte [shift], 0
    je .sel
    lea rbx, [keymap_shift]
.sel:
    movzx r8d, al
    mov al, [rbx + r8]
    test al, al
    jz .skip
    movzx ecx, byte [kb_head]
    lea edx, [ecx+1]
    and edx, 31
    movzx r8d, byte [kb_tail]
    cmp edx, r8d
    je .skip                  ; full - drop char
    mov [kb_buf + rcx], al
    mov [kb_head], dl
.skip:
    ret
.sh_on:
    mov byte [shift], 1
    ret
.sh_off:
    mov byte [shift], 0
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
    movzx edx, word [cur]
    mov rdi, VGA_BASE
    lea rdi, [rdi + rdx*2]
    mov [rdi], al
    mov byte [rdi+1], 0x07
    inc word [cur]
    cmp word [cur], COLS*ROWS
    jb  .out
    call scroll
    jmp .out
.nl:
    movzx eax, word [cur]
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
    mov [cur], ax
    jmp .out
.bs:
    cmp word [cur], 0
    je  .out
    dec word [cur]
    movzx edx, word [cur]
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
    mov edi, idt
    mov ecx, 256*16/4
    xor eax, eax
    rep stosd

    mov rsi, ex_table
    xor ebx, ebx
.next:
    mov rax, [rsi]
    mov rdi, rbx
    shl rdi, 4                ; entry size = 16 bytes
    add rdi, idt
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
    add rdi, idt + 32*16
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

    lidt tword [idtr]
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

; --- hardware IRQ common: EOI + dispatch vector 32..47 ---
common_irq:
    push rax
    mov rax, [rsp+8]          ; vector number pushed by stub
    cmp rax, 40
    jb .master_eoi
    mov al, 0x20
    out 0xA0, al              ; EOI slave
.master_eoi:
    mov al, 0x20
    out 0x20, al              ; EOI master
    mov rax, [rsp+8]
    sub rax, 32               ; irq index
    cmp rax, 0
    je .timer
    cmp rax, 1
    je .kbd
    jmp .ret
.timer:
    inc dword [timer_ticks]
    jmp .ret
.kbd:
    call kb_irq
.ret:
    pop rax
    add rsp, 8                ; drop pushed vector -> rsp points at RIP
    iretq

rept 16 n
{
    irq#n:
    push n+31                 ; vector = 32 + (n-1)
    jmp common_irq
}

align 8
irq_table:
rept 16 n { dq irq#n }

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
banner   db "Opensweet OS 0.0.1 [x86_64] - built with FASM", 10, 0
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
