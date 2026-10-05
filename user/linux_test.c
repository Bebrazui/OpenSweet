/*
 * Minimal standalone Linux x86_64 binary.
 * Uses Linux syscalls directly:
 *   syscall 1 = sys_write(stdout, msg, len)
 *   syscall 60 = sys_exit(0)
 */

static inline long linux_syscall3(long long num, long long arg1, const void *arg2, long long arg3) {
    long long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
        : "rcx", "r11", "memory"
    );
    return (long)ret;
}

static inline void linux_exit(long long code) {
    __asm__ volatile (
        "syscall"
        :
        : "a"(60LL), "D"(code)
        : "rcx", "r11", "memory"
    );
}

static const char msg[] = "[Linux ABI] Hello from native Linux x86_64 ELF!\n";

void _start(void) {
    linux_syscall3(1, 1, msg, sizeof(msg) - 1);
    linux_exit(0);
}
