# OpenSweet Linux ABI — Roadmap to .deb

## Текущее состояние (2026-10-07)

### Последний статус: ЭТАП 1 И ЭТАП 2 ПОЛНОСТЬЮ ЗАВЕРШЕНЫ И РАБОТАЮТ!
1. Динамически скомпилированный ELF из Debian (`debian_hello`), использующий официальные `ld-linux-x86-64.so.2` и `libc.so.6`, успешно:
   - Загружается и линкуется в Ring 3.
   - Проходит все bootstrap-ассерты glibc и `_dl_check_map_versions`.
   - Отрабатывает `_dl_init` и передает управление в `main()`.
   - Выводит строку через POSIX stdout pipe:
     `[Debian] Hello from dynamic glibc ELF!`
   - Корректно завершается через `exit_group(0)`.
   - Терминал считывает весь вывод из пайпа и плавно отображает результат на рабочем столе GUI.
2. Вся линейка системных вызовов runtime glibc из Этапа 2 полностью реализована и протестирована.

---

## План до полноценного запуска .deb пакетов

### Этап 1: Динамическая линковка базового Debian ELF [ВЫПОЛНЕНО 100%]
- [x] Загрузка `ld-linux-x86-64.so.2` в адресное пространство (0x600000)
- [x] Формирование вектора `auxv` (AT_PHDR, AT_BASE, AT_ENTRY, AT_PAGESZ, AT_CLKTCK, AT_RANDOM, AT_SECURE)
- [x] Прохождение bootstrap-ассертов `ld.so`
- [x] Резолв путей библиотек (`openat` + fallback к basename в корне `/`)
- [x] `mmap` для библиотек: поддержка отображения файлов (`fd != -1`), вычитка секций через `ext4_read_file_chunk_inode`
- [x] Корректная адресация сегментов `MAP_FIXED` в `mmap` для `libc.so.6`
- [x] Исправление сбалансированности стека и параметров в `.l_mmap`
- [x] Корректная маршрутизация дескрипторов `stdout`/`pipe` в терминале и VFS
- [x] Успешное выполнение `main()` в `debian_hello`, вывод строки и чистый `exit_group(0)`

### Этап 2: Расширение системных вызовов для glibc runtime [ВЫПОЛНЕНО 100%]
- [x] **Файловый ввод-вывод:**
  - [x] `readv` (векторное чтение из дескрипторов)
  - [x] `access` / `faccessat` (проверка прав и существования файлов)
  - [x] `ioctl` (TCGETS, TIOCGWINSZ с геометрией 80x25, фиктивный успех TCSETS)
- [x] **Память и адресация:**
  - [x] `madvise` (заглушка 0)
  - [x] `arch_prctl` (ARCH_SET_FS и ARCH_GET_FS)
- [x] **Информация о системе и процессах:**
  - [x] `uname` ("Linux", "opensweet", "5.10.0-opensweet")
  - [x] `sysinfo` (структура системной памяти и времени работы)
  - [x] `getuid`, `geteuid`, `getgid`, `getegid` (возврат 0 для root)
  - [x] `getrusage`, `times` (учёт времени процесса и тактов таймера)
- [x] **/proc и псевдо-ФС:**
  - [x] `/proc/self/exe` (через `readlink` / `readlinkat`)
  - [x] `/dev/null`, `/dev/urandom`, `/dev/zero` в VFS

### Этап 3: Потоки и многозадачность (Thread Support) [ВЫПОЛНЕНО 100%]
Многопоточность для Linux ABI и `pthread` полностью реализована и протестирована (`debian_thread`):
- [x] **Системный вызов `clone`**:
  - [x] Поддержка `CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS | CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID`
  - [x] Разделение единого виртуального адресного пространства (PML4 / CR3) между всеми потоками одного процесса
  - [x] Разделение таблицы дескрипторов VFS (`vfs_get_fd_ptr` через `TGID - 1` для тредов)
  - [x] Выделение и инициализация стека Ring 3 для дочернего потока
  - [x] Установка TLS (`FS_BASE`) для дочернего потока при `CLONE_SETTLS`
  - [x] Запись TID в `parent_tid` и `child_tid` при `CLONE_PARENT_SETTID` / `CLONE_CHILD_SETTID`
