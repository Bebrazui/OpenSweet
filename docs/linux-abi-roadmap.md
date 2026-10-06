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

## 2. Где мы сейчас: 33 / ~450

Фактически реализовано (`compat_linux.inc:87`, неизвестные → ENOSYS из
`compat_linux.inc:165`):

```
read(0) write(1) open(2) close(3) lseek(8) poll(7) ioctl(16) writev(20)
mmap(9) mprotect(10) munmap(11) brk(12) dup2(33) getpid(39)
clone(56) fork(57) vfork(58) wait4(61) getppid(110)
exit(60) exit_group(231) uname(63) getcwd(79) chdir(80) mkdir(83)
rmdir(84) unlink(87) arch_prctl(158) set_tid_address(218)
getuid(102) getgid(104) geteuid(107) getegid(108)
```

Уже работает end-to-end: детект ABI → spawn → argv/envp/auxv → старт
musl/glibc-кода → mmap-зона/brk/TLS → вывод через пайп в GUI-терминал →
exit-код. Эмпирически подтверждено: `linux_test.elf`, `busybox echo …`,
полный список апплетов `busybox`, цепочка `fork → child exit 42 →
wait4 → status=0x2a00`.

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

### Фаза 1 — стабильность (ЗАКРЫТА, см. коммит после `160ca8b`)

- [x] **errno-маппинг**: `vfs_last_err` (positive errno) + отрицательные
      Linux-errno в `.l_read/.l_write/.l_writev` (-EFAULT/-EBADF/-EPIPE/-EIO),
      `.l_close` → -EBADF; mmap уже возвращал -ENOMEM/-EINVAL. Нативные
      syscall'ы продолжают получать просто -1 (контракт не нарушен).
- [x] **Утечка физ. страниц при munmap**: `vmm_unmap` теперь возвращает
      освобождённую физ.страницу, `.l_munmap` делает `pmm_free` (только для
      страниц из диапазона PMM < 0x10000000 — identity-страницы зоны не трогаем);
      `.l_mmap` при OOM откатывает уже замапленные страницы и фиксирует
      bump-указатель только после успеха.
- [x] **Заглушки-идентичность**: uname/getcwd/uid/gid проверены —
      правдоподобны (uid=0 = root, uname = Linux/6.1.0-opensweet/x86_64);
      неизвестные номера уже дают ENOSYS (`compat_linux.inc`).
- [x] **Слот кода/стека**: геометрия переработана —
      **код/данные 4MB** (PD[2..3], 0x400000..0x7FFFFF), **стек переехал**
      на PD[255] (0x1FE00000..0x1FFFFFFF, вершина 0x1FFFFF00, argv-вектор
      @ 0x1FFFFE00), `USER_MEM_STRIDE` 4MB → 6MB (7 слотов = 42MB, 80..122MB
      — в пределах зарезервированных 128MB), валидатор получил Range C
      [0x1FE00000, 0x20000000). Прежний потолок 2MB (стек стоял у кода,
      brk владеет 16..64MB, канвасы 8..16MB — растись было некуда) снят.
- [x] **vmm_unmap и huge**: проверка bit7 на уровнях PDPT/PD — huge-запись
      не разбирается «как таблица» (была дыра записи по мусорному адресу) и
      не сносит identity-маппинг.
- [x] **Пайп**: блокирующее ожидание места в буфере (`sti → task_sleep(2) → cli`)
      вместо возврата 0 (busybox `full_write` зациклился бы) и честный -EPIPE
      при отсутствии читателей; per-pipe-блокировки не потребовалось —
      на текущем планировщике достаточно глобального cli.

### Фаза 2 — процессы и сигналы → Tier A

- [x] **fork/clone** — `.l_fork` (57/58/56): deep copy кода (4MB) и стека
      (2MB) в новый слот через `task_alloc_user_slot`, копия fd-таблицы
      с инкрементом pipe refcount (зеркало `vfs_dup2`), TCB поле-в-поле
      (HEAP_BRK/ABI/FS_BASE/FXSAVE от родителя; PARENT_ID выставляется до
      копирования — гонка с чужим waitpid исключена; CR3 от alloc),
      дочерний iretq-фрейм строится из syscall-рамки родителя (RAX=0,
      RIP/RSP/регистры те же), возврат = слот+1 (= getpid), нет слота →
      -EAGAIN. clone-флаги и child_stack игнорируются: child получает
      приватную копию VM (корректно для vfork+exec и shell-оболочек,
      CLONE_VM/CLONE_THREAD — Фаза 5/Tier B).
- [x] **wait4/getppid** — `.l_wait4` (pid>1 → слот=pid-1, pid≤0 → любой
      ребёнок, pid=1 → -ECHILD; успех → pid, WNOHANG/ошибки без
      конвертации; status уже POSIX `(code&0xFF)<<8`, ожидает
      `sys_handler_waitpid`), `.l_getppid` = PARENT_ID+1.
      Проверено: `fork → child exit 42 → parent wait4 → status=0x2a00`.
- [ ] **execve из ring3** (сейчас exec есть только через spawn терминала),
      **waitid**, **setsid/getpgrp/setpgid** (job control шелла).
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
- User-адреса: код/данные 4–8MB (PD[2..3], 4MB), канвасы 8–16MB,
  brk-heap 16–64MB, стек 0x1FE00000–0x1FFFFFFF (PD[255], вершина 0x1FFFFF00),
  mmap-зона 512MB–1GB; валидация — `sys_validate_user_ptr`
  (Range A/B/C).
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
