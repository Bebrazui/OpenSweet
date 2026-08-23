# Opensweet OS

Мультиархитектурная ОС: x86_64, ARM64, RISC-V, Tensilica Xtensa.
Ядро на ассемблере (FASM для x86_64), файловые системы ext4 + littlefs, GUI на LVGL, ввод по WiFi/BT (мышь/клавиатура).

## Сборка и запуск (x86_64, уже работает)

    build.cmd
    run.cmd        (QEMU + лог COM1 в build\serial.log)
    log.cmd        (показать последний serial-лог)

Автотест клавиатуры headless (QEMU monitor sendkey):

    make-disk.cmd             (тестовый диск build\disk.img)
    test.cmd                  (набирает div ret)
    test.cmd t,i,c,k,s,ret    (свои клавиши через запятые)

Serial-лог ядра пишется на COM1 (`-serial file:log` для headless-теста).

Что умеет ядро 0.0.2: MBR -> protected mode -> long mode, higher-half
(ядро на 0xFFFF800000010000), identity map 1GB + [3GB,4GB) для MMIO,
VGA text + COM1 serial, IDT с обработкой исключений, Local APIC timer 1кГц,
прерывистая PS/2 клавиатура, PMM (e820 + bitmap 256MB), VMM с аллокацией
таблиц по требованию. Shell-команды: `exc`, `div`, `ticks`, `mem`, `map`.

## Структура

    arch/x86_64   загрузчик + ядро (FASM)
    arch/arm64    план: QEMU virt, PL011 UART, Linux-style Image header
    arch/riscv64  план: QEMU virt + SBI, NS16550 UART
    arch/xtensa   план: ESP32, crosstool-NG toolchain, esptool
    fs/ext4       read-only драйвер ext4 (extents, CRC32C)
    fs/littlefs   портирование littlefs (MIT)
    gui/lvgl      портирование LVGL, framebuffer-драйвер
    drivers/input BT HID (BLE HOG) + WiFi стек

Подробный план: docs/ROADMAP.md