- [x] **`set_tid_address` и завершение потоков**:
  - [x] Регистрация `clear_child_tid` в `TCB_CLEAR_CHILD_TID`
  - [x] Очистка (`*clear_child_tid = 0`) и вызов `futex_wake_addr` при `task_exit`
  - [x] Сохранение общих файловых дескрипторов при завершении вторичных потоков
  - [x] Системный вызов `exit_group` для синхронного завершения всех потоков группы
- [x] **Полноценный `futex`**:
  - [x] `FUTEX_WAIT`, `FUTEX_WAIT_BITSET`
  - [x] `FUTEX_WAKE`, `FUTEX_WAKE_BITSET`
  - [x] `FUTEX_REQUEUE`, `FUTEX_CMP_REQUEUE`
- [x] **Тестирование в Ring 3**:
  - [x] Динамический бинарник `debian_thread`, использующий glibc `clone` + `futex`, успешно стартует дочерний тред, выполняет параллельный I/O, синхронизируется через `futex` и чисто завершается.

### Промежуточный этап: Расширение файловых операций VFS/ext4 и системных вызовов [ВЫПОЛНЕНО 100%]
Для полноценной работы пакетного менеджера и утилит GNU Coreutils/Debian реализована поддержка создания/модификации ФС:
- [x] **ext4 write / alloc / modify engine**:
  - [x] Корректная разметка групп блоков (2 группы блоков по 8192 блока, резервные суперблоки и GDT в группе 1)
  - [x] Распределение и освобождение блоков (`ext4_alloc_blocks`, `ext4_free_blocks`) и инодов (`ext4_alloc_inode`, `ext4_free_inode`)
  - [x] Создание и удаление записей каталогов (`ext4_dir_add_entry`, `ext4_dir_remove_entry`)
- [x] **Системные вызовы ссылок и файлов (все протестированы в `linux_test.elf`, rc=0)**:
  - [x] `symlink` / `symlinkat` / `readlink` / `readlinkat` (символические ссылки)
  - [x] `link` / `linkat` (жёсткие ссылки)
  - [x] `rename` / `renameat` / `renameat2` (переименование с перезаписью)
  - [x] `mkdirat` / `rmdir` (создание и удаление директорий)
  - [x] `unlink` / `unlinkat` (удаление файлов)
  - [x] `chmod` / `fchmod` / `fchmodat` / `chown` / `fchown` / `fchownat`
  - [x] `truncate` / `ftruncate` / `utimensat` / `flock`
  - [x] Управление правами и группами: `umask`, `setuid`, `setgid`, `setreuid`, `setregid`, `setresuid`, `getresuid`, `setresgid`, `getresgid`, `getgroups`, `setgroups`, `chroot`
  - [x] Сессии и PGID: `setpgid`, `getpgrp`, `setsid`, `getpgid`, `getsid`

### Этап 4: FHS, дерево каталогов и базовый набор утилит Linux [ВЫПОЛНЕНО 100%]
1. **Каталоги и FHS в ext4 (`make-ext4.ps1`)**:
   - Создан каталог `/bin` (Inode 28).
   - В `/bin` развернуты утилиты: `echo`, `sh`, `ls`, `cat`, `tar`, `dpkg`, `busybox`.
   - Созданы корневые симлинки стандарта Linux: `/usr -> .`, `/lib -> .`, `/lib64 -> .`.
2. **Поддержка $PATH в Terminal (`user/terminal.c`)**:
   - Команды ищутся последовательно: `/<cmd>.elf` -> `/<cmd>` -> `/bin/<cmd>` -> `/usr/bin/<cmd>`.
   - Команды `echo`, `cat`, `ls`, `busybox` работают как с префиксом `/bin/`, так и напрямую по имени.
