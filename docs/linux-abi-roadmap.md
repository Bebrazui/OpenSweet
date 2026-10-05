# Roadmap: полный Linux ABI OpenSweet

Цель: **запуск произвольных программ того же Debian amd64** — статических и
динамических, из штатных пакетов дистрибутива (coreutils, bash, apt-утилиты,
python и т.п.), без пересборки под OpenSweet.

Язык: кольцо 0 (FASM, монолитное ядро) + кольцо 3 (C). Linux-вызовы —
`kernel/compat_linux.inc`, детект ABI и загрузка — `kernel/elf.inc`,
стрек/адресация — `kernel/sched.inc`, файлы/пайпы — `kernel/vfs.inc`.

---

## 1. Что значит «произвольные программы Debian»

Полный Linux ABI для x86_64 — это таблица из **~450 syscall'ов** плюс
семантика (процессы, нити, сигналы, файловая система, сокеты). Один
hobby-ядерный цикл это не закрывает, поэтому выделяю три уровня (tier),
каждый — измеримая веха:

| Tier | Покрытие | Что запускается | Ключевые подсистемы |
|------|----------|-----------------|---------------------|
| **A. Статические** | ~100–120 вызовов | Любая **статическая** программа без сетей и нитей: coreutils, `bash`/`dash` static, `vi`, `tar`, `grep`… | FS (stat/getdents/open-флаги), сигналы, fork/wait/exec, kill |
| **B. Динамические + нити** | ~230 | Готовые **glibc/musl-динамические** .deb-бинарники, многопоточные: `python3`, `curl`, `rsync`… | ld.so (PT_INTERP, релокации), clone/futex/robust-list, epoll/eventfd/timerfd, /proc, /dev |
| **C. Сети и полнота** | ~450 | «Почти всё» из Debian без ядра/драйверов: сокеты, DNS, `apt`, `wget`, демоны | socket-стек, inotify, ppoll/pselect, signalfd, prlimit, namespaces(заглушки) |

Честные оговорки уровня C: без ptrace-трейсеров, seccomp, cgroups,
io_uring, raw-сокетов/netlink и мультипроцессорного SMP-параллелизма
(это уже не «ABI», а разметка Linux). Критерий — бинарники из официального
Debian amd64 запускаются и выполняют свою работу, а не «проходят тесты».

## 2. Где мы сейчас: 28 / ~450

Фактически реализовано (`compat_linux.inc:82`, неизвестные → ENOSYS из
`compat_linux.inc:150`):

```
read(0) write(1) open(2) close(3) lseek(8) poll(7) ioctl(16) writev(20)
mmap(9) mprotect(10) munmap(11) brk(12) dup2(33) getpid(39)
exit(60) exit_group(231) uname(63) getcwd(79) chdir(80) mkdir(83)
rmdir(84) unlink(87) arch_prctl(158) set_tid_address(218)
getuid(102) getgid(104) geteuid(107) getegid(108)
```

Уже работает end-to-end: детект ABI → spawn → argv/envp/auxv → старт
musl/glibc-кода → mmap-зона/brk/TLS → вывод через пайп в GUI-терминал →
exit-код. Эмпирически подтверждено: `linux_test.elf`, `busybox echo …`,
полный список апплетов `busybox`.

**Метод развития — покрытие таблицы, а не отдельные «фичи»:**
- `docs/linux-syscalls.md` — матрица: номер → имя → статус
  (нет/заглушка/работает) → тест-бинарник из Debian;
- встроенный strace: лог `[Linux ABI] Call: #N` в serial — по нему
  разбираем, чего не хватает конкретному бинарнику (corpus-driven);
- метрика прогресса = % покрытия матрицы + растущий список запускаемых
  бинарников Debian (см. §6 критерии).

---

## 3. Статус: закрыто (этап стабилизации)

- [x] **Детект ABI** — `elf_get_basename` портил указатель (lodsb затирал AL);
      исправлено (кандидат в rcx). Логи `[ELF] ABI detect -> N`.
- [x] **Вывод через пайп** — два бага:
      1. `vfs_redirect_child_fd`: после `rep movsd` rdi сдвинут на +48 →
         проверка PIPE шла по чужой записи → `writers++` пропускался.
      2. `vfs_read_fd`: пустой пайп при живых writer'ах возвращал 0 (EOF)
         вместо блокировки → терминал закрывал чтение → запись ребенка -1.
      Исправлено: `push rdi/pop rdi` + блокирующий wait-цикл
      (`sti`/`task_sleep(2)`/`cli`, баланс флагов сохранён).
