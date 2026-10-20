# Матрица покрытия Linux syscall'ов (x86_64)

Источник истины — dispatch в `kernel/compat_linux.inc` (`linux_syscall_dispatch`).

Встроенный strace: каждый вызов печатает в serial `[Linux ABI] Call: #N`.
Неизвестный номер логируется как `[Linux ABI] Unhandled syscall: #N` и
возвращает `-ENOSYS`.

Легенда:

- **работает** — полноценная реализация;
- **заглушка** — вызов принимается и возвращает правдоподобный ответ, но
  реальной семантики нет (нужно для старта glibc/coreutils);
- **нет** — даже константы нет, возвращается `-ENOSYS`.

Всего диспетчеризуется **151** номер.

## Файлы, каталоги, права

| # | Имя | Статус | Примечание |
|---|-----|--------|------------|
| 2 | open | работает | O_RDONLY/WRONLY/RDWR/APPEND/CREAT/TRUNC/EXCL |
| 257 | openat | работает | современный coreutils |
| 0 | read | работает | файл/пайп, блокирующий, EINTR |
| 17 | pread64 | работает | |
| 19 | readv | работает | |
| 1 | write | работает | файл/пайп, SIGPIPE, EINTR |
| 18 | pwrite64 | работает | |
| 20 | writev | работает | |
| 3 | close | работает | -EBADF |
| 8 | lseek | работает | SEEK_SET/CUR/END |
| 4 | stat | работает | struct stat |
| 5 | fstat | работает | |
| 6 | lstat | работает | symlinks |
| 262 | newfstatat | работает | |
| 332 | statx | работает | |
| 21 | access | работает | |
| 269 | faccessat | работает | |
| 79 | getcwd | работает | |
| 80 | chdir | работает | |
| 83 | mkdir | работает | |
| 258 | mkdirat | работает | |
| 84 | rmdir | работает | |
| 87 | unlink | работает | |
| 263 | unlinkat | работает | |
| 82 | rename | работает | |
| 264 | renameat | работает | |
| 316 | renameat2 | работает | флаги игнорируются |
| 86 | link | работает | |
| 265 | linkat | работает | |
| 88 | symlink | работает | |
| 266 | symlinkat | работает | |
| 89 | readlink | работает | |
| 267 | readlinkat | работает | |
| 90/91/268 | chmod/fchmod/fchmodat | частично | режим сохраняется в inode |
| 92/93/260 | chown/fchown/fchownat | заглушка | → 0 (один root-пользователь) |
| 95 | umask | работает | |
| 74 | fsync | работает | |
| 75 | fdatasync | работает | |
| 76 | truncate | работает | |
| 77 | ftruncate | работает | |
| 280 | utimensat | работает | |
| 217 | getdents64 | работает | |
| 137 | statfs | заглушка | синтетические значения ext4 |
| 138 | fstatfs | заглушка | то же |
| 161 | chroot | заглушка | → 0 |

## Процессы, потоки, сигналы

| # | Имя | Статус | Примечание |
|---|-----|--------|------------|
| 57 | fork | работает | deep copy 4MB code + 2MB stack + heap |
| 56 | clone | заглушка | флаги игнорируются → как fork |
| 58 | vfork | заглушка | как fork |
| 59 | execve | работает | стейджинг argv в ядре, re-exec в слоте |
| 60 | exit | работает | |
| 231 | exit_group | работает | → exit |
| 61 | wait4 | работает | WNOHANG, WIFSIGNALED, EINTR |
| 39 | getpid | работает | слот+1 |
| 110 | getppid | работает | |
| 186 | gettid | заглушка | → pid |
| 218 | set_tid_address | работает | |
| 234 | tgkill | работает | tgid игнорируется |
| 62 | kill | работает | pid>0, 0 = проверка существования |
| 13 | rt_sigaction | работает | SA_RESTORER/SA_NODEFER, oldact |
| 14 | rt_sigprocmask | работает | BLOCK/UNBLOCK/SETMASK |
| 15 | rt_sigreturn | работает | восстановление mcontext + fxstate |
| 34 | pause | работает | -EINTR по сигналу |
| 37 | alarm | работает | SIGALRM из sched_tick |
| 109 | setpgid | заглушка | → 0 |
| 111 | getpgrp | заглушка | → pid |
| 112 | setsid | заглушка | → pid |
| 121 | getpgid | заглушка | → pid |
| 124 | getsid | заглушка | → pid |
| 273 | set_robust_list | заглушка | → 0 |
| 274 | get_robust_list | заглушка | отдаёт нули |
| 334 | rseq | заглушка | → -ENOSYS (glibc отключает rseq) |
| 102/104/107/108 | getuid/getgid/geteuid/getegid | заглушка | → 0 |
| 105/106 | setuid/setgid | заглушка | → 0 |
| 113/114/117/119 | setreuid/setregid/setresuid/setresgid | заглушка | → 0 |
| 115/116 | getgroups/setgroups | заглушка | |
| 118/120 | getresuid/getresgid | заглушка | → 0 |
| 302 | prlimit64 | заглушка | RLIMIT_NOFILE=1024, RLIMIT_STACK=8MB, прочее = infinity |
| 97 | getrlimit | заглушка | то же |
| 160 | setrlimit | заглушка | → 0 |

