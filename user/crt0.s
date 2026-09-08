/* =============================================================================
 * OpenSweet OS - C Runtime Startup Routine (user/crt0.s)
 * Entry point for Ring 3 C applications. Invokes main() and terminates via SYS_EXIT.
 * ============================================================================= */

.globl _start
.globl __main
.text

_start:
    /* Align stack to 16 bytes per System V AMD64 ABI */
    andq $-16, %rsp

    /* Pass argc = 0, argv = NULL */
    xor %edi, %edi
    xor %esi, %esi
    call main

    /* Exit code in EAX -> EDI */
    mov %eax, %edi
    mov $0, %eax        /* SYS_EXIT = 0 */
    syscall

.halt:
    jmp .halt

__main:
    ret
