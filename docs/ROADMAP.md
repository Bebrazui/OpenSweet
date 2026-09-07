# Дорожная карта Opensweet OS

## Фаза 0 — x86_64 фундамент (ГОТОВО)
- [x] MBR -> protected mode -> long mode (FASM)
- [x] Identity map 1GB, 2MB pages
- [x] VGA text + COM1 serial
- [x] PS/2 клавиатура (polled), echo-shell

## Фаза 1 — ядро x86_64
- [x] IDT + обработчики исключений 0-31 (halt-loop + печать вектора/RIP), тест-команды `exc`/`div`
- [x] Прерывания вместо polling: remap 8259 PIC, PIT ~1kHz тики (`ticks`), IRQ1 клавиатура -> ring buffer, hlt-idle
- [x] Local APIC: включение, маскировка LVT, таймер periodic 1кГц; PIC через LINT0=ExtINT; dual EOI; полное сохранение контекста в IRQ
- [x] E820 memory map (boot) -> PMM bitmap 256MB (`mem`: free/alloc/free/alloc-EQ)
- [x] VMM: vmm_map/vmm_unmap 4KB с аллокацией таблиц по требованию (`map`: запись/чтение через новую трансляцию)
- [x] Higher-half kernel: org 0xFFFF800000010000, PML4[256] -> 2MB @ phys 0, трамплин из identity; R15=база образа, R14=0 для low-refs
- [x] Вытесняющая многозадачность (Preemptive Multitasking Scheduler): квантование APIC, TCB, переключение контекста, фоновые демоны (clock, sysmon), команды `tasks`/`ps`
- [x] Динамический аллокатор памяти ядра (Kernel Heap): `kmalloc`, `kzalloc`, `kfree`, `krealloc`, boundary-tag splitting & coalescing, динамическое расширение через PMM, команды `heap`/`heaptest`
- Загрузка через UEFI (PE32+ образ) — FASM умеет PE64
- Кольцо защиты Ring 3 (User Space) и системные вызовы `syscall`/`sysret`

## Фаза 2 — подсистемы
- [x] Блочный слой: ATA PIO драйвер (LBA28, primary master+slave, polling, таймауты), команда `ata` (identify + дамп сектора), make-disk.cmd тестовый образ
- [x] ext4: read-only VFS (Superblock, Block Groups, Inode Table, Extents tree, Directory indexing), команды `ls`, `cd`, `pwd`, `cat`, `stat`
- [x] Full HD Графика (1920x1080 @ 32bpp VBE LFB) и 2D композитор: альфа-блендинг, скругленные углы, тени, субпиксельный сглаженный векторный курсор мыши
- [x] Оконный менеджер (Window Manager): Z-order, фокус, плавный драг окон, док-панель приложений, верхнее меню
- [x] Встроенный декодер PNG на чистом ассемблере x86_64: парсинг чанков IHDR/IDAT/IEND, zlib/deflate декомпрессия (Huffman, LZ77), фильтрация сканлайнов, обои рабочего стола (`wallpaper`)
- littlefs: портирование эталонного кода (чистый C99) — нужен компилятор в ядре или ручной транслят; вариант: собрать через GCC и слинковать
- LVGL: framebuffer-драйвер (VBE/VESA или UEFI GOP), lv_port_disp
- Ввод: USB HID (xHCI) для мыши; BT — BLE HOG через контроллер (ESP32 как сопроцессор по UART/SPI — самый быстрый путь)

## Фаза 3 — другие архитектуры
| Арх    | Инструменты                          | Плата/QEMU            |
|--------|--------------------------------------|-----------------------|
| ARM64  | LLVM (`clang --target=aarch64-none-elf`) или aarch64-none-elf-gcc | QEMU virt, PL011 |
| RISC-V | riscv64-unknown-elf-gcc / clang      | QEMU virt + SBI       |
| Xtensa | crosstool-NG `xtensa-esp32s3-elf-`   | ESP32-S3 (реальное железо) |

FASM нативно не собирает эти архитектуры. Варианты:
1. LLVM (один clang покрывает arm64+riscv64, Xtensa — из fork Espressif)
2. fasmg + macro-пакеты (экспериментально)

## Фаза 4 — WiFi/BT HID
- Путь А: сопроцессор ESP32 (WiFi+BT на борту, прошивка на NimBLE), связь с ядром по UART-H4/SPI
- Путь Б: полноценный стек (Zephyr-подход) — очень дорого
