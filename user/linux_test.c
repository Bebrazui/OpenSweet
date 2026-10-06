/*
 * Standalone Linux x86_64 test binary for the OpenSweet Linux ABI.
 * Raw syscalls used:
 *   1  = write, 39 = getpid, 57 = fork, 60 = exit, 61 = wait4
 *
 * Sequence:
 *   1. hello line (regression marker used by the screenshot tests)
 *   2. fork -> child prints its pid and exits 42,
 *      parent wait4()s and reports pid/wait/status (expect status 0x2a00)
 *   3. execve(busybox echo hello-from-execve)
 *   4. signals: rt_sigaction(SIGUSR1)+SA_RESTORER, kill(self) -> handler ->
 *      rt_sigreturn (g_saw=10, kill rc=0), sigprocmask BLOCK roundtrip
 *      (old=0x0 cur=0x800), child kill(self, SIGTERM) -> wait4 status=0xf
 *   5. faults & timers: child #PF without handler -> wait4 status=0xb
 *      (WIFSIGNALED SIGSEGV), child #PF with handler -> handler prints
 *      si_addr=CR2 and exits 42 -> status=0x2a00, alarm(1)+pause ->
 *      SIGALRM handler + pause returns -EINTR
 *   --sigint: never returns — blocks in pause() until the keyboard Ctrl+C
 *      path kills it (serial: "[Linux ABI] Terminated by signal 2").
 */