- [x] **argv/envp/auxv** — копирование строк аргументов читало с физ. 0x80
      (rsi затирался длиной валидации) → мусор → «applet not found».
      Исправлено. Вектор @ 0x7FFE00, строки @ 0x7FF800, RSP = 0x7FFE00.
- [x] **mmap-зона [0x20000000, 0x40000000)** — `vmm_map` не умел 2MB huge
      (identity-PD ниже 1GB целиком из них): huge-запись бралась за указатель
      таблицы → PTE писался в никуда, VA читала 0xFF → musl-malloc видел мусор
      → `hlt` → #GP. Исправлено: сплит 2MB → 4KB (`.split_2m`), User-бит
      ставится только на реальные таблицы (закрыта и дыра: ring3 больше не
      получает R/W к identity-физпамяти 512MB+).
- [x] **`sys_validate_user_ptr`** — зона расширена:
      `[0x400000, 0x04000000) ∪ [0x20000000, 0x40000000)` (буферы musl-heap
      в mmap-зоне отвергались → write=-1 → «echo: write error»).
- [x] **Тесты зелёные**: `linux_test.elf` (Hello, exit 0),
      `busybox echo hello-from-busybox` (вывод + exit 0), `busybox`
      (полный список апплетов через пайп), `test_combo.py` +
      `test_linux_abi.py` генерируют скриншоты.

---

## 4. Фазы

### Фаза 1 — добить стабильность (ближайшие шаги)

- [ ] **errno-маппинг**: возвращать канонические отрицательные Linux-errno
      (-EBADF, -EPIPE, -ENOMEM…), а не generic -1 — сейчас musl трактует -1
      как errno=1 (EPERM) и маскирует реальные причины.
- [ ] **Утечка физ. страниц при munmap**: `.l_munmap` чистит PTE, но не
      делает `pmm_free` → долгие циклы mmap/munmap иссякнут.
- [ ] **Заглушки-идентичность**: `l_zero_id`/`uname`/`getcwd` — правдоподобные
      значения вместо нулей; ENOSYS уже возвращает диспетчер
      (`compat_linux.inc:150`) — оставить и покрыть все частичные пути.
- [ ] **Слот код/стек > 2MB**: PD-слот кода 2MB (0x400000–0x5FFFFF), busybox
      уже ~1.1MB — упрётся в потолок; нужен 4MB-слот/переезд.
- [ ] **vmm_unmap и huge**: добавить проверку bit7 (как в `vmm_map`).
- [ ] **Пайп**: per-pipe блокировка вместо глобального cli; кольцевой буфер
      не должен терять байты при прерывании.

### Фаза 2 — процессы и сигналы → Tier A

- [ ] **fork/clone** — клонирование таблицы страниц (сначала deep copy,
      потом COW), fd-таблицы, TCB. Основа для `sh -c`, конвейеров, всех
      оболочек.
- [ ] **execve из ring3** (сейчас exec есть только через spawn терминала),
      **wait4/waitid** с корректными WEXITSTATUS/WTERMSIG, **getppid/setsid/
      getpgrp/setpgid** (job control шелла).
- [ ] **Сигналы**: rt_sigaction/rt_sigprocmask/**sigreturn**, kill/tgkill,
      минимальный набор SIGINT/SIGTERM/SIGCHLD/SIGPIPE/SIGSEGV/SIGALRM;
      Ctrl+C терминала → SIGINT процессу; запись в пайп без readers → SIGPIPE
      (-EPIPE); #PF в ring3 → SIGSEGV, а не «crashed. Terminating».
- [ ] **Таймеры**: clock_gettime/gettimeofday/nanosleep(уже)/setitimer/
      alarm → SIGALRM.

### Фаза 3 — файловая система → Tier A (продолжение)

- [ ] open/openat c O_CREAT/O_TRUNC/O_APPEND/O_EXCL/O_NONBLOCK/O_DIRECTORY,
      umask, faccessat, fcntl (F_DUPFD/GETFL/SETFL/CLOEXEC), pipe2, dup3.
- [ ] **stat-семейство**: stat/lstat/fstat/newfstatat/fstatfs — точные
      структуры (busybox `ls -l`, `cp`, `tar`, `df`).
- [ ] **getdents64** — каталоги (`ls`, globbing в sh, `find`).
- [ ] symlink/readlink/unlinkat/rename/link/truncate/ftruncate/fsync/
      pread64/pwrite64/chown/fchmod/utimensat.
- [ ] **/proc и /dev**: /proc/self/{exe,cmdline,status,maps}, /dev/null,
      /dev/zero, /dev/urandom (+getrandom(318)), /dev/tty — чтобы `ps`,
      `top`, `test -e`, рандом работали.
