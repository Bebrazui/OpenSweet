/* =============================================================================
 * OpenSweet OS - C Runtime Startup Routine (user/crt0.s)
 * Entry point for Ring 3 C applications. Invokes main() and terminates via SYS_EXIT.
 * ============================================================================= */

.globl _start
.globl __main
.text

_start:
    /* Extract System V AMD64 ABI startup arguments from initial stack */
    movq (%rsp), %rdi                 /* %rdi = argc */
    leaq 8(%rsp), %rsi                /* %rsi = argv */
    leaq 8(%rsi, %rdi, 8), %rdx       /* %rdx = envp (&argv[argc + 1]) */

    /* Align stack to 16 bytes per AMD64 ABI requirements */
    andq $-16, %rsp
    call main

    /* Exit code in EAX -> EDI */
    mov %eax, %edi
    mov $0, %eax        /* SYS_EXIT = 0 */
    syscall

.halt:
    jmp .halt

__main:
    ret

.globl ___chkstk_ms
.globl __chkstk_ms
.globl ___chkstk
.globl __chkstk
___chkstk_ms:
__chkstk_ms:
___chkstk:
__chkstk:
    ret