static inline long long sys3(long long num, long long a1, long long a2, long long a3) {
    long long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long long sys1(long long num, long long a1) {
    long long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long long sys_wait4(long long pid, int *status) {
    long long ret;
    register long r10 __asm__("r10") = 0;   /* rusage = NULL */
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(61L), "D"(pid), "S"(status), "d"(0LL), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static void print(const char *s) {
    long long n = 0;
    while (s[n]) n++;
    sys3(1, 1, (long long)s, n);
}

static void print_dec(long long v) {
    char buf[24];
    int i = 24;
    unsigned long long u;
    int neg = 0;
    if (v < 0) { neg = 1; u = (unsigned long long)(-v); }
    else        { u = (unsigned long long)v; }
    if (u == 0) buf[--i] = '0';
    while (u) { buf[--i] = (char)('0' + (u % 10)); u /= 10; }
    if (neg) buf[--i] = '-';
    sys3(1, 1, (long long)(buf + i), 24 - i);
}

static void print_hex(unsigned long long v) {
    char buf[16];
    int i = 16;
    const char *hex = "0123456789abcdef";
    if (v == 0) buf[--i] = '0';
    while (v) { buf[--i] = hex[v & 0xF]; v >>= 4; }
    sys3(1, 1, (long long)(buf + i), 16 - i);
}

static const char msg[] = "[Linux ABI] Hello from native Linux x86_64 ELF!\n";

/* Entry trampoline: the kernel enters the process with the Linux
 * argc/argv/envp/auxv vector at RSP (0x1FFFFE00) and a 16-byte-aligned RSP —
 * exactly like Linux create_elf_tables/STACK_ROUND. Capture argc/argv first,
 * then align and CALL os_main: GCC compiles C functions under the CALL
 * convention where RSP%16==8 on entry — otherwise SSE spills (movaps) die
 * with #GP on the misaligned address. glibc/musl crt1 do the same. */
__asm__(
    ".global _start\n"
    "_start:\n"
    "    mov (%rsp), %rdi\n"           /* argc from the entry vector */
    "    lea 8(%rsp), %rsi\n"          /* argv */
    "    andq $-16, %rsp\n"
    "    call os_main\n"
);

/* Kernel restorer trampoline for SA_RESTORER: the handler's `ret` lands here
 * and issues rt_sigreturn with RSP exactly 8 bytes past the frame base. */
__asm__(
    ".global sig_restore_rt\n"
    "sig_restore_rt:\n"
    "    mov $15, %eax\n"
    "    syscall\n"
);
extern void sig_restore_rt(void);

/* Linux x86_64 userspace sigaction layout: handler, flags, restorer, mask */
struct k_sigaction {
    long long handler;      /* +0  */
    long long flags;        /* +8  */
    long long restorer;     /* +16 */
    long long mask;         /* +24 */
};

#define SA_RESTORER_FLAG 0x04000000

static volatile long long g_sig_seen = 0;
static volatile long long g_alrm_seen = 0;

static void usr1_handler(long long sig, void *info, void *uc) {
    (void)info;
    (void)uc;
    g_sig_seen = sig;
    print("[Linux ABI] SIGUSR1 handler ran\n");
}

static void alrm_handler(long long sig, void *info, void *uc) {
    (void)sig;
    (void)info;
    (void)uc;
    g_alrm_seen = 1;
    print("[Linux ABI] SIGALRM handler ran\n");
}

/* SIGSEGV handler: report si_addr (CR2 for a #PF) and exit(42) — returning
 * would re-execute the faulting instruction and loop forever. */
static void segv_handler(long long sig, void *info, void *uc) {
    (void)sig;
    (void)uc;
    long long addr = *(long long *)((char *)info + 16);   /* si_addr */
    print("[Linux ABI] SIGSEGV handler ran, si_addr=0x");
    print_hex((unsigned long long)addr);
    print("\n");
    sys1(60, 42);
}

static int streq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

void os_main(int argc, char **argv) {
    /* --sigint: keyboard-path victim — block until Ctrl+C SIGINT kills us
     * with the default action (wait status = 2). Never returns otherwise. */
    if (argc > 1 && argv && argv[1] && streq(argv[1], "--sigint")) {
        print("[Linux ABI] --sigint: blocking in pause, press Ctrl+C\n");
        for (;;) sys1(34, 0);          /* pause() — no return before a signal */
    }

    sys3(1, 1, (long long)msg, sizeof(msg) - 1);

    long long pid = sys1(57, 0);                 /* fork() */
    if (pid == 0) {
        /* child */
        print("[Linux ABI] fork child: pid=");
        print_dec(sys1(39, 0));             /* getpid() */
        print(" -> exit 42\n");
        sys1(60, 42);
    } else if (pid > 0) {
        /* parent */
        int status = 0;
        long long w = sys_wait4(pid, &status);
        print("[Linux ABI] fork parent: child=");
        print_dec(pid);
        print(" wait=");
        print_dec(w);
        print(" status=0x");
        print_hex((unsigned long long)(unsigned int)status);
        print("\n");
    } else {
        print("[Linux ABI] fork failed: ");
        print_dec(pid);
        print("\n");
    }

    /* execve test: the child replaces its image with /busybox echo ...
     * xargv lives on the child's stack — the kernel must stage it before
     * overwriting the old image. */
    long long pid2 = sys1(57, 0);
    if (pid2 == 0) {
        static const char a0[] = "busybox";
        static const char a1[] = "echo";
        static const char a2[] = "hello-from-execve";
        long long xargv[4];
        xargv[0] = (long long)a0;
        xargv[1] = (long long)a1;
        xargv[2] = (long long)a2;
        xargv[3] = 0;
        long long er = sys3(59, (long long)"/busybox", (long long)xargv, 0);
        print("[Linux ABI] execve failed: ");   /* reached only on error */
        print_dec(er);
        print("\n");
        sys1(60, 1);
    } else if (pid2 > 0) {
        int st2 = 0;
        long long w2 = sys_wait4(pid2, &st2);
        print("[Linux ABI] execve parent: child=");
        print_dec(pid2);
        print(" wait=");
        print_dec(w2);
        print(" status=0x");
        print_hex((unsigned long long)(unsigned int)st2);
        print("\n");
    } else {
        print("[Linux ABI] fork2 failed: ");
        print_dec(pid2);
        print("\n");
    }

    /* --- signal tests: rt_sigaction -> kill -> handler -> rt_sigreturn --- */
    struct k_sigaction act, old;
    act.handler = (long long)usr1_handler;
    act.flags = SA_RESTORER_FLAG;
    act.restorer = (long long)sig_restore_rt;
    act.mask = 0;
    long long sr = sys3(13, 10, (long long)&act, (long long)&old);
    print("[Linux ABI] sigaction: ");
    print_dec(sr);
    print("\n");

    long long kr = sys3(62, sys1(39, 0), 10, 0);      /* kill(self, SIGUSR1) */
    print("[Linux ABI] SIGUSR1 handler saw: ");
    print_dec(g_sig_seen);          /* 10 -> sigreturn restored the context */
    print(", kill returned: ");
    print_dec(kr);
    print("\n");

    /* blocked-mask roundtrip: BLOCK SIGUSR2(12) -> read back -> unblock */
    long long mset = 0x800, mold = 0, mcur = 0;
    sys3(14, 1, (long long)&mset, (long long)&mold);   /* SIG_BLOCK */
    sys3(14, 1, 0, (long long)&mcur);                  /* query current */
    print("[Linux ABI] sigprocmask: old=0x");
    print_hex((unsigned long long)mold);
    print(" cur=0x");
    print_hex((unsigned long long)mcur);
    print("\n");
    sys3(14, 2, (long long)&mset, 0);                  /* SIG_UNBLOCK again */

    /* default action: SIGTERM kills the child, wait4 sees WIFSIGNALED */
    long long pid3 = sys1(57, 0);
    if (pid3 == 0) {
        sys3(62, sys1(39, 0), 15, 0);                  /* kill(self, TERM) */
        sys1(60, 77);                                  /* not reached */
    } else if (pid3 > 0) {
        int st3 = 0;
        long long w3 = sys_wait4(pid3, &st3);
        print("[Linux ABI] signal death: wait=");
        print_dec(w3);
        print(" status=0x");
        print_hex((unsigned long long)(unsigned int)st3);   /* expect 0xf */
        print("\n");
    }

    /* --- fault paths: #PF without a handler -> wait4 WIFSIGNALED(11) --- */
    long long pid4 = sys1(57, 0);
    if (pid4 == 0) {
        volatile long long *bad = (volatile long long *)0x10;
        *bad = 1;                      /* #PF -> SIGSEGV default -> die */
        sys1(60, 77);                  /* not reached */
    } else if (pid4 > 0) {
        int st4 = 0;
        long long w4 = sys_wait4(pid4, &st4);
        print("[Linux ABI] segv death: wait=");
        print_dec(w4);
        print(" status=0x");
        print_hex((unsigned long long)(unsigned int)st4);   /* expect 0xb */
        print("\n");
    }

    /* --- #PF with a handler: fork inherits the action table --- */
    long long pid5 = sys1(57, 0);
    if (pid5 == 0) {
        struct k_sigaction sa;
        sa.handler = (long long)segv_handler;
        sa.flags = SA_RESTORER_FLAG;
        sa.restorer = (long long)sig_restore_rt;
        sa.mask = 0;
        sys3(13, 11, (long long)&sa, 0);               /* rt_sigaction(SIGSEGV) */
        volatile long long *bad = (volatile long long *)0x20;
        *bad = 42;                     /* #PF -> handler (never returns) */
        sys1(60, 77);                  /* not reached */
    } else if (pid5 > 0) {
        int st5 = 0;
        long long w5 = sys_wait4(pid5, &st5);
        print("[Linux ABI] segv handled: wait=");
        print_dec(w5);
        print(" status=0x");
        print_hex((unsigned long long)(unsigned int)st5);   /* expect 0x2a00 */
        print("\n");
    }

    /* --- futex: WAIT fast path (-EAGAIN) and WAKE with no waiters --- */
    {
        volatile int faddr = 1;
        long long fe = sys3(202, (long long)&faddr, 0, 2); /* WAIT val=2: -EAGAIN */
        long long fw = sys3(202, (long long)&faddr, 1, 1); /* WAKE 1: 0 waiters */
        print("[Linux ABI] futex: eagain=");
        print_dec(fe);          /* expect -11 */
        print(" wake=");
        print_dec(fw);          /* expect 0 */
        print("\n");
    }

    /* --- alarm(1) -> SIGALRM -> pause() returns -EINTR --- */
    struct k_sigaction sa2;
    sa2.handler = (long long)alrm_handler;
    sa2.flags = SA_RESTORER_FLAG;
    sa2.restorer = (long long)sig_restore_rt;
    sa2.mask = 0;
    sys3(13, 14, (long long)&sa2, 0);                  /* rt_sigaction(SIGALRM) */
    long long aret = sys1(37, 1);                      /* alarm(1) -> 0 */
    long long pret = sys1(34, 0);                      /* pause -> -EINTR */
    print("[Linux ABI] alarm: prev=");
    print_dec(aret);
    print(" pause=");
    print_dec(pret);
    print(" fired=");
    print_dec(g_alrm_seen);                            /* expect 1 */
    print("\n");
    sys1(37, 0);                                       /* cancel (nothing left) */

    sys1(60, 0);
}
