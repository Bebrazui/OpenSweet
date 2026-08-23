# Дорожная карта Opensweet OS

## Фаза 0 — x86_64 фундамент (ГОТОВО)
- [x] MBR -> protected mode -> long mode (FASM)
- [x] Identity map 1GB, 2MB pages
- [x] VGA text + COM1 serial
- [x] PS/2 клавиатура (polled), echo-shell

## Фаза 1 — ядро x86_64
- [x] IDT + обработчики исключений 0-31 (halt-loop), тест-команды `exc`/`div` в shell
- APIC таймер, прерывания вместо polling
- PMM (bitmap), VMM (higher-half, 4KB+2MB pages)
- Загрузка через UEFI (PE32+ образ) — FASM умеет PE64
- Многозадачность: context switch, кольца 0/3

## Фаза 2 — подсистемы
- ext4: read-only сначала (superblock, GDT, extents, inode cache); запись потом
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