- [ ] statfs (busybox `df`), sysinfo.

### Фаза 4 — нити, epoll, динамический линкер → Tier B

- [ ] **clone с CLONE_VM/CLONE_FILES/CLONE_THREAD** + **futex** (FUTEX_WAIT/
      WAKE, PI не нужен), set_robust_list/get_robust_list, gettid, rseq-заглушка —
      любая glibc-программа зовёт их при старте, без них даже однопоточный
      бинарник может упасть.
- [ ] **epoll** (epoll_create1/ctl/wait), eventfd, timerfd, signalfd,
      inotify, ppoll/pselect6 — цикл событий glibc/libuv/Python.
- [ ] mmap-семантика: MAP_SHARED/ANON/ FIXED-корректность, madvise, mremap,
      msync, posix_fadvise.
- [ ] **Динамический линкер**: свой ld.so либо поддержка штатного
      (PT_INTERP, DT_NEEDED, R_X86_64_RELATIVE/GLOB_DAT/JUMP_SLOT/GLOB_DAT
      lazy binding), поиск библиотек по /lib/x86_64-linux-gnu.
- [ ] **Дебиановская упаковка**: распаковка .deb (ar+tar.gz — busybox уже
      умеет на месте, python на хосте кладёт файлы в ext4 через
      make-ext4.ps1), симлинги busybox как /bin/sh и applet-ссылки.
- [ ] locale/iconv/strftime-минимум (UTF-8), setrlimit/prlimit64,
      sched_getaffinity (glibc их спрашивает).

### Фаза 5 — сокеты и полнота → Tier C

- [ ] socket/bind/connect/accept/listen, send/recv/sendto/recvfrom,
      sendmsg/recvmsg (ancillary-данные минимально), getsockopt/setsockopt
      (SOL_SOCKET: SO_REUSEADDR…, TCP_NODELAY), shutdown, dup-сокетов.
- [ ] select, getpeername/getsockname, fcntl F_SETLK (заглушки).
- [ ] DNS: чистая userspace-реализация (getaddrinfo поверх UDP/53) —
      отдельной библиотекой, ядру сеть не нужна.
- [ ] inotify/epoll — уже из фазы 4; prlimit, uname-детали, getrusage,
      times, sysconf(_SC_*) — чтобы Python/bash не падали на опросах.
- [ ] Симуляция отсутствующего: unshare/namespaces → EPERM-заглушки,
      seccomp → всегда allow, ptrace → EPERM (честно, documented).

---

## 5. Архитектурные ограничения, о которых надо помнить

- Один общий CR3 (identity + слоты + high-half); per-task таблицы копируются
  при spawn (`sched.inc`), не при syscall. Это упрощает fork-семантику
  (берём одну таблицу) и осложняет COW.
- User-адреса: код/данные 4–12MB (слоты), стек ~8MB, mmap-зона 512MB–1GB;
  валидация — `sys_validate_user_ptr`.
- Ring3 не получает identity-физпамять: U-бит ставится только на реальные
  таблицы страниц (huge-страницы сплитятся, `vmm_map: .split_2m`).
- Пайпы: глобальные readers/writers + кольцевой буфер под cli; read
  блокирует через `task_sleep`.
- vDSO нет — все вызовы прямые syscall'ы (это нормально для ABI).
- Нет MMU-COW на первом этапе fork: deep copy до оптимизации.

## 6. Критерии готовности

1. **Tier A**: матрица `docs/linux-syscalls.md` ≥ 100 позиций «работает»;
   из Debian статических запускаются `dash`, coreutils-набор (`ls -la`,
   `cp`, `tar tf`, `grep`, `wc`), `vi`; `sh -c 'echo hi | cat'` (fork+pipe);
   Ctrl+C убивает процесс (SIGINT); errno осмыслены.
2. **Tier B**: `python3 -c 'print(1)'` и `curl --version` из динамических
   .deb стартуют (ld.so + TLS + futex); многопоточный бинарник не падает;
   долгий цикл mmap/munmap не течёт.
3. **Tier C**: `apt-get --version`, `wget` к локальному ресурсу, `nc`;
   покрытие матрицы ≥ 450/450 номеров x86_64 с задокументированными
   заглушками.

Прогресс фиксируется двумя числами: **% матрицы** и **список реально
запущенных бинарников Debian** (лежит в этом же документе, раздел §2).