## Память

| # | Имя | Статус | Примечание |
|---|-----|--------|------------|
| 9 | mmap | работает | anon/private, зона 512MB–1GB |
| 10 | mprotect | работает | |
| 11 | munmap | работает | возврат физ.страниц PMM |
| 12 | brk | работает | приватный heap 16–48MB на задачу |
| 28 | madvise | заглушка | → 0 |
| 29/30/31/67 | shmget/shmat/shmctl/shmdt | работает | X11 SHM |

## Ввод-вывод, ожидание, таймеры

| # | Имя | Статус | Примечание |
|---|-----|--------|------------|
| 7 | poll | работает | пайпы, таймаут |
| 23 | select | работает | |
| 270 | pselect6 | работает | |
| 271 | ppoll | работает | |
| 213 | epoll_create | работает | |
| 291 | epoll_create1 | работает | |
| 233 | epoll_ctl | работает | |
| 232 | epoll_wait | работает | |
| 16 | ioctl | частично | TCGETS/TCSETS, TIOCGWINSZ |
| 22 | pipe | работает | |
| 293 | pipe2 | работает | O_CLOEXEC/O_NONBLOCK/O_DIRECT |
| 32 | dup | работает | |
| 33 | dup2 | работает | |
| 292 | dup3 | работает | |
| 72 | fcntl | частично | F_DUPFD/GETFD/SETFD/GETFL/SETFL/GETLK/SETLK |
| 73 | flock | работает | |
| 40 | sendfile | работает | |
| 96 | gettimeofday | работает | |
| 228 | clock_gettime | работает | |
| 35 | nanosleep | работает | |
| 221 | fadvise64 | заглушка | → 0 |
| 187 | readahead | заглушка | → 0 |

## Сеть

| # | Имя | Статус | Примечание |
|---|-----|--------|------------|
| 41–55 | socket…getsockopt | работает | AF_INET/AF_UNIX; AF_INET через свой стек |
| 288 | accept4 | работает | |

## Синхронизация и прочее

| # | Имя | Статус | Примечание |
|---|-----|--------|------------|
| 202 | futex | работает | WAIT/WAKE/REQUEUE/CMP_REQUEUE |
| 63 | uname | работает | Linux/6.1.0-opensweet/x86_64 |
| 158 | arch_prctl | работает | SET_FS/GET_FS |
| 99 | sysinfo | работает | |
| 98 | getrusage | работает | |
| 100 | times | работает | |
| 135 | personality | заглушка | → 0 |
| 203 | sched_setaffinity | заглушка | → 0 |
| 204 | sched_getaffinity | заглушка | маска = 1 CPU |
| 309 | getcpu | заглушка | → 0 |
| 318 | getrandom | заглушка | xorshift64* от TSC; **не** криптостойкий |
| 324 | membarrier | заглушка | → 0 |

## Чего нет и что это блокирует

- **inotify** (253/254/255) — `tail -f`, файловые менеджеры, некоторые сборочные
  системы.
- **eventfd** (284), **timerfd** (282/283), **signalfd** (282) — современные
  event loop'ы; `epoll` уже есть, но без них часть кода падает на старте.
- **restart_syscall** (219), **set_robust_list** уже есть, но robust-futex не
  реальный.
- **/proc** (`/proc/self/{exe,cmdline,status,maps}`, `/proc/cpuinfo`) — `ps`,
  `python`, `gdb`, `htop`. Сейчас это главный блокер после syscall-покрытия.
- **/dev**: `null`/`zero`/`urandom`/`tty` обрабатываются как спец-типы fd, но
  настоящего `devtmpfs` нет; `open("/dev/random")` вернёт ENOENT.
- **futex time64 и PI-futex** — многопоточные программы на pthread работают
  частично (fork deep copy, без COW).
- **statfs/df/du** — синтетика, реальных счётчиков томов нет.

## Тестовые бинарники Debian для проверки покрытия

| Бинарник | Что проверяет |
|----------|---------------|
| `linux_test.elf` (свой, freestanding) | fork/execve/wait4/сигналы/fault/alarm |
| `busybox` (статический) | апплеты, список, pipe, exec |
| `bash` (динамический, Debian) | ld.so, mmap, futex, rt_sig*, termios |
| `ls -la`, `cat`, `grep`, `wc` | stat/getdents/read |
| `python3` (динамический) | ld.so, futex, mmap, `/proc` |
| `curl --version` | динамическая линковка, сокеты |
| `ico` + libX11 | X11 wire protocol, SHM |
