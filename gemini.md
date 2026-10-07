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

---

## Текущая задача в процессе (где остановились)
- Подготовка полноценного каталога `/bin` в образе `disk.img` (через `make-ext4.ps1`) или symlink на `busybox`/Debian `echo`, чтобы команды вида `/bin/echo` и утилиты coreutils запускались напрямую из терминала OpenSweet.

---

### Этап 4: Сетевой стек и события (для утилит типа curl, apt, wget)
1. `socket`, `connect`, `bind`, `listen`, `accept`, `sendto`, `recvfrom`
2. `epoll_create1`, `epoll_ctl`, `epoll_wait` (цикл событий glibc)
3. Интеграция с драйвером сетевой карты (E1000)

### Этап 5: Распаковка и установка .deb
1. `dpkg-deb` / `ar` + `tar.xz` (через busybox или утилиту)
2. Извлечение `data.tar.xz` в корень VFS ext4
3. Запуск бинарников из `/usr/bin/`

---

## Архитектурные заметки
- Сегменты `ld.so` загружаются в `0x600000`, бинарник в `0x400000`.
- Зона `mmap` для библиотек начинается с `0x20100000`.
- Терминал запускает процессы через `os_spawn_stdio` с пайпами stdout/stderr и считывает поток в цикле.

---

## Правила работы ассистента
- **Поиск по коду:** использовать ИСКЛЮЧИТЕЛЬНО `findstr` или `Select-String` (с параметром `-Context`). Никакого повторного слепого чтения одних и тех же больших диапазонов файлов через `view_file`.
- **Экономия контекста:** точечные правки, минимизация пустого трепа, проверка каждой фичи через `build.cmd` + `make-ext4.ps1` + `test_linux_abi.py`.
