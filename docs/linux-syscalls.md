# Матрица покрытия Linux syscall'ов (x86_64)

Источник истины — dispatch в `kernel/compat_linux.inc` (`linux_syscall_dispatch`).
Статусы: **работает** (есть хендлер в dispatch), **заглушка** (константа есть,
номер не диспетчеризуется → `-ENOSYS`), **нет** (даже константы).

Встроенный strace: каждый вызов печатает в serial `[Linux ABI] Call: #N`.

## Работает (46)

| # | Имя | Статус | Примечание |
|---|-----|--------|------------|
| 0 | read | работает | пайп/файл, блокирующий, EINTR |
| 1 | write | работает | пайп/файл, SIGPIPE, EINTR |
| 2 | open | работает | O_RDONLY/WRONLY/RDWR/APPEND/CREAT/TRUNC/EXCL |
| 3 | close | работает | -EBADF |
| 7 | poll | работает | пайпы, таймаут |
| 8 | lseek | работает | SEEK_SET/CUR/END |
| 9 | mmap | работает | anon/private, зона 512MB–1GB |
| 10 | mprotect | работает | |
| 11 | munmap | работает | возврат физ.страниц PMM |
| 12 | brk | работает | heap 16–64MB |
| 13 | rt_sigaction | работает | SA_RESTORER/SA_NODEFER, oldact |
| 14 | rt_sigprocmask | работает | BLOCK/UNBLOCK/SETMASK |
| 15 | rt_sigreturn | работает | восстановление mcontext+fxstate |
| 16 | ioctl | работает | TCGETS и др. |
| 20 | writev | работает | |
| 33 | dup2 | работает | редирект fd, pipe refcount |
| 34 | pause | работает | -EINTR по сигналу |
| 37 | alarm | работает | TCB_ALARM_MS, SIGALRM из sched_tick |
| 39 | getpid | работает | слот+1 |
| 56 | clone | заглушка | флаги игнорируются → как fork (deep copy) |
| 57 | fork | работает | deep copy 4MB+2MB, iretq-фрейм |
| 58 | vfork | заглушка | как fork (VM не общий) |
| 59 | execve | работает | стейджинг argv в ядре, re-exec в слоте |
| 60 | exit | работает | |
| 61 | wait4 | работает | WNOHANG, WIFSIGNALED, EINTR |
| 62 | kill | работает | pid>0, 0 = проверка существования |
| 63 | uname | работает | Linux/6.1.0-opensweet/x86_64 |
| 79 | getcwd | работает | |
| 80 | chdir | работает | |
| 83 | mkdir | работает | |
| 84 | rmdir | работает | |
| 87 | unlink | работает | |
| 102 | getuid | работает | 0 (root) |
| 104 | getgid | работает | 0 |
| 107 | geteuid | работает | 0 |
| 108 | getegid | работает | 0 |
| 109 | setpgid | заглушка | → 0 |
| 110 | getppid | работает | PARENT_ID+1 |
| 111 | getpgrp | заглушка | → pid |
| 112 | setsid | заглушка | → pid |
| 121 | getpgid | заглушка | → pid |
| 124 | getsid | заглушка | → pid |
| 158 | arch_prctl | работает | SET_FS/GET_FS |
| 186 | gettid | заглушка | → pid |
| 218 | set_tid_address | работает | |
| 231 | exit_group | работает | → exit |
| 234 | tgkill | работает | tgid игнорируется |

## Заглушки (константа есть, номер не диспетчеризуется → -ENOSYS)

| # | Имя | Нужен для |
|---|-----|-----------|
| 4 | stat | `ls -l`, `test -e`, coreutils |
| 5 | fstat | то же |
| 6 | lstat | symlinks |
| 21 | access | `test -r/-w/-x` |
| 22 | pipe | shell `|` |
| 32 | dup | fd-жонглирование |
| 35 | nanosleep | `sleep`, busybox |
| 72 | fcntl | F_DUPFD/GETFL/SETFL/CLOEXEC |
| 202 | futex | **glibc-старт любого динамического бинарника** |
| 257 | openat | современный coreutils |
| 258 | mkdirat | то же |
| 263 | unlinkat | то же |

## Ключевые отсутствующие блоки (не отдельные номера, а подсистемы)

- **stat-семейство** (4/5/6 + newfstatat 262) — точные структуры `struct stat`.
- **getdents64** (217) — каталоги (`ls`, globbing в sh).
- **pipe2** (293), **dup3** (292), **fcntl** (72).
- **/proc** (/proc/self/{exe,cmdline,status,maps}), **/dev** (null/zero/urandom/tty).
- **futex** (202) + **set_robust_list** (274) — старт glibc.
- **epoll** (232/233/291), **eventfd** (284), **timerfd** (282/283), **signalfd** (282).
- **socket-стек** (41–56) — Tier C.
- **waitid** (247) — упомянут в roadmap, не реализован.
- **clock_gettime** (228), **gettimeofday** (96), **setitimer** (38), **timer_create** (222).
- **pread64/pwrite64** (17/18), **readv** (19), **fsync** (74), **truncate** (76).
- **symlink/readlink** (88/89), **rename** (82), **link** (86), **chmod** (90), **chown** (92).
- **statfs** (137), **sysinfo** (116), **getrusage** (98), **times** (100).
- **ppoll** (271), **pselect6** (270), **inotify** (253/254/255).
- **prlimit64** (302), **sched_getaffinity** (204), **rseq** (334).
- **madvise** (28), **mremap** (25), **msync** (26), **posix_fadvise** (221).
- **uname-детали**, **personality** (135), **getrandom** (318).

## Тестовые бинарники Debian для проверки покрытия

| Бинарник | Что проверяет |
|----------|---------------|
| `linux_test.elf` (свой, freestanding) | fork/execve/wait4/сигналы/fault/alarm |
| `busybox` (статический) | апплеты, список, pipe, exec |
| `dash` (статический) | shell: fork+pipe+wait |
| `ls -la`, `cat`, `grep`, `wc` | stat/getdents/read |
| `python3` (динамический) | ld.so, futex, mmap, /proc |
| `curl --version` | динамическая линковка, сокеты |