3. **Исправление векторного вывода `writev` (Syscall #20)**:
   - `.l_writev` перенаправлен в `vfs_write_fd`, благодаря чему вывод glibc/coreutils через pipe попадает прямо в окно терминала.
   - Проверена и работает цветная раскраска ANSI для файлов, папок и бинарников (`ls /`).
4. **Официальный пакетный менеджер и архивы**:
   - В образ ext4 добавлен тестовый пакет `test.deb` (Inode 32).

---

### Этап 5: Распаковка и запуск реальных .deb пакетов [ВЫПОЛНЕНО 100%]
1. **Инспекция и распаковка .deb (`/bin/dpkg-deb`)**:
   - `dpkg-deb -c /test.deb` успешно инспектирует ar-архив, запускает декомпрессор через `sys_fork` + `sys_pipe`, на лету читает gzip-поток и выводит листинг файлов пакета в терминал.
   - `dpkg-deb -x /test.deb /` извлекает `data.tar.gz` прямо в корень файловой системы ext4, создавая распакованные файлы (`/hello_from_deb.txt`).
   - Команда `cat /hello_from_deb.txt` мгновенно читает и выводит содержимое распакованного файла.
2. **Исправление системного ядра под fork & paging**:
   - `elf.inc`: строки аргументов (`argv`) и окружения перенесены выше стартового `RSP`, предотвращая их порчу стековыми фреймами libc.
   - `compat_linux.inc`: репликация страниц mmap зоны `[0x20000000, linux_mmap_next)` в дочерний процесс при `sys_fork` копирует данные напрямую из физической страницы родителя в новую физическую страницу потомка.

---

### Сетевая подсистема и сокеты Linux ABI [ВЫПОЛНЕНО 100%]
1. **Сетевой драйвер Realtek RTL8139 (`arch/x86_64/drivers/rtl8139.asm`)**:
   - PCI автосканирование (`10EC:8139`), чтение I/O-порта из BAR0 и IRQ, включение Bus Master.
   - Выделение физически непрерывных буферов через `pmm_alloc_contiguous` (RX Ring 16KB, TX 8KB).
   - Считывание 48-битного MAC-адреса, отправка (`rtl8139_send`) и приём (`rtl8139_poll`).
2. **Сетевой стек ядра (`kernel/net.inc`)**:
   - Статический IP `10.0.2.15` (QEMU user net).
   - Автоматический ответ ARP Reply на широковещательные ARP-запросы шлюза.
   - Автоматический ответ ICMP Echo Reply (Ping) с пересчетом контрольных сумм IP и ICMP.
   - Фоновый сетевой демон `netd` (`task_net_daemon`) в шедулере.
   - Системный вызов `SYS_NET_INFO (36)` и команда `ifconfig` в терминале.
3. **Линейка системных вызовов сокетов и I/O мультиплексирования в Linux ABI (`compat_linux.inc`)**:
   - `socket` (#41), `connect` (#42), `accept` (#43), `accept4` (#288)
   - `sendto` (#44), `recvfrom` (#45), `sendmsg` (#46), `recvmsg` (#47), `shutdown` (#48)
   - `bind` (#49), `listen` (#50), `getsockname` (#51), `getpeername` (#52), `socketpair` (#53)
   - `setsockopt` (#54), `getsockopt` (#55)
   - `select` (#23), `pselect6` (#270), `poll` (#7), `ppoll` (#271)
   - `epoll_create` (#213), `epoll_create1` (#291), `epoll_ctl` (#233), `epoll_wait` (#232)

---

## Текущая задача в процессе (где остановились)
- **Этап 6**: Базовый обработчик протокола X11 (X11 wire protocol сервер в `/tmp/.X11-unix/X0`), транслирующий создание окон, отрисовку битмапов и ввод от мыши/клавиатуры в окна Acrylic R2D Window Manager для запуска GUI Debian.

---

### Этап 6: Полноценные графические приложения Debian (X11 / GTK / Qt)
1. UNIX Domain Sockets (`AF_UNIX` `/tmp/.X11-unix/X0`) [ВЫПОЛНЕНО].
2. Базовый сервер X11 протокола, интегрированный с композитором OpenSweet (Acrylic R2D Window Manager).
3. Разделяемая память MIT-SHM (`shmget` / `shmat`).

---

## Архитектурные заметки
- Сегменты `ld.so` загружаются в `0x600000`, бинарник в `0x400000`.
- Зона `mmap` для библиотек начинается с `0x20100000`.
- Терминал запускает процессы через `os_spawn_stdio` с пайпами stdout/stderr и считывает поток в цикле.

---

## Правила работы ассистента
- **Поиск по коду:** использовать ИСКЛЮЧИТЕЛЬНО `findstr` или `Select-String` (с параметром `-Context`). Никакого повторного слепого чтения одних и тех же больших диапазонов файлов через `view_file`.
- **Экономия контекста:** точечные правки, минимизация пустого трепа, проверка каждой фичи через `build.cmd` + `make-ext4.ps1` + `test_linux_abi.py`.
