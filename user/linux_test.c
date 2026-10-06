/*
 * Standalone Linux x86_64 test binary for the OpenSweet Linux ABI.
 * Raw syscalls used:
 *   1  = write, 39 = getpid, 57 = fork, 60 = exit, 61 = wait4
 *
 * Sequence:
 *   1. hello line (regression marker used by the screenshot tests)
 *   2. fork -> child prints its pid and exits 42,
 *      parent wait4()s and reports pid/wait/status (expect status 0x2a00)
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

void _start(void) {
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

    sys1(60, 0);
}
